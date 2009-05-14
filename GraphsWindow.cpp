#include "GraphsWindow.h"
#include "Util.h"
#include <QToolBar>
#include <QLCDNumber>
#include <QLabel>
#include <QGridLayout>
#include <math.h>
#include <QTimer>
#include <QCheckBox>
#include <QVector>
#include <QAction>
#include <QIcon>
#include <QPixmap>
#include <QStatusBar>
#include <QFrame>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QDoubleValidator>
#include <math.h>
#include "play.xpm"
#include "pause.xpm"
#include "window_fullscreen.xpm"
#include "window_nofullscreen.xpm"
#include "apply_all.xpm"
static const QIcon *playIcon(0), *pauseIcon(0), *windowFullScreenIcon(0), *windowNoFullScreenIcon(0), *applyAllIcon(0);

static void initIcons()
{
    if (!playIcon) {
        playIcon = new QIcon(play_xpm);
    }
    if (!pauseIcon) {
        pauseIcon = new QIcon(pause_xpm);
    }
    if (!windowFullScreenIcon) {
        windowFullScreenIcon = new QIcon(window_fullscreen_xpm);
    }
    if (!windowNoFullScreenIcon) {
        windowNoFullScreenIcon = new QIcon(window_nofullscreen_xpm);
    }
    if (!applyAllIcon) {
        applyAllIcon = new QIcon(apply_all_xpm);
    }
}

GraphsWindow::GraphsWindow(const DAQ::Params & p, QWidget *parent)
    : QMainWindow(parent), params(p), nPtsAllGs(0), downsampleRatio(1.), tNow(0.), tLast(0.), tAvg(0.), tNum(0.)
{    
    initIcons();
    setCentralWidget(graphsWidget = new QWidget(this));
    statusBar();
    resize(1024,768);
    graphCtls = addToolBar("Graph Controls");
    graphCtls->addWidget(new QLabel("Channel:", graphCtls));
    graphCtls->addWidget(chanLCD = new QLCDNumber(2, graphCtls));
    graphCtls->addSeparator();
    pauseAct = graphCtls->addAction(*pauseIcon, "Pause/Unpause graph", this, SLOT(pauseGraph()));
    maxAct = graphCtls->addAction(*windowFullScreenIcon, "Maximize/Restore graph", this, SLOT(toggleMaximize()));
    pauseAct->setCheckable(true);
    maxAct->setCheckable(true);
    graphCtls->addSeparator();
    QLabel *lbl = new QLabel("Graph Seconds:", graphCtls);
    graphCtls->addWidget(lbl);
    graphSecs = new QLineEdit(graphCtls);
    QDoubleValidator *dv = new QDoubleValidator(graphCtls);
    dv->setRange(.01, 10.0, 2);
    graphSecs->setValidator(dv);
    graphCtls->addWidget(graphSecs);
    lbl = new QLabel("Graph YScale:", graphCtls);
    graphCtls->addWidget(lbl);
    graphYScale = new QLineEdit(graphCtls);
    dv = new QDoubleValidator(graphCtls);
    dv->setRange(.01, 100.0, 2);
    graphYScale->setValidator(dv);
    graphCtls->addWidget(graphYScale);

    applyAllAct = graphCtls->addAction(*applyAllIcon, "Apply scale/secs to all graphs", this, SLOT(applyAll()));

    graphCtls->addSeparator();
    QCheckBox *dsc = new QCheckBox(QString("Downsample graphs to %1 Hz").arg(DOWNSAMPLE_TARGET_HZ), graphCtls);
    graphCtls->addWidget(dsc);
    dsc->setChecked(true);
    downsampleChk(true);
    Connect(dsc, SIGNAL(clicked(bool)), this, SLOT(downsampleChk(bool)));
    
    pdChan = -1;
    if (p.usePD) {
        pdChan = p.nVAIChans-1;
        graphs.resize(p.nVAIChans);
    } else
        graphs.resize(p.nVAIChans-1);
    graphFrames.resize(graphs.size());
    pausedGraphs.resize(graphs.size());
    graphTimesSecs.resize(graphs.size());
    nptsAll.resize(graphs.size());
    points.resize(graphs.size());

    maximized = 0;

    Connect(graphSecs, SIGNAL(textEdited(const QString &)), this, SLOT(graphSecsEdited(const QString &)));
    Connect(graphYScale, SIGNAL(textEdited(const QString &)), this, SLOT(graphYScaleEdited(const QString &)));

    QGridLayout *l = new QGridLayout(graphsWidget);
    l->setHorizontalSpacing(1);
    l->setVerticalSpacing(1);
    int nrows = int(sqrtf(graphs.size())), ncols = nrows;
    while (nrows*ncols < (int)graphs.size()) {
        if (nrows > ncols) ++ncols;
        else ++nrows;
    };
    for (int r = 0; r < nrows; ++r) {
        for (int c = 0; c < ncols; ++c) {
            int num = r*ncols+c;
            if (num >= (int)graphs.size()) { r=nrows,c=ncols; break; } // break out of loop
            QFrame *f = graphFrames[num] = new QFrame(graphsWidget);
            QVBoxLayout *bl = new QVBoxLayout(f);
            graphs[num] = new GLGraph(f);
            // do this for all the graphs.. disable vsync!
            graphs[num]->makeCurrent();
            Util::setVSyncMode(false, num == 0);
            bl->addWidget(graphs[num]);
            bl->setSpacing(0);
            bl->setContentsMargins(0,0,0,0);
            f->setLineWidth(2);
            graphFrames[num]->setFrameStyle(QFrame::StyledPanel|QFrame::Plain); // only enable frame when it's selected!
            graphs[num]->setObjectName(QString("GLGraph %1").arg(num));
            Connect(graphs[num], SIGNAL(cursorOver(double,double)), this, SLOT(mouseOverGraph(double,double)));
            Connect(graphs[num], SIGNAL(clicked(double,double)), this, SLOT(mouseClickGraph(double,double)));
            Connect(graphs[num], SIGNAL(doubleClicked(double,double)), this, SLOT(mouseDoubleClickGraph(double,double)));
            graphs[num]->setAutoUpdate(false);
            graphs[num]->setMouseTracking(true);
            l->addWidget(f, r, c);
            setGraphTimeSecs(num, DEFAULT_GRAPH_TIME_SECS);
            if (num == pdChan) {
                // this is the photodiode channel
                graphs[num]->bgColor() = QColor(0xa6, 0x69,0x3c, 0xff);
            }
        }
    }
    update_nPtsAllGs();

    QTimer *t = new QTimer(this);
    Connect(t, SIGNAL(timeout()), this, SLOT(updateGraphs()));
    t->setSingleShot(false);
    t->start(1000/TASK_READ_FREQ_HZ);        

    selectGraph(0);
    updateGraphCtls();
}

GraphsWindow::~GraphsWindow()
{
}

void GraphsWindow::setGraphTimeSecs(int num, double t)
{
    if (num < 0 || num >= (int)graphs.size()) return;
    graphTimesSecs[num] = t;
    i64 npts = nptsAll[num] = i64(ceil(t*params.srate/downsampleRatio));
    points[num].reserve(npts);
    double s = t;
    int nlines = 1;
    // try to figure out how many lines to draw based on nsecs..
    while (s>0. && !(nlines = int(s))) s*=10.;
    graphs[num]->setNumVGridLines(nlines);
    graphs[num]->setPoints(0);
    // NOTE: someone should call update_nPtsAllGs() after this!
}

void GraphsWindow::update_nPtsAllGs()
{
    nPtsAllGs = 0;
    for (int i = 0; i < graphs.size(); ++i) nPtsAllGs += nptsAll[i];
}

void GraphsWindow::putScans(std::vector<int16> & data, u64 firstSamp)
{
        const int NGRAPHS (graphs.size());
        const int DOWNSAMPLE_RATIO((int)downsampleRatio);
        const int SRATE (params.srate);
        // avoid some operator[] and others..
        const int DSIZE = data.size();
        const int16 * const DPTR = &data[0];
        const bool * const pgraphs = &pausedGraphs[0];
        Vec2WrapBuffer * const pts = &points[0];
        int startpt = int(DSIZE) - int(nPtsAllGs*DOWNSAMPLE_RATIO);
        i64 sidx = i64(firstSamp + u64(DSIZE)) - nPtsAllGs*i64(DOWNSAMPLE_RATIO);
        if (startpt < 0) {
            //qDebug("Startpt < 0 = %d", startpt);
            sidx += -startpt;
            startpt = 0;
        }

        double t = double(double(sidx) / NGRAPHS) / double(SRATE);
        const double deltaT =  1.0/SRATE * double(DOWNSAMPLE_RATIO);
        // now, push new points to back of each graph, downsampling if need be
        Vec2 v;
        int idx = 0;
        const int maximizedIdx = (maximized ? maximized->objectName().mid(8).toInt() : -1);

        for (int i = startpt; i < DSIZE; ++i) {
            v.x = t;
            v.y = DPTR[i] / 32768.0; // hardcoded range of data
            if (!pgraphs[idx] && (maximizedIdx < 0 || maximizedIdx == idx))
                pts[idx].putData(&v, 1);
            if (!(++idx%NGRAPHS)) {                
                idx = 0;
                t += deltaT;
                i = int((i-NGRAPHS) + DOWNSAMPLE_RATIO*NGRAPHS);
                if ((i+1)%NGRAPHS) i -= (i+1)%NGRAPHS;
            }
        }
        for (int i = 0; i < NGRAPHS; ++i) {
            if (pgraphs[i]) continue;
            // now, copy in temp data
            if (pts[i].size() >= 2) {
                // now, readjust x axis begin,end
                graphs[i]->minx() = pts[i].first().x;
                graphs[i]->maxx() = graphs[i]->minx() + graphTimesSecs[i];
                // uncomment below 2 line if the empty gap at the end of the downsampled graph annoys you, or comment them out to remove this 'feature'
                //if (!points[i].unusedCapacity())
                //    graphs[i]->maxx() = points[i].last().x;
            } 
            // and, notify graph of new points
            graphs[i]->setPoints(&pts[i]);
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
    for (int i = 0; i < graphs.size(); ++i)
        setGraphTimeSecs(i, graphTimesSecs[i]); // clear the points and reserve the right capacities.
    update_nPtsAllGs();
}


void GraphsWindow::pauseGraph()
{
    int num = chanLCD->intValue();
    if (num < pausedGraphs.size()) {
        bool p = pausedGraphs[num] = !pausedGraphs[num];
        if (!p) // unpaused. clear the graph now..
            clearGraph(num);
        updateGraphCtls();
    }
}

void GraphsWindow::selectGraph(int num)
{
    int old = chanLCD->intValue();
    graphFrames[old]->setFrameStyle(QFrame::StyledPanel|QFrame::Plain);
    chanLCD->display(num);
    graphFrames[num]->setFrameStyle(QFrame::Box|QFrame::Plain);
    updateGraphCtls();
}

void GraphsWindow::updateGraphCtls()
{
    int num = chanLCD->intValue();
    bool p = pausedGraphs[num];
    pauseAct->setChecked(p);
    pauseAct->setIcon(p ? *playIcon : *pauseIcon);
    if (maximized) {
        maxAct->setChecked(true);
        maxAct->setIcon(*windowNoFullScreenIcon);
    } else {
        maxAct->setChecked(false);
        maxAct->setIcon(*windowFullScreenIcon);
    }
    graphYScale->setText(QString::number(graphs[num]->yScale()));
    graphSecs->setText(QString::number(graphTimesSecs[num]));
}

void GraphsWindow::mouseClickGraph(double x, double y)
{
    int num = sender()->objectName().mid(8).toUInt();
    selectGraph(num);
    y += 1.;
    y /= 2.;
    // scale it to range..
    y = y*(params.range.max-params.range.min) + params.range.min;

    QString msg;
    msg.sprintf("Mouse press %s %d @ pos (%f, %f)",(num == pdChan ? "photodiode graph" : "graph"),num,x,y);
    statusBar()->showMessage(msg);
}

void GraphsWindow::mouseOverGraph(double x, double y)
{
    int num = sender()->objectName().mid(8).toUInt();
    y += 1.;
    y /= 2.;
    // scale it to range..
    y = y*(params.range.max-params.range.min) + params.range.min;

    QString msg;
    msg.sprintf("Mouse over %s %d @ pos (%f, %f)",(num == pdChan ? "photodiode graph" : "graph"),num,x,y);
    statusBar()->showMessage(msg);
}

void GraphsWindow::mouseDoubleClickGraph(double x, double y)
{
    int num = sender()->objectName().mid(8).toUInt();
    selectGraph(num);
    y += 1.;
    y /= 2.;
    toggleMaximize();
    updateGraphCtls();
    // scale it to range..
    y = y*(params.range.max-params.range.min) + params.range.min;
    QString msg;
    msg.sprintf("Mouse dbl-click graph %d @ pos (%f, %f)",num,x,y);
    statusBar()->showMessage(msg);
}

void GraphsWindow::toggleMaximize()
{
    int num = chanLCD->intValue();
    if (maximized && graphs[num] != maximized) {
        Warning() << "Maximize/unmaximize on a graph that isn't maximized when e have 1 graph maximized.. how is that possible?";
    } else if (maximized) { 
        // un-maximize
        for (int i = 0; i < (int)graphs.size(); ++i) {
            if (graphs[i] == maximized) continue;
            graphFrames[i]->setHidden(false);
            graphFrames[i]->show();
            clearGraph(i); // clear previously-paused graph            
        }
        maximized = 0;
    } else if (!maximized) {
        for (int i = 0; i < (int)graphs.size(); ++i) {
            if (num == i) continue;
            graphFrames[i]->setHidden(true);
        }
        maximized = static_cast<GLGraph *>(sender());
    }
}

    // clear a specific graph's points, or all if negative
void GraphsWindow::clearGraph(int which)
{
    if (which < 0 || which > graphs.size()) {
        // clear all..
        for (int i = 0; i < (int)points.size(); ++i)
            points[i].clear(), graphs[i]->setPoints(&points[i]);
    } else
        points[which].clear(), graphs[which]->setPoints(&points[which]);
}

void GraphsWindow::graphSecsEdited(const QString &txt)
{
    int num = chanLCD->intValue();
    bool ok;
    double secs = txt.toDouble(&ok);
    if (!ok || !graphSecs->hasAcceptableInput()) {
        //graphSecs->setText(QString::number(graphTimesSecs[num]));
        return; // reject..
    }
    setGraphTimeSecs(num, secs);
    update_nPtsAllGs();    
}

void GraphsWindow::graphYScaleEdited(const QString &txt)
{
    int num = chanLCD->intValue();
    bool ok;
    double scale = txt.toDouble(&ok);
    if (!ok || !graphYScale->hasAcceptableInput()) {
        //graphYScale->setText(QString::number(graphs[num]->yScale()));
        return; // rejected..
    }
    graphs[num]->setYScale(scale);
}

void GraphsWindow::applyAll()
{
    if (!graphSecs->hasAcceptableInput() || !graphYScale->hasAcceptableInput()) return;
    double secs = graphSecs->text().toDouble(), scale = graphYScale->text().toDouble();
    for (int i = 0; i < graphs.size(); ++i) {
        setGraphTimeSecs(i, secs);
        graphs[i]->setYScale(scale);
    }
    update_nPtsAllGs();    
}
