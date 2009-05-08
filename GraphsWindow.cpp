#include "GraphsWindow.h"
#include <QToolBar>
#include <QLCDNumber>
#include <QLabel>
#include <QGridLayout>
#include <math.h>
#include <QTimer>
#include "Util.h"
#include <QCheckBox>

GraphsWindow::GraphsWindow(const DAQ::Params & p, QWidget *parent)
    : QMainWindow(parent), params(p), downsampleRatio(1.), graphTimeSecs(3.0), tNow(0.), tLast(0.), tAvg(0.), tNum(0.), npts(0), nptsTotal(0)
{    
    setCentralWidget(graphsWidget = new QWidget(this));
    setAttribute(Qt::WA_DeleteOnClose, false);
    statusBar();
    resize(1024,768);
    graphCtls = addToolBar("Graph Controls");
    graphCtls->addWidget(new QLabel("Channel:", graphCtls));
    graphCtls->addWidget(chanLCD = new QLCDNumber(2, graphCtls));
    graphCtls->addSeparator();
    QCheckBox *dsc = new QCheckBox(QString("Downsample graphs to %1 Hz").arg(DOWNSAMPLE_TARGET_HZ), graphCtls);
    graphCtls->addWidget(dsc);
    dsc->setChecked(true);
    downsampleChk(true);
    Connect(dsc, SIGNAL(clicked(bool)), this, SLOT(downsampleChk(bool)));
    graphs.resize(p.nVAIChans);
    QGridLayout *l = new QGridLayout(graphsWidget);
    int nrows = int(sqrtf(p.nVAIChans)), ncols = nrows;
    while (nrows*ncols < (int)graphs.size()) {
        if (nrows > ncols) ++ncols;
        else ++nrows;
    };
    for (int r = 0; r < nrows; ++r) {
        for (int c = 0; c < ncols; ++c) {
            int num = r*ncols+c;
            if (num >= (int)graphs.size()) { r=nrows,c=ncols; break; } // break out of loop
            graphs[num] = new GLGraph(graphsWidget);
            graphs[num]->setAutoUpdate(false);
            l->addWidget(graphs[num], r, c);
        }
    }
    points.resize(graphs.size());

    setGraphTimeSecs(3.0);

    QTimer *t = new QTimer(this);
    Connect(t, SIGNAL(timeout()), this, SLOT(updateGraphs()));
    t->setSingleShot(false);
    t->start(1000/TASK_READ_FREQ_HZ);    
}

GraphsWindow::~GraphsWindow()
{
}

void GraphsWindow::setGraphTimeSecs(double t)
{
    graphTimeSecs = t;
    npts = i64(graphTimeSecs*params.srate/downsampleRatio);
    // the total points buffer per graph is 2x the number of points in the graph or 10 seconds, whichever is larger
    //nptsTotal = MAX(npts * 2, i64(10.*params.srate/downsampleRatio));
    nptsTotal = npts * 2;
    pointsP0.resize(points.size());
    for (int i = 0; i < (int)points.size(); ++i) {
        points[i].clear();
        points[i].reserve(nptsTotal);
        pointsP0[i] = 0;
        graphs[i]->setPoints(0, 0);
    }
}

void GraphsWindow::putScans(std::vector<int16> & data, u64 firstSamp)
{
        const int NGRAPHS (graphs.size());
        const double & DOWNSAMPLE_RATIO(downsampleRatio);
        const int SRATE (params.srate);

        int startpt = int(data.size()) - int(npts*NGRAPHS*DOWNSAMPLE_RATIO);
        i64 sidx = i64(firstSamp + u64(data.size())) - npts*i64(NGRAPHS*DOWNSAMPLE_RATIO);
        if (startpt < 0) {
            //qDebug("Startpt < 0 = %d", startpt);
            sidx += -startpt;
            startpt = 0;
        }
        int npergraph = int(MIN((data.size()/NGRAPHS/DOWNSAMPLE_RATIO), npts));

        // for each graph, remove extra points (we limit each graph to npts)
        for (int i = 0; i < NGRAPHS; ++i) {
            if (points[i].size() + npergraph > nptsTotal) {
                int n2erase = /*(points[i].size() + npergraph) - npts*/ int(points[i].size())-npts;
                if (n2erase > (int)points[i].size()) n2erase = points[i].size();
                points[i].erase(points[i].begin(), points[i].begin() + n2erase);
                pointsP0[i] = 0;
            }
            if (points[i].capacity() < nptsTotal) points[i].reserve(nptsTotal);
        }
        double t = double(double(sidx) / NGRAPHS) / double(SRATE);
        const double deltaT =  1.0/SRATE * double(DOWNSAMPLE_RATIO);
        // now, push new points to back of each graph, downsampling if need be
        for (int i = startpt; i < (int)data.size(); ++i) {
            int idx = (sidx)%NGRAPHS;
            std::vector<Vec2> & pts (points[idx]);
            unsigned oldsize = pts.size();
            pts.resize(oldsize+1);
            
            Vec2 & v (pts[oldsize]);
            v.x = t;
            v.y = data[i] / 32768.0; // hardcoded range of data
            if (!(++sidx%NGRAPHS)) {                
                t += deltaT;
                i = int((i-NGRAPHS) + DOWNSAMPLE_RATIO*NGRAPHS);
                if ((i+1)%NGRAPHS) i -= (i+1)%NGRAPHS;
            }
        }
        for (int i = 0; i < NGRAPHS; ++i) {
            int n, p0 = 0;
            // now, readjust x axis begin,end
            if ((n=points[i].size())) {
                if (n > npts) {
                    p0 = pointsP0[i] = points[i].size()-npts;
                    n = npts;
                }
                graphs[i]->minx() = points[i][p0].x;
                graphs[i]->maxx() = graphs[i]->minx() + graphTimeSecs;
                // and, assign points
                graphs[i]->setPoints(&points[i][p0],n);
            }
        }
        
        tNow = getTime();

        const double tDelta = tNow - tLast;
        if (tLast > 0) {
            tAvg *= tNum;
            if (tNum >= 30) { tAvg -= tAvg/30.; --tNum; }
            tAvg += tDelta;
            tAvg /= ++tNum;
        } 
        tLast = tNow;
}

void GraphsWindow::updateGraphs()
{
    // repaint all graphs..
    for (int i = 0; i < (int)graphs.size(); ++i)
        if (graphs[i]->needsUpdateGL())
            graphs[i]->updateGL();
}

void GraphsWindow::downsampleChk(bool checked)
{
    if (checked) {
        downsampleRatio = params.srate/double(DOWNSAMPLE_TARGET_HZ);
        if (downsampleRatio < 1.) downsampleRatio = 1.;
    } else
        downsampleRatio = 1.;
    setGraphTimeSecs(graphTimeSecs); // clear the points and reserve the right capacities.
}


