#include "SpatialVisWindow.h"
#include <QVBoxLayout>
#include <QFrame>
#include <QTimer>
#include <math.h>
#include <QLabel>
#include <QStatusBar>

SpatialVisWindow::SpatialVisWindow(DAQ::Params & params, QWidget * parent)
: QMainWindow(parent), params(params), nvai(params.nVAIChans), nextra(params.nExtraChans1+params.nExtraChans2), 
  graph(0), graphFrame(0), mouseOverChan(-1)
{

	setWindowTitle("Spatial Visualization");
	resize(800,600);

    nx = sqrt(nvai), ny = nx;
	if (nx <= 0) nx = 1;
	while (nx*ny < nvai) ++ny;	

	points.resize(nvai);
	colors.resize(nvai);
	chanVolts.resize(nvai);
	
	for (int chanid = 0; chanid < nvai; ++chanid) {
		points[chanid] = chanId2Pos(chanid);
	}
	
	graphFrame = new QFrame(this);
	QVBoxLayout *bl = new QVBoxLayout(graphFrame);
    bl->setSpacing(0);
	bl->setContentsMargins(0,0,0,0);
	graph = new GLSpatialVis(graphFrame);
	bl->addWidget(graph,1);	
	setCentralWidget(graphFrame);

	if ((int)graph->numVGridLines() != nx) graph->setNumVGridLines(nx);
	if ((int)graph->numHGridLines() != ny) graph->setNumHGridLines(ny);
	
	fg = QColor(0x87, 0xce, 0xfa, 0x7f);
	fg2 = QColor(0xfa, 0x87, 0x37, 0x7f);
	QColor bg, grid;
	bg.setRgbF(.15,.15,.15);
	grid.setRgbF(0.4,0.4,0.4);			
	
	graph->setBGColor(bg);
	graph->setGridColor(grid);
		
	Connect(graph, SIGNAL(cursorOver(double, double)), this, SLOT(mouseOverGraph(double, double)));
	Connect(graph, SIGNAL(clicked(double, double)), this, SLOT(mouseClickGraph(double, double)));
	Connect(graph, SIGNAL(doubleClicked(double, double)), this, SLOT(mouseDoubleClickGraph(double, double)));
	
	QStatusBar *sb = statusBar();
	sb->addWidget(statusLabel = new QLabel(sb),1);
	
	QTimer *t = new QTimer(this);
    Connect(t, SIGNAL(timeout()), this, SLOT(updateGraph()));
    t->setSingleShot(false);
    t->start(1000/DEF_TASK_READ_FREQ_HZ);        
	
    t = new QTimer(this);
    Connect(t, SIGNAL(timeout()), this, SLOT(updateMouseOver()));
    t->setSingleShot(false);
    t->start(1000/DEF_TASK_READ_FREQ_HZ);
	
	graph->setMouseTracking(true);		
}

void SpatialVisWindow::resizeEvent (QResizeEvent * event)
{
	updatePointSize();
	QMainWindow::resizeEvent(event);
}

void SpatialVisWindow::updatePointSize()
{
	if (!graph) return;
	int ps = graph->width() / nx;
	if (graph->height() / ny < ps) ps = graph->height()/ny;
	ps *= 0.8;
	if (ps < 1) ps = 1;
	graph->setPointSize(ps);		
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
	updateGraph();
}

SpatialVisWindow::~SpatialVisWindow()
{
	delete graphFrame, graphFrame = 0;
	graph = 0;
}

void SpatialVisWindow::updateGraph()
{
	if (!graph) return;
	updatePointSize();
	graph->setPoints(points,colors);
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

void SpatialVisWindow::mouseClickGraph(double x, double y)
{
	int chanId = pos2ChanId(x,y);
	if (chanId < 0 || chanId >= (int)params.nVAIChans) { statusLabel->setText(""); return; }
	statusLabel->setText(QString("Mouse click %1,%2 -> %3").arg(x).arg(y).arg(chanId));
}

void SpatialVisWindow::mouseDoubleClickGraph(double x, double y)
{
	int chanId = pos2ChanId(x,y);
	if (chanId < 0 || chanId >= (int)params.nVAIChans) { statusLabel->setText(""); return; }
	statusLabel->setText(QString("Mouse dbl click %1,%2 -> %3").arg(x).arg(y).arg(chanId));
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
}

Vec2 SpatialVisWindow::chanId2Pos(const int chanid) const
{
	Vec2 ret;
	const int col = chanid % nx, row = chanid / nx;
	ret.x = (col/double(nx)) + (1./(nx*2.));
	ret.y = (row/double(ny)) + (1./(ny*2.));
	return ret;
}

int SpatialVisWindow::pos2ChanId(double x, double y) const
{
	int col = x*nx, row = y*ny;
	return col + row*nx;
}
