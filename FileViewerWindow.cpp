/*
 *  FileViewerWindow.cpp
 *  SpikeGL
 *
 *  Created by calin on 10/26/10.
 *  Copyright 2010 Calin Culianu <calin.culianu@gmail.com>. All rights reserved.
 *
 */

#include "FileViewerWindow.h"
#include "GLGraph.h"
#include "MainApp.h"
#include "ConsoleWindow.h"
#include <QFileInfo>
#include <QScrollArea>
#include <limits.h>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QToolBar>
#include <QMenuBar>
#include <QStatusBar>
#include "close_but_16px.xpm"
#include <QPixmap>
#include <QTimer>
#include <QCursor>
#include <QResizeEvent>
#include <QScrollBar>

const QString FileViewerWindow::viewModeNames[] = {
	"Tiled", "Stacked", "Stacked Large", "Stacked Huge"
};

const QString FileViewerWindow::colorSchemeNames[] = {
	"Ice", "Fire", "Green", "BlackWhite", "Classic"
};

FileViewerWindow::FileViewerWindow()
: QMainWindow(0), pscale(1)
{	
	QWidget *cw = new QWidget(this);
	QVBoxLayout *l = new QVBoxLayout(cw);
	setCentralWidget(cw);
	
	scrollArea = new QScrollArea(cw);	
	graphParent = new QWidget(scrollArea);
	scrollArea->setWidget(graphParent);
	l->addWidget(scrollArea, 1);
	
	// bottom slider
	QWidget *w = new QWidget(cw);
	QHBoxLayout *hl = new QHBoxLayout(w);
	QLabel *lbl = new QLabel("File position: ", w);	
	hl->addWidget(lbl);
	lbl = new QLabel("scans", w);
	posScansSB = new QSpinBox(w);
	hl->addWidget(lbl, 0, Qt::AlignRight);
	hl->addWidget(posScansSB, 0, Qt::AlignLeft);
	lbl = totScansLbl =  new QLabel("", w);
	hl->addWidget(lbl, 0, Qt::AlignLeft);
	hl->addSpacing(25);
	lbl = new QLabel("secs", w);
	posSecsSB = new QDoubleSpinBox(w);
	posSecsSB->setDecimals(3);
	posSecsSB->setSingleStep(0.001);
	hl->addWidget(lbl, 0, Qt::AlignRight);
	hl->addWidget(posSecsSB, 0, Qt::AlignLeft);
	lbl = totSecsLbl =  new QLabel("", w);
	hl->addWidget(lbl, 0, Qt::AlignLeft);
	hl->addSpacing(10);
	
	posSlider = new QSlider(Qt::Horizontal, w);
	posSlider->setMinimum(0);
	posSlider->setMaximum(1000);
	hl->addWidget(posSlider, 1);
	Connect(posSlider, SIGNAL(valueChanged(int)), this, SLOT(setFilePos(int)));
	Connect(posScansSB, SIGNAL(valueChanged(int)), this, SLOT(setFilePos(int)));
	Connect(posSecsSB, SIGNAL(valueChanged(double)), this, SLOT(setFilePosSecs(double)));
	
	toolBar = addToolBar("Graph Controls");
	lbl = new QLabel("<font size=-1>X-Scale (secs):</font>", toolBar);
    toolBar->addWidget(lbl);
    xScaleSB = new QDoubleSpinBox(toolBar);
    xScaleSB->setDecimals(3);
    xScaleSB->setRange(.0001, 1e6);
    xScaleSB->setSingleStep(.25);    
    toolBar->addWidget(xScaleSB);
	
    lbl = new QLabel("<font size=-1>Y-Scale (factor):</font>", toolBar);
    toolBar->addWidget(lbl);
    yScaleSB = new QDoubleSpinBox(toolBar);
    yScaleSB->setRange(.01, 100.0);
    yScaleSB->setSingleStep(0.25);
    toolBar->addWidget(yScaleSB);

	Connect(xScaleSB, SIGNAL(valueChanged(double)), this, SLOT(setXScaleSecs(double)));
    Connect(yScaleSB, SIGNAL(valueChanged(double)), this, SLOT(setYScale(double)));
	
	
	toolBar->addSeparator();
	toolBar->addWidget(lbl = new QLabel("<font size=-1>N Divs:</font>", toolBar));
	toolBar->addWidget(nDivsSB = new QSpinBox(toolBar));
	nDivsSB->setMinimum(0);
	nDivsSB->setMaximum(10);
	Connect(nDivsSB, SIGNAL(valueChanged(int)), this, SLOT(setNDivs(int)));
	xDivLbl = new QLabel("", toolBar);
	toolBar->addWidget(xDivLbl);
	yDivLbl = new QLabel("", toolBar);
	toolBar->addWidget(yDivLbl);
	
	toolBar->addWidget(new QLabel("<font size=-1>Gain:</font>", toolBar));
	toolBar->addWidget(auxGainSB = new QDoubleSpinBox(toolBar));
	auxGainSB->setDecimals(3);
	auxGainSB->setRange(0.001,1e6);
	Connect(auxGainSB, SIGNAL(valueChanged(double)), this, SLOT(setAuxGain(double)));
	
	l->addWidget(w);
	
	
	statusBar(); // creates one implicitly

//#ifdef Q_OS_MACX
//	// shared menu bar on OSX
//	QMenuBar *mb = mainApp()->console()->menuBar();
//#else
//	// otherwise window-specific menu bar
	QMenuBar *mb = menuBar();
//#endif
	QMenu *m = mb->addMenu("Color &Scheme");
	for (int i = 0; i < (int)N_ColorScheme; ++i) {
		QAction * a = colorSchemeActions[i] = m->addAction(colorSchemeNames[i], this, SLOT(colorSchemeMenuSlot()));
		a->setCheckable(true);
	}
	m = mb->addMenu("&View Mode");
	for (int i = 0; i < (int)N_ViewMode; ++i) {
		QAction *a = viewModeActions[i] = m->addAction(viewModeNames[i], this, SLOT(viewModeMenuSlot()));
		a->setCheckable(true);
	}
	channelsMenu = mb->addMenu("&Channels");
	channelsMenu->addAction("Show All", this, SLOT(showAllGraphs()));
	channelsMenu->addSeparator();
	
	closeLbl = new QLabel(this);
	QPixmap pm(close_but_16px);
	closeLbl->setPixmap(pm);
	closeLbl->resize(pm.size());
	closeLbl->setToolTip("Hide this graph");
	closeLbl->setCursor(Qt::ArrowCursor);
	closeLbl->hide();
	closeLbl->move(-100,-100);
	
	didLayout = false;
	
	hideCloseTimer = new QTimer(this);
	hideCloseTimer->setInterval(250);
	hideCloseTimer->setSingleShot(false);
	Connect(hideCloseTimer, SIGNAL(timeout()), this, SLOT(hideCloseTimeout()));
}

FileViewerWindow::~FileViewerWindow()
{	
	/// scrollArea and graphParent automatically deleted here because they are children of us.
}


bool FileViewerWindow::viewFile(const QString & fname, QString *errMsg /* = 0 */)
{
	if (dataFile.isOpenForRead()) {
		QString err = "INTERNAL ERROR: Cannot re-use instances of FileViewerWindow to subsequent multiple files.";
		if (errMsg) *errMsg = err;
		Error() << err;
		return false;
	}
	QString fname_no_path = QFileInfo(fname).fileName();
	if (!dataFile.openForRead(fname)) {
		QString err = fname_no_path + ": miscellaneous error on open."; 
		if (errMsg) *errMsg = err;
		Error() << err;
		return false;
	}
	if (!dataFile.scanCount()) {
		QString err = fname_no_path + " is empty!";
		if (errMsg) *errMsg = err;
		Error() << err;
		return false; // file is empty
	}
	
	setWindowTitle(QString(APPNAME) + QString(" File Viewer - ") + QFileInfo(fname_no_path).fileName() + " " 
				   + QString::number(dataFile.numChans()) + " channels @ " 
				   + QString::number(dataFile.samplingRateHz()) 
				   + " Hz"
				   + ", " + QString::number(dataFile.scanCount()) + " scans"
				   );
	
	loadSettings();
			
	graphs.resize(dataFile.numChans());	
	graphHideUnhideActions.resize(graphs.size());
	for (int i = 0, n = graphs.size(); i < n; ++i) {
		graphs[i] = new GLGraph(graphParent);
		graphs[i]->setAutoUpdate(false);
		graphs[i]->setMouseTracking(true);
		graphs[i]->setObjectName(QString("Graph ") + QString::number(i) + " ChanID " + QString::number(dataFile.channelIDs()[i]));
		graphs[i]->setToolTip(graphs[i]->objectName());
		graphs[i]->setTag(i);
		graphs[i]->setCursor(Qt::CrossCursor);
		Connect(graphs[i], SIGNAL(cursorOver(double,double)), this, SLOT(mouseOverGraph(double,double)));
		Connect(graphs[i], SIGNAL(cursorOverWindowCoords(int,int)), this, SLOT(mouseOverGraphInWindowCoords(int,int)));
		Connect(graphs[i], SIGNAL(clickedWindowCoords(int,int)), this, SLOT(clickedGraphInWindowCoords(int,int)));
		QAction *a = new QAction(graphs[i]->objectName(), this);
		a->setObjectName(QString::number(i)); // hack sorta: tag it with its id for use later in the slot
		a->setCheckable(true);
		a->setChecked(true);
		channelsMenu->addAction(a);
		Connect(a, SIGNAL(triggered()), this, SLOT(hideUnhideGraphSlot()));
		graphHideUnhideActions[i] = a;
	}
	graphBufs.resize(graphs.size());
	hiddenGraphs.fill(false, graphs.size());

	auxGain = dataFile.auxGain();
	auxGainSB->blockSignals(true); 	auxGainSB->setValue(auxGain); auxGainSB->blockSignals(false);
	chanMap = dataFile.chanMap();
	maximizedGraph = -1;
	
	layoutGraphs();
	
	//setFilePos(0);
	pos = 0;
		
	if (errMsg) *errMsg = QString::null;
	
	return true;
}

bool FileViewerWindow::queryCloseOK() 
{
	// for now, always ok..
	return true;
}

void FileViewerWindow::loadSettings() 
{
    QSettings settings("janelia.hhmi.org", APPNAME);
	
    settings.beginGroup("FileViewerWindow");
	viewMode = Tiled;
    int vm = settings.value("viewMode", (int)Tiled).toInt();
	if (vm >= 0 && vm < N_ViewMode) viewMode = (ViewMode)vm;
	nSecsZoom = settings.value("nSecsZoom", 4.0).toDouble();
	yZoom = settings.value("yZoom", 1.0).toDouble();
	nDivs = settings.value("nDivs", 4).toUInt();
	int cs = settings.value("colorScheme", DefaultScheme).toInt();
	if (cs < 0 || cs >= N_ColorScheme) cs = 0;
	colorScheme = (ColorScheme)cs;
	xScaleSB->blockSignals(true);
	yScaleSB->blockSignals(true);
	nDivsSB->blockSignals(true);
	xScaleSB->setValue(nSecsZoom);
	yScaleSB->setValue(yZoom);
	nDivsSB->setValue(nDivs);
	nDivsSB->blockSignals(false);
	xScaleSB->blockSignals(false);
	yScaleSB->blockSignals(false);	
}

void FileViewerWindow::saveSettings()
{
    QSettings settings("janelia.hhmi.org", APPNAME);
	
    settings.beginGroup("FileViewerWindow");
    settings.setValue("viewMode", (int)viewMode);	
	settings.setValue("colorScheme", (int)colorScheme);
}

void FileViewerWindow::layoutGraphs()
{	
	didLayout = false;
	closeLbl->hide();
	static const int padding = 5;
	const int n = graphs.size();

	if (graphParent->layout()) 
		delete graphParent->layout();

	if (viewMode == Tiled) {
		updateGeometry();
		const QSize sz = scrollArea->size();
		scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		graphParent->resize(sz.width(), sz.height());
		QGridLayout *l = new QGridLayout(graphParent);
		l->setHorizontalSpacing(padding);
		l->setVerticalSpacing(padding);
		graphParent->setLayout(l);
		const int ngraphs = graphs.size() - hiddenGraphs.count(true);
		int nrows = int(sqrtf(ngraphs)), ncols = nrows;
		while (nrows*ncols < (int)ngraphs) {
			if (nrows > ncols) ++ncols;
			else ++nrows;
		}
		for (int r = 0, num = 0; r < nrows; ) {
			for (int c = 0; c < ncols; ++num) {
				if (num >= graphs.size()) { r=nrows,c=ncols; break; } // break out of loop
				if (!hiddenGraphs.testBit(num)) {
					l->addWidget(graphs[num], r, c);
					if (++c >= ncols) ++r;
				}
			}
		}

	} else if (viewMode == StackedLarge || viewMode == Stacked || viewMode == StackedHuge) {
		const int stk_h = viewMode == StackedHuge ? 320 : (viewMode == StackedLarge ? 160 : 80);
		scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

		int w; 
		graphParent->resize(w=(scrollArea->width() - scrollArea->verticalScrollBar()->width() - 5), (stk_h + padding) * hiddenGraphs.count(false));
		if (w <= 0) w = 1;
		int y = 0;

		for (int i = 0; i < n; ++i) {
			if (!hiddenGraphs.testBit(i)) {
				graphs[i]->resize(w, stk_h);
				graphs[i]->move(0,y);
				y += graphs[i]->height() + padding;
			}
		}
		
	}	
	
//	graphParent->resize(w, y);
	didLayout = true;
}


void FileViewerWindow::updateData()
{
	const double srate = dataFile.samplingRateHz();

	QBitArray channelSubset = ~hiddenGraphs;
	const int nChans = graphs.size(), nChansOn = channelSubset.count(true);	
	QVector<int> chanIdsOn(nChansOn);
	for (int i = 0, j = 0; i < nChans; ++i)
		if (channelSubset.testBit(i)) chanIdsOn[j++] = i;
	
    i64 num = nSecsZoom * srate;
	int downsample = num / graphs[chanIdsOn[0]]->width();
	if (downsample <= 0) downsample = 1;
	std::vector<int16> data;
	i64 nread = dataFile.readScans(data, pos, num, channelSubset, downsample);
	
	
	if (nread < 0) {
		Error() << "Error reading data from input file!";
	} else {

		// demux for graphs
		for (int i = 0; i < nChans; ++i) {
			graphs[i]->setPoints(0);
			if (channelSubset.testBit(i) && graphBufs[i].capacity() != nread)
				graphBufs[i].reserve(nread);
		}

		for (int i = 0; i < nread; ++i) {
			for (int j = 0; j < nChansOn; ++j) {
				Vec2 vec(double(i)/double(nread), double(data[i * nChansOn + j])/double(SHRT_MAX));
				graphBufs[chanIdsOn[j]].putData(&vec, 1);
			}
		}
		
	}
	
	for (int i = 0; i < nChans; ++i) {
		graphs[i]->setYScale(yZoom);
		graphs[i]->setNumHGridLines(nDivs);
		graphs[i]->setNumVGridLines(nDivs);
		applyColorScheme(graphs[i]);
		graphs[i]->setPoints(&graphBufs[i]);
		graphs[i]->updateGL();
	}
	
	// set ranges, etc of various widgets
	configureMiscControls();
	
	// now update various widgets
	posScansSB->blockSignals(true);
	posSecsSB->blockSignals(true);
	posSlider->blockSignals(true);
	posScansSB->setValue(pos); // FIXME TODO: the value here is an int.  We want a 64-bit value...
	posSecsSB->setValue(timeFromPos(pos));
	posSlider->setValue(pos);
	posScansSB->blockSignals(false);
	posSecsSB->blockSignals(false);
	posSlider->blockSignals(false);
	
	totSecsLbl->setText(QString("<font size=-1> to ") + QString::number(timeFromPos(pos + nScansPerGraph()), 'f', 3) + "</font> <font size=-2>(out of " + QString::number(timeFromPos(dataFile.scanCount()-1), 'f', 3) + ")</font>");
	totScansLbl->setText(QString("<font size=-1> to ") + QString::number(pos + nScansPerGraph()) + "</font> <font size=-2>(out of " + QString::number(dataFile.scanCount()-1) + ")</font>");
	xDivLbl->setText(QString("<font size=-1>Secs/Div: ") + (!nDivs ? "-" : QString::number(nSecsZoom / nDivs)) + "</font>");
	QPair<double, double> yv = yVoltsAfterGain();
	yDivLbl->setText(QString("<font size=-1>Volts/Div: ") + (!nDivs ? "-" : QString::number(((yv.second-yv.first) / nDivs) / yZoom)) + "</font>");
	
	for (int i = 0; i < (int)N_ColorScheme; ++i) 
		colorSchemeActions[i]->setChecked(i == (int)colorScheme);	
	for (int i = 0; i < (int)N_ViewMode; ++i)
		viewModeActions[i]->setChecked(i == (int)viewMode);
}

double FileViewerWindow::timeFromPos(qint64 p) const 
{
	return p / dataFile.samplingRateHz();
}

qint64 FileViewerWindow::posFromTime(double s) const 
{
	return dataFile.samplingRateHz() * s;
}

void FileViewerWindow::setFilePos(int p) 
{
	if (p < 0) p = 0;
	pos = qint64(p) * pscale;
	const qint64 limit = dataFile.scanCount() - nScansPerGraph() - 1;
	if (pos > limit) pos = limit;
	updateData();
}

void FileViewerWindow::setFilePosSecs(double s)
{
	setFilePos(posFromTime(s));
}

void FileViewerWindow::configureMiscControls()
{	
	xScaleSB->setRange(0.0001, dataFile.fileTimeSecs());

	qint64 maxVal = (dataFile.scanCount()-1) - nScansPerGraph();
	if (maxVal < 0) maxVal = 0;

	posScansSB->setMinimum(0);
	posScansSB->setMaximum(maxVal);	
	posSecsSB->setMinimum(0);
	posSecsSB->setMaximum(timeFromPos(maxVal));
	
	// figure out appropriate scale for the slider
	pscale = 1;
	while ( maxVal / pscale > qint64(INT_MAX))
		pscale = pscale==qint64(1) ? qint64(2) : pscale*pscale;	
	posSlider->setMaximum(maxVal);

	posScansSB->blockSignals(true);
	posSecsSB->blockSignals(true);
	posSlider->blockSignals(true);
	
	posSlider->setValue(pos);
	posScansSB->setValue(pos);
	posSecsSB->setValue(timeFromPos(pos));
	
	posScansSB->blockSignals(false);
	posSecsSB->blockSignals(false);
	posSlider->blockSignals(false);
}

qint64 FileViewerWindow::nScansPerGraph() const { return nSecsZoom * dataFile.samplingRateHz(); }

void FileViewerWindow::setXScaleSecs(double d) 
{ 
	nSecsZoom = d;
	updateData();
}

void FileViewerWindow::setYScale(double d) 
{ 
	yZoom = d;
	updateData();
}

QPair<double, double> FileViewerWindow::yVoltsAfterGain() const
{
	QPair<double, double> ret;
	ret.first = dataFile.rangeMin() / auxGain;
	ret.second = dataFile.rangeMax() / auxGain;
	return ret;
}

void FileViewerWindow::setNDivs(int n)
{
	nDivs = n;
	updateData();
}

void FileViewerWindow::applyColorScheme(GLGraph *g)
{
	QColor fg, bg, grid;
	switch (colorScheme) {
		case Ice:
			fg = QColor(0x87, 0xce, 0xfa, 0x7f);
			bg.setRgbF(.15,.15,.15);
			grid.setRgbF(0.4,0.4,0.4);			
			break;
		case Fire:
			fg = QColor(0xfa, 0x87, 0x37, 0x7f);
			bg.setRgbF(.15,.15,.15);
			grid.setRgbF(0.4,0.4,0.4);			
			break;
		case Green:
			fg = QColor(0x09, 0xca, 0x09, 0x7f);
			bg.setRgbF(.07,.07,.07);
			grid.setRgbF(0.4,0.4,0.4);			
			break;
		case BlackWhite:
			fg = QColor(0xca, 0xca, 0xca, 0x7f);
			bg.setRgbF(.05,.05,.05);
			grid.setRgbF(0.4,0.4,0.4);			
			break;
		case Classic:
		default:
			bg = QColor(0x2f, 0x4f, 0x4f);
			fg = QColor(0xee, 0xdd, 0x82);
			grid = QColor(0x87, 0xce, 0xfa, 0x7f);			
			break;
	}
	g->bgColor() = bg;
	g->graphColor() = fg;
	g->gridColor() = grid;
}

void FileViewerWindow::colorSchemeMenuSlot()
{
	ColorScheme oldScheme = colorScheme;
	for (int i = 0; i < (int)N_ColorScheme; ++i) {
		if (sender() == colorSchemeActions[i]) {
			colorScheme = (ColorScheme)i;
			colorSchemeActions[i]->setChecked(true);
		} else {
			colorSchemeActions[i]->setChecked(false);			
		}
	}
	if (oldScheme != colorScheme) {
		saveSettings();
		updateData();
	}
}

void FileViewerWindow::setAuxGain(double d)
{
	auxGain = d;
	updateData();
}

void FileViewerWindow::mouseOverGraph(double x, double y)
{
	GLGraph *sendr = dynamic_cast<GLGraph *>(sender());
	if (!sendr) return;
	const int num = sendr->tag().toInt();
	
	QWidget *w = QApplication::widgetAt(QCursor::pos());
    bool isNowOver = true;
    if (!w || !dynamic_cast<GLGraph *>(w)) isNowOver = false;
    if (num < 0 || num >= (int)graphs.size()) {
        statusBar()->clearMessage();
        return;
    }
    const char *unit = "V";
	x = timeFromPos(pos) + x*nSecsZoom; // map x, originally a 0->1 value, to a time
	y += 1.;
    y /= 2.;
    y = (y*(dataFile.rangeMax()-dataFile.rangeMin()) + dataFile.rangeMin()) / auxGain;
    // check for millivolt..
    if ((((dataFile.rangeMax()-dataFile.rangeMin()) + dataFile.rangeMin()) / auxGain) < 1.0)
        unit = "mV",y *= 1000.;
	
    QString msg;
    QString chStr;
	const int chanId = dataFile.channelIDs()[num];
    if (dataFile.daqMode() == DAQ::AIRegular) {
        chStr.sprintf("AI%d", chanId);
    } else { // MUX mode
		if (num < chanMap.size()) {
			const ChanMapDesc & desc = chanMap[chanId];
            chStr.sprintf("%d ChanID %d  [I%u_C%u pch: %u ech:%u]", num, chanId, desc.intan,desc.intanCh,desc.pch,desc.ech);        
		} else {
			chStr.sprintf("%d ChanID %d", num, chanId);
		}
    }
    msg.sprintf("%s Graph %s @ pos (%.4f s, %.4f %s)",(isNowOver ? "Mouse over" : "Last mouse-over"),chStr.toUtf8().constData(),x,y,unit);
    statusBar()->showMessage(msg);
	
}

void FileViewerWindow::mouseOverGraphInWindowCoords(GLGraph *g, int x, int y)
{
	if (!didLayout) return;
	const QSize sz = closeLbl->size();
	if (!g) return;
	if (x > g->width()-sz.width() && y < sz.height()) {
		if (hiddenGraphs.count(true) >= graphs.size()-1) // on last graph, can't close
			return;
		closeLbl->hide();
		closeLbl->setParent(g);
		closeLbl->move(g->width()-sz.width(),0);
		closeLbl->show();
		hideCloseTimer->stop();
		hideCloseTimer->start();
	} else if (closeLbl->isVisible()) {
		// let the timer hide it! :)
	}	
}

void FileViewerWindow::mouseOverGraphInWindowCoords(int x, int y)
{
	mouseOverGraphInWindowCoords(dynamic_cast<GLGraph *>(sender()), x, y);
}

void FileViewerWindow::hideGraph(int num)
{
	if (num < 0 || num >= graphs.size()) return;
	GLGraph *g = graphs[num];
	g->hide();
	hiddenGraphs.setBit(num);
	graphHideUnhideActions[num]->setChecked(false);	
	layoutGraphs();
	updateData();
}

void FileViewerWindow::showGraph(int num)
{
	if (num < 0 || num >= graphs.size()) return;
	GLGraph *g = graphs[num];
	hiddenGraphs.clearBit(num);
	graphHideUnhideActions[num]->setChecked(true);	
	layoutGraphs();
	updateData();
	g->show();
}

void FileViewerWindow::clickedGraphInWindowCoords(int x, int y)
{
	if (!didLayout) return;
	GLGraph *g = dynamic_cast<GLGraph *>(sender());
	const QSize sz = closeLbl->size();
	if (!g) return;
	if (x > g->width()-sz.width()  && y < sz.height() ) {
		int num = g->tag().toInt();
		hideGraph(num);
		// make sure close label is still visible on the next graph that took this one's place..
		while (++num < (int)graphs.size()) {
			if (!hiddenGraphs.testBit(num)) {
				mouseOverGraphInWindowCoords(graphs[num], x, y);
				break;
			}
		}
	}
}

void FileViewerWindow::showAllGraphs()
{
	closeLbl->hide();
	const int n = graphs.size();
	for (int i = 0; i < n; ++i) {
		hiddenGraphs.clearBit(i);
		graphHideUnhideActions[i]->setChecked(true);	
	}
	layoutGraphs();
	updateData();
	for (int i = 0; i < n; ++i) 
		graphs[i]->show();
}

void FileViewerWindow::hideUnhideGraphSlot()
{
	QAction *s = dynamic_cast<QAction *>(sender());
	if (!s) return;
	bool ok = false;
	const int num = s->objectName().toInt(&ok);
	if (!ok) return;
	if (num >= 0 && num < graphs.size()) {
		if (hiddenGraphs.testBit(num)) {
			// unhide			
			showGraph(num);
		} else {
			if (hiddenGraphs.count(true) < graphs.size()-1) // if last graph, can't close
				// hide
				hideGraph(num);
		}
	}
}

void FileViewerWindow::hideCloseTimeout()
{
	QPoint p = QCursor::pos();
	QRect r = closeLbl->geometry();
	r.moveTopLeft(closeLbl->mapToGlobal(QPoint(0,0)));
	if (r.contains(p)) {
		// still inside the close but, do nothing
	} else {
		closeLbl->hide();
		hideCloseTimer->stop();
	}
}

void FileViewerWindow::viewModeMenuSlot()
{
	QAction *a = dynamic_cast<QAction *>(sender());
	if (!a) return;
	for (int i = 0; i < (int)N_ViewMode; ++i) {
		if (a == viewModeActions[i]) {
			viewMode = (ViewMode)i;
			saveSettings();
			layoutGraphs();
			QTimer::singleShot(10, this, SLOT(updateIt()));
			return;
		}
	}
}

void FileViewerWindow::resizeEvent(QResizeEvent *r)
{
	QMainWindow::resizeEvent(r);
	layoutGraphs();
	QTimer::singleShot(10, this, SLOT(updateIt()));
}

void FileViewerWindow::resizeIt()
{
	resize(1024,640);	
}

void FileViewerWindow::updateIt()
{
	updateData();	
}

void FileViewerWindow::showEvent(QShowEvent *e)
{
	QMainWindow::showEvent(e);
	updateGeometry();
	QTimer::singleShot(10, this, SLOT(resizeIt()));
}
