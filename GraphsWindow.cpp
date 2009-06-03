#include "GraphsWindow.h"
#include "Util.h"
#include <QToolBar>
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
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QCursor>
#include <QFont>
#include <math.h>
#include "MainApp.h"
#include "HPFilter.h"
#include "play.xpm"
#include "pause.xpm"
#include "window_fullscreen.xpm"
#include "window_nofullscreen.xpm"
#include "apply_all.xpm"
#include "fastsettle.xpm"

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
    graphCtls->addWidget(chanLbl = new QLabel("", graphCtls));
    chanLbl->setMargin(5);
    chanLbl->setFont(QFont("Courier", 10, QFont::Bold));
    graphCtls->addSeparator();
    pauseAct = graphCtls->addAction(*pauseIcon, "Pause/Unpause ALL graphs -- hold down shift for just this graph", this, SLOT(pauseGraph()));
    maxAct = graphCtls->addAction(*windowFullScreenIcon, "Maximize/Restore graph", this, SLOT(toggleMaximize()));
    pauseAct->setCheckable(true);
    maxAct->setCheckable(true);
    graphCtls->addSeparator();

    QLabel *lbl = new QLabel("Graph Seconds:", graphCtls);
    graphCtls->addWidget(lbl);
    graphSecs = new QDoubleSpinBox(graphCtls);
    graphSecs->setDecimals(3);
    graphSecs->setRange(.001, 10.0);
    graphSecs->setSingleStep(.25);    
    graphCtls->addWidget(graphSecs);

    lbl = new QLabel("Graph YScale:", graphCtls);
    graphCtls->addWidget(lbl);
    graphYScale = new QDoubleSpinBox(graphCtls);
    graphYScale->setRange(.01, 100.0);
    graphYScale->setSingleStep(0.25);
    graphCtls->addWidget(graphYScale);

    applyAllAct = graphCtls->addAction(*applyAllIcon, "Apply scale/secs to all graphs", this, SLOT(applyAll()));

    graphCtls->addSeparator();
    QCheckBox *dsc = new QCheckBox(QString("Downsample graphs to %1 Hz").arg(DOWNSAMPLE_TARGET_HZ), graphCtls);
    graphCtls->addWidget(dsc);
    dsc->setChecked(true);
    downsampleChk(true);
    Connect(dsc, SIGNAL(clicked(bool)), this, SLOT(downsampleChk(bool)));

    highPassChk = new QCheckBox("Filter < 300Hz", graphCtls);
    graphCtls->addWidget(highPassChk);
    highPassChk->setChecked(false);
    Connect(highPassChk, SIGNAL(clicked(bool)), this, SLOT(hpfChk(bool)));
    filter = 0;


    graphCtls->addSeparator();
    QPushButton *fset = new QPushButton(QIcon(QPixmap(fastsettle_xpm)), "Fast Settle", graphCtls);
    if (p.mode == DAQ::AI60Demux || p.mode == DAQ::AI120Demux) {
        fset->setToolTip("Toggle the DIO control line low/high for 2 seconds to 'fast settle' the input channels.");        
    } else {
        fset->setDisabled(true);
        fset->setToolTip("'Fast settle' is only available for DEMUX (INTAN) mode.");                
    }
    graphCtls->addWidget(fset);
    Connect(fset, SIGNAL(clicked(bool)), mainApp(), SLOT(doFastSettle()));
    
    pdChan = -1;
    if (p.usePD) {
        pdChan = p.nVAIChans-1;
    }
    firstExtraChan = p.nVAIChans - p.nExtraChans;
    graphs.resize(p.nVAIChans);
    graphFrames.resize(graphs.size());
    pausedGraphs.resize(graphs.size());
    graphTimesSecs.resize(graphs.size());
    nptsAll.resize(graphs.size());
    points.resize(graphs.size());
    graphStats.resize(graphs.size());
    
    maximized = 0;

    Connect(graphSecs, SIGNAL(valueChanged(double)), this, SLOT(graphSecsChanged(double)));
    Connect(graphYScale, SIGNAL(valueChanged(double)), this, SLOT(graphYScaleChanged(double)));

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
            graphs[num]->setTag(QVariant(num));
            if (num >= firstExtraChan) {
                // this is the photodiode channel
                graphs[num]->bgColor() = QColor(0xa6, 0x69,0x3c, 0xff);
            }
        }
    }
    selectedGraph = 0;
    isMVScale = fabs(((params.range.max-params.range.min) + params.range.min) / params.auxGain) < 1.0;
    lastMouseOverGraph = -1;;

    update_nPtsAllGs();

    QTimer *t = new QTimer(this);
    Connect(t, SIGNAL(timeout()), this, SLOT(updateGraphs()));
    t->setSingleShot(false);
    t->start(1000/TASK_READ_FREQ_HZ);        

    t = new QTimer(this);
    Connect(t, SIGNAL(timeout()), this, SLOT(updateMouseOver()));
    t->setSingleShot(false);
    t->start(MOUSE_OVER_UPDATE_INTERVAL_MS);

    selectGraph(0);
    updateGraphCtls();
}

GraphsWindow::~GraphsWindow()
{
    if (filter) delete filter, filter = 0;
}

void GraphsWindow::installEventFilter(QObject *obj)
{
    QObject::installEventFilter(obj);
    graphYScale->installEventFilter(obj);
    graphSecs->installEventFilter(obj);
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
        const int maximizedIdx = (maximized ? parseGraphNum(maximized) : -1);

        bool needFilter = filter;
        for (int i = startpt; i < DSIZE; ++i) {
            if (needFilter) {
                filter->apply(&data[i], deltaT);
                needFilter = false;
            }
            v.x = t;
            v.y = DPTR[i] / 32768.0; // hardcoded range of data
            if (!pgraphs[idx] && (maximizedIdx < 0 || maximizedIdx == idx)) {
                Vec2WrapBuffer & pbuf = pts[idx];
                GraphStats & gs = graphStats[idx];
                if (!pbuf.unusedCapacity()) {
                    const double val = pbuf.first().y;
                    gs.s1 -= val; // un-tally sum of values
                    gs.s2 -= val*val; // un-tally sum of squares of values
                    --gs.num;
                }
                pbuf.putData(&v, 1);
                gs.s1 += v.y; // tally sum of values
                gs.s2 += v.y*v.y; // tally sum of squares of values
                ++gs.num;
            }
            if (!(++idx%NGRAPHS)) {                
                idx = 0;
                t += deltaT;
                i = int((i-NGRAPHS) + DOWNSAMPLE_RATIO*NGRAPHS);
                if ((i+1)%NGRAPHS) i -= (i+1)%NGRAPHS;
                needFilter = filter;
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


void GraphsWindow::doPauseUnpause(int num, bool updateCtls)
{
    if (num < pausedGraphs.size()) {
        bool p = pausedGraphs[num] = !pausedGraphs[num];
        if (!p) // unpaused. clear the graph now..
            clearGraph(num);
    }
    if (updateCtls) 
        updateGraphCtls();
}

void GraphsWindow::pauseGraph()
{
    if (mainApp()->isShiftPressed()) { // shift pressed, do 1 graph
        int num = selectedGraph;
        doPauseUnpause(num, false);
    } else { // no shift pressed -- do all graphs
        const int sz = pausedGraphs.size();
        for (int i = 0; i < sz; ++i) 
            doPauseUnpause(i, false);
    }
    updateGraphCtls();
}

void GraphsWindow::selectGraph(int num)
{
    int old = selectedGraph;
    graphFrames[old]->setFrameStyle(QFrame::StyledPanel|QFrame::Plain);
    selectedGraph = num;
    if (params.mode == DAQ::AIRegular) { // straight AI (no MUX)
        chanLbl->setText(QString("AI%1").arg(num));
    } else { // MUX mode
        chanLbl->setText(QString("I%1_C%2").arg(params.chanMap[num].intan).arg(params.chanMap[num].intanCh));
    }
    graphFrames[num]->setFrameStyle(QFrame::Box|QFrame::Plain);
    updateGraphCtls();
}

void GraphsWindow::updateGraphCtls()
{
    int num = selectedGraph;
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
    graphYScale->setValue(graphs[num]->yScale());
    graphSecs->setValue(graphTimesSecs[num]);
}

void GraphsWindow::computeGraphMouseOverVars(unsigned num, double & y,
                                             double & mean, double & stdev,
                                             const char * & unit)
{
    y += 1.;
    y /= 2.;
    // scale it to range..
    y = (y*(params.range.max-params.range.min) + params.range.min) / params.auxGain;
    mean = graphStats[num].mean();
    stdev = graphStats[num].stdDev();
    mean = (((mean+1.)/2.)*(params.range.max-params.range.min) + params.range.min) / params.auxGain;
    stdev = (((stdev+1.)/2.)*(params.range.max-params.range.min) + params.range.min) / params.auxGain;
    unit = "V";
    if (isMVScale) unit = "mV",y *= 1000., mean *= 1000., stdev *= 1000.;  
}


void GraphsWindow::mouseClickGraph(double x, double y)
{
    int num = parseGraphNum(sender());
    selectGraph(num);
    lastMouseOverGraph = num;
    lastMousePos = Vec2(x,y);
    updateMouseOver();
}

void GraphsWindow::mouseOverGraph(double x, double y)
{
    int num = parseGraphNum(sender());
    lastMouseOverGraph = num;
    lastMousePos = Vec2(x,y);
    updateMouseOver();
}

void GraphsWindow::mouseDoubleClickGraph(double x, double y)
{
    int num = parseGraphNum(sender());
    selectGraph(num);
    toggleMaximize();
    updateGraphCtls();
    lastMouseOverGraph = num;
    lastMousePos = Vec2(x,y);   
    updateMouseOver();
}

void GraphsWindow::updateMouseOver()
{
    QWidget *w = QApplication::widgetAt(QCursor::pos());
    bool isNowOver = true;
    if (!w || !dynamic_cast<GLGraph *>(w)) isNowOver = false;
    const int & num = lastMouseOverGraph;
    if (num < 0 || num >= (int)graphs.size()) {
        statusBar()->clearMessage();
        return;
    }
    double y = lastMousePos.y, x = lastMousePos.x;
    double mean, stdev;
    const char *unit;
    computeGraphMouseOverVars(num, y, mean, stdev, unit);
    QString msg;
    ChanMapDesc & desc = params.chanMap[num];
    QString chStr;
    if (params.mode == DAQ::AIRegular) {
        chStr.sprintf("AI%d", num);
    } else { // MUX mode
        chStr.sprintf("%d [I%u_C%u pch: %u ech:%u]",num,desc.intan,desc.intanCh,desc.pch,desc.ech);        
    }
    msg.sprintf("%s %s %s @ pos (%.3f s, %.3f %s) -- mean: %.3f %s stdDev: %.3f %s",(isNowOver ? "Mouse over" : "Last mouse-over"),(num == pdChan ? "photodiode graph" : (num < firstExtraChan ? "demuxed graph" : "graph")),chStr.toUtf8().constData(),x,y,unit,mean,unit,stdev,unit);
    statusBar()->showMessage(msg);
}

void GraphsWindow::toggleMaximize()
{
    int num = selectedGraph;
    if (maximized && graphs[num] != maximized) {
        Warning() << "Maximize/unmaximize on a graph that isn't maximized when e have 1 graph maximized.. how is that possible?";
    } else if (maximized) { 
        graphsWidget->setHidden(true); // if we don't hide the parent, the below operation is slow and jerky
        // un-maximize
        for (int i = 0; i < (int)graphs.size(); ++i) {
            if (graphs[i] == maximized) continue;
            graphFrames[i]->setHidden(false);
            clearGraph(i); // clear previously-paused graph            
        }
        graphsWidget->setHidden(false);
        graphsWidget->show(); // now show parent
        maximized = 0;
    } else if (!maximized) {
        graphsWidget->setHidden(true); // if we don't hide the parent, the below operation is slow and jerky
        for (int i = 0; i < (int)graphs.size(); ++i) {
            if (num == i) continue;
            graphFrames[i]->setHidden(true);
        }
        maximized = graphs[num];
        graphsWidget->setHidden(false);
        graphsWidget->show(); // now show parent
    }
    updateGraphCtls();
}

    // clear a specific graph's points, or all if negative
void GraphsWindow::clearGraph(int which)
{
    if (which < 0 || which > graphs.size()) {
        // clear all..
        for (int i = 0; i < (int)points.size(); ++i) {
            points[i].clear();
            graphs[i]->setPoints(&points[i]);
            graphStats[i].clear();
        }
    } else {
        points[which].clear();
        graphs[which]->setPoints(&points[which]);
        graphStats[which].clear();
    }
}

void GraphsWindow::graphSecsChanged(double secs)
{
    int num = selectedGraph;
    setGraphTimeSecs(num, secs);
    update_nPtsAllGs();    
}

void GraphsWindow::graphYScaleChanged(double scale)
{
    int num = selectedGraph;
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

void GraphsWindow::hpfChk(bool b)
{
    if (filter) delete filter, filter = 0;
    if (b) {
        filter = new HPFilter(graphs.size(), 300.0);
    }
}

double GraphsWindow::GraphStats::stdDev() const
{
    return sqrt((s2 - ((s1*s1) / num)) / (num - 1));
}

int GraphsWindow::parseGraphNum(QObject *graph)
{
    int ret;
    bool ok;
    ret = graph->objectName().mid(8).toInt(&ok);
    if (!ok) ret = -1;
    return ret;
}
