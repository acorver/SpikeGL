#include "SpatialVisWindow.h"
#include <QVBoxLayout>
#include <QFrame>
#include <QTimer>
#include <math.h>
#include <QLabel>
#include <QStatusBar>
#include <QPushButton>
#include <QToolBar>
#include <QPainter>
#include <QPixmap>
#include <QIcon>
#include <QSettings>
#include <QColorDialog>
#include <QKeyEvent>
#include <QSpinBox>
#include <QSlider>
#include <QMatrix>
#include <QCheckBox>
#include "StimGL_SpikeGL_Integration.h"

#include "no_data.xpm"

#define SETTINGS_GROUP "SpatialVisWindow Settings"
#define GlyphScaleFactor 0.9725 /**< set this to less than 1 to give each glyph a margin */
#define BlockScaleFactor 1.0 /**< set this to less than 1 to give blocks a margin */

SpatialVisWindow::SpatialVisWindow(DAQ::Params & params, const Vec2 & blockDims, QWidget * parent)
: QMainWindow(parent), params(params), nvai(params.nVAIChans), nextra(params.nExtraChans1+params.nExtraChans2), 
  graph(0), graphFrame(0), mouseOverChan(-1), last_fs_frame_num(0xffffffff)
{
	static bool registeredMetaType = false;
		
	if (fshare.shm) {
		Log() << "SpatialVisWindow: " << (fshare.createdByThisInstance ? "Created" : "Attatched to pre-existing") <<  " StimGL 'frame share' memory segment, size: " << (double(fshare.size())/1024.0/1024.0) << "MB.";
		fshare.lock();
		fshare.shm->enabled = 0;
		ovlSetNoData();
		fshare.unlock();
	} else {
		Error() << "INTERNAL ERROR: Could not attach to StimGL 'frame share' shared memory segment! FIXME!";
	}
	
	if (!registeredMetaType) {
		qRegisterMetaType<QVector<uint>	>("QVector<uint>");
		registeredMetaType = true;
	}

	setWindowTitle("Spatial Visualization");
	resize(800,600);

	toolBar = addToolBar("Spatial Visualization Controls");
	
	QLabel *label;
	//toolBar->addWidget(label = new QLabel("Color: ", toolBar));
	//toolBar->addWidget(colorBut = new QPushButton(toolBar));
	
	//toolBar->addSeparator();
	
	toolBar->addWidget(label = new QLabel("Layout: ", toolBar));
	toolBar->addWidget(sbCols = new QSpinBox(toolBar));
	toolBar->addWidget(new QLabel("x", toolBar));
	toolBar->addWidget(sbRows = new QSpinBox(toolBar));

	toolBar->addSeparator();
	
	toolBar->addWidget(overlayChk = new QCheckBox("StimGL Overlay:", toolBar));
	toolBar->addWidget(overlayAlpha = new QSlider(Qt::Horizontal, toolBar));
	QSize sz = overlayAlpha->maximumSize(); overlayAlpha->setMaximumSize(QSize(125,sz.height()));
	overlayAlpha->setRange(0,100);
	overlayAlpha->setSingleStep(1);
	overlayAlpha->setPageStep(10);
	overlayAlpha->setValue(50);
	
	overlayChk->setEnabled(!!fshare.shm);
	overlayAlpha->setEnabled(!!fshare.shm);
	
	Connect(overlayChk, SIGNAL(toggled(bool)), this, SLOT(overlayChecked(bool)));
	Connect(overlayAlpha, SIGNAL(valueChanged(int)), this, SLOT(overlayAlphaChanged(int)));

	toolBar->addWidget(overlayBut = new QPushButton("Set Overlay Box...",toolBar));
	QFont f = overlayBut->font();
	f.setPointSize(10);
	overlayBut->setFont(f);
	Connect(overlayBut, SIGNAL(clicked(bool)), this, SLOT(overlayButPushed()));
	//Connect(colorBut, SIGNAL(clicked(bool)), this, SLOT(colorButPressed()));
		
	nGraphsPerBlock = blockDims.x * blockDims.y;
	nblks = (nvai / nGraphsPerBlock) + (nvai%nGraphsPerBlock?1:0);
    nbx = floor(sqrtf(static_cast<float>(nblks))+0.5), nby = 0;
	if (nbx <= 0) nbx = 1;
	while (nbx*nby < nblks) ++nby;
	
	blocknx = blockDims.x;
	blockny = blockDims.y;
	//Debug() << " nvai=" << nvai << " nGraphsPerBlock=" << nGraphsPerBlock << " nblks=" << nblks << " nbx=" << nbx << " nby=" << nby << " blkdims=" << blocknx << "," << blockny;
	
	points.resize(nvai);
	colors.resize(nvai);
	chanVolts.resize(nvai);
	
	for (int chanid = 0; chanid < nvai; ++chanid) {
		points[chanid] = chanId2Pos(chanid);
	}

	sbRows->setValue(nby);
	sbCols->setValue(nbx);
	sbRows->setRange(1,100);
	sbCols->setRange(1,100);
	Connect(sbCols, SIGNAL(valueChanged(int)), this, SLOT(blockLayoutChanged()));
	Connect(sbRows, SIGNAL(valueChanged(int)), this, SLOT(blockLayoutChanged()));
	
	graphFrame = new QFrame(this);
	QVBoxLayout *bl = new QVBoxLayout(graphFrame);
    bl->setSpacing(0);
	bl->setContentsMargins(0,0,0,0);
	graph = new GLSpatialVis(graphFrame);
	bl->addWidget(graph,1);	
	setCentralWidget(graphFrame);

	setupGridlines();
	
	fg = QColor(0x87, 0xce, 0xfa, 0x7f);
	fg2 = QColor(0xfa, 0x87, 0x37, 0x7f);
	QColor bg, grid;
	bg.setRgbF(.15,.15,.15);
	grid.setRgbF(0.4,0.4,0.4);			
	
	graph->setBGColor(bg);
	graph->setGridColor(grid);
		
	QTimer *t;
	
	t = new QTimer(this);
	Connect(t, SIGNAL(timeout()), this, SLOT(ovlUpdate()));
	t->setSingleShot(false);
	t->start(1000.0/60.0);
		
	Connect(graph, SIGNAL(cursorOver(double, double)), this, SLOT(mouseOverGraph(double, double)));
	Connect(graph, SIGNAL(clicked(double, double)), this, SLOT(mouseClickGraph(double, double)));
	Connect(graph, SIGNAL(clickReleased(double, double)), this, SLOT(mouseReleaseGraph(double, double)));
	Connect(graph, SIGNAL(doubleClicked(double, double)), this, SLOT(mouseDoubleClickGraph(double, double)));
	
	QStatusBar *sb = statusBar();
	sb->addWidget(statusLabel = new QLabel(sb),1);
	
	t = new QTimer(this);
    Connect(t, SIGNAL(timeout()), this, SLOT(updateGraph()));
    t->setSingleShot(false);
    t->start(1000/DEF_TASK_READ_FREQ_HZ);        
	
    t = new QTimer(this);
    Connect(t, SIGNAL(timeout()), this, SLOT(updateMouseOver()));
    t->setSingleShot(false);
    t->start(1000/DEF_TASK_READ_FREQ_HZ);
	
	graph->setMouseTracking(true);	
	
	graph->setGlyphType(GLSpatialVis::Square);
	graph->setPoints(points); // setup graph points
	
	selClear();
	
	loadSettings();
	updateToolBar();
	
	graph->setAutoUpdate(false);
}

void SpatialVisWindow::setupGridlines()
{
	graph->setNumVGridLines(nbx);
	graph->setNumHGridLines(nby);
}

void SpatialVisWindow::resizeEvent (QResizeEvent * event)
{
	updateGlyphSize();
	QMainWindow::resizeEvent(event);
}

void SpatialVisWindow::updateGlyphSize()
{
	if (!graph) return;
	Vec4 br = blockBoundingRect(0);
	Vec2 bs = Vec2(br.v3-br.v1, br.v2-br.v4);
	int szx = bs.x/blocknx * graph->width();
	int szy = bs.y/blockny * graph->height();
	szx *= GlyphScaleFactor, szy *= GlyphScaleFactor;
	if (szx < 1) szx = 1;
	if (szy < 1) szy = 1;
	graph->setGlyphSize(Vec2f(szx,szy));		
}

void SpatialVisWindow::putScans(const std::vector<int16> & scans, u64 firstSamp)
{
	(void)firstSamp; // unused warning
	int firstidx = scans.size() - nvai;
	if (firstidx < 0) firstidx = 0;
	for (int i = firstidx; i < int(scans.size()); ++i) {
		int chanid = i % nvai;
		const QColor color (chanid < nvai-nextra ? fg : fg2);
		double val = (double(scans[i])+32768.) / 65535.;
		chanVolts[chanid] = val * (params.range.max-params.range.min)+params.range.min;
		colors[chanid].x = color.redF()*val;
		colors[chanid].y = color.greenF()*val;
		colors[chanid].z = color.blueF()*val;
		colors[chanid].w = color.alphaF();
	}
//	updateGraph();
}

SpatialVisWindow::~SpatialVisWindow()
{
	delete graphFrame, graphFrame = 0;
	graph = 0;
	if (fshare.shm) {
		if (fshare.lock()) {
			fshare.shm->enabled = 0;
			fshare.unlock();
		}
		fshare.detach();
	}
}

void SpatialVisWindow::updateGraph()
{
	if (!graph) return;
	updateGlyphSize();
	graph->setColors(colors);
	if (graph->needsUpdateGL())
		graph->updateGL();
	
	if (fshare.shm) {
		bool en = overlayBut->isEnabled();
		if (en && !fshare.shm->stimgl_pid) {
			overlayBut->setEnabled(false);
			ovlSetNoData();
		}
		else if (!en && fshare.shm->stimgl_pid) overlayBut->setEnabled(true);
	}
}

void SpatialVisWindow::selClear() { 
	selIdxs.clear(); 	
}

void SpatialVisWindow::mouseOverGraph(double x, double y)
{
	mouseOverChan = -1;
	int chanId = pos2ChanId(x,y);
	if (chanId < 0 || chanId >= (int)params.nVAIChans) 
		mouseOverChan = -1;
	else
		mouseOverChan = chanId;
	updateMouseOver();
}

void SpatialVisWindow::selectBlock(int blk)
{
	const QVector<unsigned> oldIdxs(selIdxs);
	selClear();
	Vec4 r = blockBoundingRectNoMargins(blk);
	selIdxs.reserve(nGraphsPerBlock);
	for (int i = 0, ch = 0; i < nGraphsPerBlock && ((ch=i + blk*nGraphsPerBlock) < nvai); ++i) {
		selIdxs.push_back(ch);
	}
	if (blk >= 0 && blk < nblks) {
		graph->setSelectionRange(r.v1,r.v3,r.v2,r.v4, GLSpatialVis::Outline);
		graph->setSelectionEnabled(true, GLSpatialVis::Outline);
	} else {
		// not normally reached unless we have "blank" blocks at the end.....
		graph->setSelectionEnabled(false, GLSpatialVis::Outline);
	}
	if (oldIdxs.size() != selIdxs.size() 
		|| (oldIdxs.size() && selIdxs.size() && oldIdxs[0] != selIdxs[0]))
		emit channelsSelected(selIdxs);	
}

void SpatialVisWindow::mouseClickGraph(double x, double y)
{
	int chanId = pos2ChanId(x,y);
	int blk = chanId / nGraphsPerBlock;
	selectBlock(blk);
	emit channelsSelected(selIdxs);
}


void SpatialVisWindow::mouseReleaseGraph(double x, double y)
{ 
	(void)x; (void)y;
}

void SpatialVisWindow::mouseDoubleClickGraph(double x, double y)
{
	(void)x; (void)y;
	emit channelsOpened(selIdxs);
}

void SpatialVisWindow::updateMouseOver() // called periodically every 1s
{
	if (!statusLabel) return;
	const int chanId = mouseOverChan;
	if (chanId < 0 || chanId >= chanVolts.size()) 
		statusLabel->setText("");
	else
		statusLabel->setText(QString("Chan: #%3 -- Volts: %4 V")
							 .arg(chanId)
							 .arg(chanId < (int)chanVolts.size() ? chanVolts[chanId] : 0.0));
	
	if (selIdxs.size()) {
		QString t = statusLabel->text();
		if (t.length()) t = QString("(mouse at: %1)").arg(t);
		statusLabel->setText(QString("Selected channels %1-%2 of %3, page %4/%5. %6").arg(selIdxs.first()).arg(selIdxs.last()).arg(nvai).arg(selIdxs.first()/nGraphsPerBlock + 1).arg(nblks).arg(t));
	}
}

Vec4 SpatialVisWindow::blockBoundingRectNoMargins(int blk) const
{
	Vec4 ret;
	// top left
	ret.v1 = (1.0/nbx) * (blk % nbx);
	ret.v2 = 1.0 - (1.0/nby) * (blk / nbx);
	// bottom right
	ret.v3 = ret.v1 + (1.0/nbx);
	ret.v4 = ret.v2 - (1.0/nby);
	return ret;
}


Vec2 SpatialVisWindow::blockMargins() const 
{
	return Vec2(1.0/nbx - (1.0/nbx * BlockScaleFactor), 1.0/nby - (1.0/nby * BlockScaleFactor)); 
}

Vec4 SpatialVisWindow::blockBoundingRect(int blk) const
{
	Vec4 ret (blockBoundingRectNoMargins(blk));
	Vec2 blkmrg(blockMargins());
	ret.v1+=blkmrg.x;
	ret.v3-=blkmrg.x;
	ret.v2-=blkmrg.y;
	ret.v4+=blkmrg.y;
	return ret;
}

Vec2 SpatialVisWindow::chanId2Pos(const int chanid) const
{
/*	Vec2 ret;
	const int col = chanid % nx, row = chanid / nx;
	ret.x = (col/double(nx)) + (1./(nx*2.));
	ret.y = (row/double(ny)) + (1./(ny*2.));
	return ret;*/
	const int blk = chanid / nGraphsPerBlock;
	Vec4 r(blockBoundingRect(blk));
	int ch = chanid % nGraphsPerBlock;
	const int col = ch % blocknx, row = ch / blocknx;
	double cellw = (r.v3-r.v1)/blocknx, cellh = (r.v2-r.v4)/blockny; 
	return Vec2(
				r.v1 + ((cellw * col) + cellw/2.0),
		        r.v4 + ((cellh * (blockny-row)) - cellh/2.0) 
		);
}

int SpatialVisWindow::pos2ChanId(double x, double y) const
{
/*	int col = x*nx, row = y*ny;
	return col + row*nx;
*/
	
	int blkcol = x*nbx, blkrow = (1.0-y)*nby;
	int blk = (blkrow * nbx) + blkcol;
	Vec4 r(blockBoundingRect(blk));
	Vec2 offset(x-r.v1, r.v2-y); // transformed for 0,0 is top left
	double cellw = (r.v3-r.v1)/blocknx, cellh = (r.v2-r.v4)/blockny; 
	int col = (offset.x/cellw), row = (offset.y/cellh);
	return blk*nGraphsPerBlock + (row*blocknx + col);
}

void SpatialVisWindow::updateToolBar()
{
/*	{ // update color button
        QPixmap pm(22,22);
        QPainter p;
        p.begin(&pm);
        p.fillRect(0,0,22,22,QBrush(fg));
        p.end();
        colorBut->setIcon(QIcon(pm));
    }
 */
}

void SpatialVisWindow::colorButPressed()
{
    QColorDialog::setCustomColor(0,fg.rgba());
    QColorDialog::setCustomColor(1,fg2.rgba());
    QColor c = QColorDialog::getColor(fg, this);
    if (c.isValid()) {
		fg = c;
        updateToolBar();
		saveSettings();
    }	
}

void SpatialVisWindow::saveSettings()
{
	QSettings settings(SETTINGS_DOMAIN, SETTINGS_APP);
	settings.beginGroup(SETTINGS_GROUP);

	settings.setValue("fgcolor1", static_cast<unsigned int>(fg.rgba())); 
	settings.setValue(QString("layout%1cols").arg(nblks), nbx);
	settings.setValue(QString("layout%1rows").arg(nblks), nby);
	settings.setValue(QString("UseStimGLOverlay"), overlayChk->isChecked());
	
	settings.endGroup();
}

void SpatialVisWindow::loadSettings()
{
	QSettings settings(SETTINGS_DOMAIN, SETTINGS_APP);
	settings.beginGroup(SETTINGS_GROUP);

	fg = QColor::fromRgba(settings.value("fgcolor1", static_cast<unsigned int>(fg.rgba())).toUInt());
	int sbcols = settings.value(QString("layout%1cols").arg(nblks), nbx).toInt();
	int sbrows = settings.value(QString("layout%1rows").arg(nblks), nby).toInt();
	if (sbrows > 0 && sbcols > 0 && sbrows <= 100 && sbcols <= 100 && sbrows * sbcols >= nblks) {
		sbRows->setValue(sbrows);
		sbCols->setValue(sbcols);
		if (nbx != sbcols || nby != sbrows)
			blockLayoutChanged();
	}
	overlayChecked(settings.value(QString("UseStimGLOverlay"), false).toBool());
	
	settings.endGroup();
}

Vec2 SpatialVisWindow::glyphMargins01Coords() const 
{
	Vec2 ret(graph->glyphSize().x/GlyphScaleFactor, graph->glyphSize().y/GlyphScaleFactor);
	ret.x = (ret.x - graph->glyphSize().x) / double(graph->width()) / 2.0;
	ret.y = (ret.y - graph->glyphSize().y) / double(graph->height()) / 2.0;
	return ret;
}

void SpatialVisWindow::keyPressEvent(QKeyEvent *e)
{
	if (e->key() == Qt::Key_Return && selIdxs.size() > 0) {
		e->accept();
		emit channelsOpened(selIdxs);
		return;
	}
	QMainWindow::keyPressEvent(e);
}

void SpatialVisWindow::blockLayoutChanged()
{
	int sbrows = sbRows->value(), sbcols = sbCols->value();
	if (sbrows * sbcols >= nblks) {
		nbx = sbcols;
		nby = sbrows;
		
		for (int chanid = 0; chanid < nvai; ++chanid) {
			points[chanid] = chanId2Pos(chanid);
		}
		setupGridlines();
		graph->setPoints(points);
		if (selIdxs.size()) {
			int blk = selIdxs[0] / nGraphsPerBlock;
			selectBlock(blk);
		}
		saveSettings();
	}
}

void SpatialVisWindow::overlayChecked(bool chk)
{
	overlayAlpha->setEnabled(chk);
	overlayChk->blockSignals(true);
	overlayChk->setChecked(chk);
	overlayAlphaChanged(chk ? overlayAlpha->value() : 0);
	if (fshare.shm && fshare.lock()) {
		fshare.shm->enabled = chk ? 1 : 0;
		overlayBut->setEnabled(chk && fshare.shm->stimgl_pid);
		fshare.unlock();
	}
	overlayChk->blockSignals(false);
//	saveSettings(); // uncomment if you want to save the state of this in the settings...
}

void SpatialVisWindow::overlayAlphaChanged(int v)
{
	graph->setOverlayAlpha(v / 100.0f);
}

void SpatialVisWindow::ovlUpdate()
{
	GLuint frameNum = last_fs_frame_num;
	if (overlayAlpha->value() && overlayAlpha->isEnabled() && fshare.shm) {
		graph->setOverlay((void *)fshare.shm->data, fshare.shm->w, fshare.shm->h, fshare.shm->fmt);
		frameNum = fshare.shm->frame_num;
	}
	if (frameNum != last_fs_frame_num && graph->needsUpdateGL()) {
		graph->updateGL();
	}
	last_fs_frame_num = frameNum;
}

void SpatialVisWindow::ovlSetNoData()
{
	QImage nodata(no_data_xpm);
	nodata = QGLWidget::convertToGLFormat(nodata);
	if (nodata.isNull()) {
		Error() << "could not convert nodata QImage to GL format";
	} else {
		fshare.shm->fmt = GL_RGBA;
		fshare.shm->w = nodata.width();
		fshare.shm->h = nodata.height();
		fshare.shm->sz_bytes = nodata.byteCount();
		memcpy((void *)fshare.shm->data, nodata.bits(), nodata.byteCount() < fshare.size() ? nodata.byteCount() : fshare.size());
	}
}

void SpatialVisWindow::closeEvent(QCloseEvent *e)
{
	overlayChecked(false); // force overlay to uncheck when closing window..
	QMainWindow::closeEvent(e);
}

void SpatialVisWindow::overlayButPushed()
{
	if (fshare.shm) {
		fshare.lock();
		fshare.shm->do_box_select = 1;
		fshare.unlock();
	}
}