#ifndef SpatialVisWindow_H
#define SpatialVisWindow_H

#include <QMainWindow>
#include "DAQ.h"
#include "GLSpatialVis.h"
#include "TypeDefs.h"
#include "VecWrapBuffer.h"
#include <QVector>
#include <vector>
#include <QSet>
#include <QColor.h>
#include "StimGL_SpikeGL_Integration.h"

class QToolBar;
class QLabel;
class QAction;
class QFrame;
class QPushButton;
class QSpinBox;
class QSlider;
class QCheckBox;

class SpatialVisWindow : public QMainWindow
{
    Q_OBJECT
public:
    SpatialVisWindow(DAQ::Params & params, const Vec2 & blockDims, QWidget *parent = 0);
    ~SpatialVisWindow();
	
    void putScans(const std::vector<int16> & scans, u64 firstSamp);
		
public slots:
	void selectBlock(int tabNum);

signals:
	void channelsSelected(const QVector<unsigned> & ids);
	void channelsOpened(const QVector<unsigned> & ids);
	
protected:
	// virtual from parent class
	void resizeEvent(QResizeEvent *event);
	void keyPressEvent(QKeyEvent *event);
	
private slots:
    void updateGraph();
//    void selectGraph(int num);
	
    void mouseOverGraph(double x, double y);
    void mouseClickGraph(double x, double y);
    void mouseReleaseGraph(double x, double y);
    void mouseDoubleClickGraph(double x, double y);
    void updateMouseOver(); // called periodically every 1s
	void updateToolBar();
	void colorButPressed();
	void blockLayoutChanged();
	void overlayChecked(bool);
	void overlayAlphaChanged(int);
	void ovlUpdate();
	
private:	
	int pos2ChanId(double x, double y) const;
	Vec2 chanId2Pos(const int chanid) const;
	Vec4 blockBoundingRect(int block) const;
	Vec4 blockBoundingRectNoMargins(int block) const;
	Vec2 blockMargins() const;
	void updateGlyphSize();
	void selClear();
	
	void setupGridlines();
	
	void saveSettings();
	void loadSettings();
	Vec2 glyphMargins01Coords() const;
	
    DAQ::Params & params;
	const int nvai, nextra;
	int nblks, nbx, nby, nGraphsPerBlock, blocknx, blockny;
    QVector<Vec2> points;
    QVector<Vec4f> colors;
	QVector<double> chanVolts;
    GLSpatialVis * graph;
    QFrame * graphFrame;
	QColor fg, fg2;
	QLabel *statusLabel;
	int mouseOverChan;
	QVector<unsigned> selIdxs;
	
	QToolBar *toolBar;
//	QPushButton *colorBut;
	QSpinBox *sbCols, *sbRows;
	QCheckBox *overlayChk;
	QSlider *overlayAlpha;
		
	StimGL_SpikeGL_Integration::FrameShare fshare;
};


#endif
