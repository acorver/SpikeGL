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
#include <QHboxLayout>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QCursor>
#include <QFont>
#include <QPainter>
#include <QBrush>
#include <QColorDialog>
#include <QLineEdit>
#include <math.h>
#include <QMessageBox>
#include <QSettings>
#include "MainApp.h"
#include "HPFilter.h"
#include "QLed.h"
#include "ConfigureDialogController.h"
#include "play.xpm"
#include "pause.xpm"
#include "window_fullscreen.xpm"
#include "window_nofullscreen.xpm"
#include "apply_all.xpm"
//#include "fastsettle.xpm"

static const QIcon *playIcon(0), *pauseIcon(0), *windowFullScreenIcon(0), *windowNoFullScreenIcon(0), *applyAllIcon(0);

static const QColor AuxGraphBGColor(0xa6, 0x69,0x3c, 0xff),
                    NormalGraphBGColor(0x2f, 0x4f, 0x4f, 0xff);

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

GraphsWindow::GraphsWindow(DAQ::Params & p, QWidget *parent, bool isSaving)
    : QMainWindow(parent), params(p), nPtsAllGs(0), downsampleRatio(1.), tNow(0.), tLast(0.), tAvg(0.), tNum(0.), filter(0), suppressRecursive(false)
{
    sharedCtor(p, isSaving);
}

void GraphsWindow::sharedCtor(DAQ::Params & p, bool isSaving)
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

    QLabel *lbl = new QLabel("Seconds:", graphCtls);
    graphCtls->addWidget(lbl);
    graphSecs = new QDoubleSpinBox(graphCtls);
    graphSecs->setDecimals(3);
    graphSecs->setRange(.001, 10.0);
    graphSecs->setSingleStep(.25);    
    graphCtls->addWidget(graphSecs);

    lbl = new QLabel("YScale:", graphCtls);
    graphCtls->addWidget(lbl);
    graphYScale = new QDoubleSpinBox(graphCtls);
    graphYScale->setRange(.01, 100.0);
    graphYScale->setSingleStep(0.25);
    graphCtls->addWidget(graphYScale);

    lbl = new QLabel("Color:", graphCtls);
    graphCtls->addWidget(lbl);
    graphColorBut = new QPushButton(graphCtls);
    graphCtls->addWidget(graphColorBut);
    Connect(graphColorBut, SIGNAL(clicked(bool)), this, SLOT(doGraphColorDialog()));
    
    applyAllAct = graphCtls->addAction(*applyAllIcon, "Apply scale, secs & color to all graphs", this, SLOT(applyAll()));

    graphCtls->addSeparator();

	// Create downsample and filter controls and apply saved saved DownSampleChk and HPF Chk
	// NB: to correctly apply the saved filter and downsample state, we call downsampleChk() and hpfChk() at the end of this contstructor!
    QSettings settings("janelia.hhmi.org", APPNAME);
	settings.beginGroup("GraphsWindow");
	const bool setting_ds = settings.value("downsample",false).toBool(),
		       setting_filt = settings.value("filter",true).toBool();

    QCheckBox *dsc = new QCheckBox(QString("Downsample (%1 KHz)").arg(DOWNSAMPLE_TARGET_HZ/1000.), graphCtls);
    graphCtls->addWidget(dsc);
    dsc->setChecked(setting_ds);
    Connect(dsc, SIGNAL(clicked(bool)), this, SLOT(downsampleChk(bool)));

    highPassChk = new QCheckBox("Filter < 300Hz", graphCtls);
    graphCtls->addWidget(highPassChk);
    highPassChk->setChecked(setting_filt);
    filter = 0;
    Connect(highPassChk, SIGNAL(clicked(bool)), this, SLOT(hpfChk(bool)));

    graphCtls->addSeparator();
	
	/*
    QPushButton *fset = new QPushButton(QIcon(QPixmap(fastsettle_xpm)), "Fast Settle", graphCtls);
    if (p.mode == DAQ::AI60Demux || p.mode == DAQ::AI120Demux || p.mode == DAQ::AI128Demux) {
        fset->setToolTip("Toggle the DIO control line low/high for 2 seconds to 'fast settle' the input channels.");        
    } else {
        fset->setDisabled(true);
        fset->setToolTip("'Fast settle' is only available for DEMUX (INTAN) mode.");                
    }
    graphCtls->addWidget(fset);
    Connect(fset, SIGNAL(clicked(bool)), mainApp(), SLOT(doFastSettle()));
	 */
	
    graphCtls->addSeparator();
    toggleSaveChk = new QCheckBox("Toggle save", graphCtls);
    graphCtls->addWidget(toggleSaveChk);
    toggleSaveChk->setChecked(isSaving);
    Connect(toggleSaveChk, SIGNAL(clicked(bool)), this, SLOT(toggleSaveChecked(bool)));
    graphCtls->addWidget(saveFileLE = new QLineEdit(p.outputFile,graphCtls));
    saveFileLE->setEnabled(false);
    saveFileLE->setMinimumWidth(100);
    
    pdChan = -1;
    if (p.usePD) {
        pdChan = p.nVAIChans-1;
    }
    firstExtraChan = p.nVAIChans - p.nExtraChans;
    graphs.resize(p.nVAIChans);
	chks.resize(graphs.size());
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
            const int num = r*ncols+c;
            if (num >= (int)graphs.size()) { r=nrows,c=ncols; break; } // break out of loop
            QFrame * & f = (graphFrames[num] = mainApp()->getGLGraphWithFrame());
			QList<GLGraph *>  chlds = f->findChildren<GLGraph *>();			
			graphs[num] = chlds.size() ? chlds.front() : 0;
			QList<QCheckBox *> chkchlds = f->findChildren<QCheckBox *>();
			chks[num] = chkchlds.size() ? chkchlds.front() : 0;
            if (!graphs[num] || !chks[num]) {
                Error() << "INTERNAL ERROR: GLGraph " << num << " is invalid!";
				QMessageBox::critical(0,"INTERNAL ERROR", QString("GLGraph ") + QString::number(num) + " is invalid!");
				QApplication::exit(1);
                delete f, f = 0;
                continue;
            }
			chks[num]->setObjectName(QString::number(num));
			if (p.mode != DAQ::AIRegular && num < p.chanMap.size())
				chks[num]->setText(QString("Save Elec. %1").arg(p.chanMap[num].electrodeId));
			else
				chks[num]->setText(QString("Save Graph %1").arg(num));
			chks[num]->setChecked(p.demuxedBitMap.at(num));
			chks[num]->setDisabled(isSaving);
			chks[num]->setHidden(!mainApp()->isSaveCBEnabled() || p.mode == DAQ::AIRegular);
            f->setParent(graphsWidget);
            // do this for all the graphs.. disable vsync!
            graphs[num]->makeCurrent();
            Util::setVSyncMode(false, num == 0);
            f->setLineWidth(2);
            f->setFrameStyle(QFrame::StyledPanel|QFrame::Plain); // only enable frame when it's selected!
            graphs[num]->setObjectName(QString("GLGraph %1").arg(num));
            Connect(graphs[num], SIGNAL(cursorOver(double,double)), this, SLOT(mouseOverGraph(double,double)));
            Connect(graphs[num], SIGNAL(clicked(double,double)), this, SLOT(mouseClickGraph(double,double)));
            Connect(graphs[num], SIGNAL(doubleClicked(double,double)), this, SLOT(mouseDoubleClickGraph(double,double)));
			Connect(chks[num], SIGNAL(toggled(bool)), this, SLOT(saveGraphChecked(bool)));
            graphs[num]->setAutoUpdate(false);
            graphs[num]->setMouseTracking(true);
			graphs[num]->setCursor(Qt::CrossCursor);
            l->addWidget(f, r, c);
			///
			//QCheckBox *chk = new QCheckBox(graphsWidget);
			//l->addWidget(chk, r, c);
			//chk->raise();
			///
            setGraphTimeSecs(num, DEFAULT_GRAPH_TIME_SECS);
            graphs[num]->setTag(QVariant(num));
            if (num >= firstExtraChan) {
                // this is the photodiode channel
                graphs[num]->bgColor() = AuxGraphBGColor;
            } else {
                graphs[num]->bgColor() = NormalGraphBGColor;
            }
        }
    }
    selectedGraph = 0;
    lastMouseOverGraph = -1;;

    update_nPtsAllGs();

    QTimer *t = new QTimer(this);
    Connect(t, SIGNAL(timeout()), this, SLOT(updateGraphs()));
    t->setSingleShot(false);
    t->start(1000/DEF_TASK_READ_FREQ_HZ);        

    t = new QTimer(this);
    Connect(t, SIGNAL(timeout()), this, SLOT(updateMouseOver()));
    t->setSingleShot(false);
    t->start(MOUSE_OVER_UPDATE_INTERVAL_MS);

    selectGraph(0);
    updateGraphCtls();

	QWidget *leds = new QWidget;
	QHBoxLayout *hbl = new QHBoxLayout;


	// LED setup..
	stimTrigLed = new QLed;
	bool modeCaresAboutSGL = false, modeCaresAboutPD = false;
	switch(p.acqStartEndMode) {
		case DAQ::PDStart:
		case DAQ::PDStartEnd:
			modeCaresAboutPD = true;
		case DAQ::StimGLStartEnd:
		case DAQ::StimGLStart:
			modeCaresAboutSGL = true;
		default: break;/* do nothing */
	}
	lbl = new QLabel("SGLTrig:");
	stimTrigLed->setOffColor(p.stimGlTrigResave || modeCaresAboutSGL ? QLed::Red : QLed::Grey);
	stimTrigLed->setOnColor(QLed::Green);
	hbl->addWidget(lbl);
	stimTrigLed->setMinimumSize(20,20);
	hbl->addWidget(stimTrigLed);
	lbl = new QLabel("PDTrig:");
	pdTrigLed = new QLed;
	pdTrigLed->setOffColor(modeCaresAboutPD ? QLed::Red : QLed::Grey);
	pdTrigLed->setOnColor(QLed::Green);
	hbl->addWidget(lbl);
	pdTrigLed->setMinimumSize(20,20);
	hbl->addWidget(pdTrigLed);
	leds->setLayout(hbl);
//	leds->setMinimumSize(100,40);
	statusBar()->addPermanentWidget(leds);

	/// apply saved settings by calling the callback after everything is constructed
	downsampleChk(setting_ds);
	hpfChk(setting_filt);
	
	if (mainApp()->sortGraphsByElectrodeId) {
		// re-sort the graphs on-screen by electrode Id to restore previous state..
		sortGraphsByElectrodeId();
	}
}

GraphsWindow::~GraphsWindow()
{
	if (maximized) toggleMaximize(); // resets graphs to original state..
    const int gfs = graphFrames.size();
    for (int i = 0; i < gfs; ++i) mainApp()->putGLGraphWithFrame(graphFrames[i]);
    if (filter) delete filter, filter = 0;
}

void GraphsWindow::hideUnhideSaveChannelCBs() 
{
	for (int num = 0; num < (int)chks.size(); ++num)
		chks[num]->setHidden(!mainApp()->isSaveCBEnabled() || params.mode == DAQ::AIRegular);
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
    graphStats[num].clear();
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
        const double SRATE (params.srate);
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
    QSettings settings("janelia.hhmi.org", APPNAME);
	settings.beginGroup("GraphsWindow");
	settings.setValue("downsample",checked);
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
        if (isAuxChan(num)) {
            chanLbl->setText(QString("AUX%1").arg(int(num-(params.nVAIChans-params.nExtraChans)+1)));
        } else if (params.mode == DAQ::JFRCIntan32) {
            // JFRC Intan 32 mode has a hard-coded mapping.. sorry, not elegant but expedient!
            chanLbl->setText(QString("I%1_C%2").arg(num/16 + 1).arg(num % 16 + 1));            
        } else {
            // otherwise AIMux60 and AIMux120 both use the real mapping
            chanLbl->setText(QString("I%1_C%2").arg(params.chanMap[num].intan).arg(params.chanMap[num].intanCh));
        }
    }
    graphFrames[num]->setFrameStyle(QFrame::Box|QFrame::Plain);
    updateGraphCtls();
}

void GraphsWindow::doGraphColorDialog()
{
    int num = selectedGraph;
    QColorDialog::setCustomColor(0,NormalGraphBGColor.rgb());
    QColorDialog::setCustomColor(1,AuxGraphBGColor.rgb());
    QColor c = QColorDialog::getColor(graphs[num]->graphColor(), this);
    if (c.isValid()) {
        graphs[num]->graphColor() = c;
        updateGraphCtls();
    }
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
    { // update color button
        QPixmap pm(22,22);
        QPainter p;
        p.begin(&pm);
        p.fillRect(0,0,22,22,QBrush(graphs[num]->graphColor()));
        p.end();
        graphColorBut->setIcon(QIcon(pm));
    }
}

bool GraphsWindow::isAuxChan(unsigned num) const
{
    return num >= (params.nVAIChans-params.nExtraChans);
}

void GraphsWindow::computeGraphMouseOverVars(unsigned num, double & y,
                                             double & mean, double & stdev,
											 double & rms,
                                             const char * & unit)
{
    y += 1.;
    y /= 2.;
    // scale it to range..
    const double gain = isAuxChan(num) ? 1. : params.auxGain;
    y = (y*(params.range.max-params.range.min) + params.range.min) / gain;
    mean = graphStats[num].mean();
    stdev = graphStats[num].stdDev();
    rms = graphStats[num].rms();
    mean = (((mean+1.)/2.)*(params.range.max-params.range.min) + params.range.min) / gain;
    stdev = (((stdev+1.)/2.)*(params.range.max-params.range.min) + params.range.min) / gain;
    rms = (((rms+1.)/2.)*(params.range.max-params.range.min) + params.range.min) / gain;
    unit = "V";
    // check for millivolt..
    if ((((params.range.max-params.range.min) + params.range.min) / gain) < 1.0)
        unit = "mV",y *= 1000., mean *= 1000., stdev *= 1000., rms *= 1000.;  
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
    double mean, stdev, rms;
    const char *unit;
    computeGraphMouseOverVars(num, y, mean, stdev, rms, unit);
    QString msg;
    QString chStr;
    if (params.mode == DAQ::AIRegular) {
        chStr.sprintf("AI%d", num);
    } else { // MUX mode
        if (isAuxChan(num)) {
            chStr.sprintf("AUX%d",int(num-(params.nVAIChans-params.nExtraChans)+1));
        } else {
			const ChanMapDesc & desc = params.chanMap[num];
            chStr.sprintf("%d [I%u_C%u elec:%u]",num,desc.intan,desc.intanCh,desc.electrodeId);        
        }
    }
    msg.sprintf("%s %s %s @ pos (%.4f s, %.4f %s) -- mean: %.4f %s rms: %.4f %s stdDev: %.4f %s",(isNowOver ? "Mouse over" : "Last mouse-over"),(num == pdChan ? "photodiode graph" : (num < firstExtraChan ? "demuxed graph" : "graph")),chStr.toUtf8().constData(),x,y,unit,mean,unit,rms,unit,stdev,unit);
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
    QColor c = graphs[selectedGraph]->graphColor();
    for (int i = 0; i < graphs.size(); ++i) {
        setGraphTimeSecs(i, secs);
        graphs[i]->setYScale(scale);
        graphs[i]->graphColor() = c;
    }
    update_nPtsAllGs();    
}

void GraphsWindow::hpfChk(bool b)
{
    if (filter) delete filter, filter = 0;
    if (b) {
        filter = new HPFilter(graphs.size(), 300.0);
    }
    QSettings settings("janelia.hhmi.org", APPNAME);
	settings.beginGroup("GraphsWindow");
	settings.setValue("filter",b);
}

double GraphsWindow::GraphStats::rms() const { return sqrt(rms2()); }

double GraphsWindow::GraphStats::stdDev() const
{
//    return sqrt((s2 - ((s1*s1) / num)) / (num - 1));
    const double m = mean(), r2 = rms2();
    return sqrt(r2 - m*m);
}

int GraphsWindow::parseGraphNum(QObject *graph)
{
    int ret;
    bool ok;
    ret = graph->objectName().mid(8).toInt(&ok);
    if (!ok) ret = -1;
    return ret;
}

void GraphsWindow::toggleSaveChecked(bool b)
{
    if (b) {
        params.outputFile = saveFileLE->text().trimmed();
		if (sender() && !suppressRecursive && QFileInfo(params.outputFile).exists()) {
			QMessageBox::StandardButton b = QMessageBox::question(this, "File Exists", "The specified output file exists.  Continue with save?", QMessageBox::Save|QMessageBox::Abort, QMessageBox::Save);
			if (b == QMessageBox::Abort) {
				toggleSaveChk->setChecked(false);
				return;
			}
		}
        saveFileLE->setEnabled(false);
		suppressRecursive = true;
        mainApp()->toggleSave(b);
		suppressRecursive = false;
    } else {
        saveFileLE->setEnabled(true);
        mainApp()->toggleSave(b);
    }
	// hide/unhide all graph save checkboxes..
	const int n = chks.size();
	for (int i = 0; i < n; ++i)
		chks[i]->setDisabled(b || (params.mode == DAQ::AIRegular));
}

void GraphsWindow::setToggleSaveChkBox(bool b)
{
    toggleSaveChk->setChecked(b);
    toggleSaveChecked(b);
}

void GraphsWindow::setToggleSaveLE(const QString & fname)
{
    saveFileLE->setText(fname);
}

void GraphsWindow::setPDTrig(bool b) {
	pdTrigLed->setValue(b);
}

void GraphsWindow::setSGLTrig(bool b) {
	stimTrigLed->setValue(b);
}


void GraphsWindow::saveGraphChecked(bool b) {
	const int num = sender()->objectName().toInt();
	if (num >= 0 && num < (int)params.nVAIChans) {
		QVector<unsigned> subset;
		const int n = chks.size();
		for (int i = 0; i < n; ++i)
			if (chks[i]->isChecked()) subset.push_back(i);
		params.demuxedBitMap.setBit(num, b);
		qSort(subset);
		params.subsetString = ConfigureDialogController::generateAIChanString(subset);
		Debug() << "New subset string: " << params.subsetString;
		mainApp()->configureDialogController()->saveSettings();
	}
}

void GraphsWindow::retileGraphsAccordingToSorting(const QVector<int> & sorting) {
    int nrows = int(sqrtf(graphs.size())), ncols = nrows;
    while (nrows*ncols < (int)graphs.size()) {
        if (nrows > ncols) ++ncols;
        else ++nrows;
    };
	QGridLayout *l = (QGridLayout *)graphsWidget->layout();
    for (int i = 0; i < (int)graphFrames.size(); ++i) {
		// clear the layout of widgets..
           QFrame * f = graphFrames[i];
		l->removeWidget(f);
	}
	// no re-add them in the sorting order
    for (int r = 0; r < nrows; ++r) {
        for (int c = 0; c < ncols; ++c) {
            const int num = r*ncols+c;
            if (num >= (int)graphs.size()) { r=nrows,c=ncols; break; } // break out of loop
			const int graphId = sorting[num];
            QFrame * & f = graphFrames[graphId];
            l->addWidget(f, r, c);
        }
    }
}


void GraphsWindow::sortGraphsByElectrodeId() {
	QMap<int,int> eid2graph;
	DAQ::Params & p (params);
	const int cms = p.chanMap.size(), gs = graphs.size();
	int i;
	for (i = 0; i < cms; ++i) {
		eid2graph[p.chanMap[i].electrodeId] = i;
	}
	QVector<int> sorting;
	sorting.reserve(gs);
	for (QMap<int,int>::iterator it = eid2graph.begin(); it != eid2graph.end(); ++it)
		sorting.push_back(it.value());
	for ( ; i < gs; ++i)
		sorting.push_back(i);
	
	retileGraphsAccordingToSorting(sorting);
}

void GraphsWindow::sortGraphsByIntan() {
	const int gs = graphs.size();
	QVector<int> sorting;
	sorting.reserve(gs);
	// graph id's were originally sorted in this order so just specify that order..
	for (int i = 0; i < gs; ++i) sorting.push_back(i);
	
	retileGraphsAccordingToSorting(sorting);
}

