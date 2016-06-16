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
#include <QMutex>

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
    void putScans(const int16 *scans, unsigned scans_size_samps, u64 firstSamp);
		
    bool threadsafeIsVisible() const { return threadsafe_is_visible; }

    void setStaticBlockLayout(int nrows, int ncols);

    QColor & glyphColor1() { return fg; }
    QColor & glyphColor2() { return fg2; }

public slots:
	void selectBlock(int tabNum);
    void setSorting(const QVector<int> & sorting, const QVector<int> & naming);

signals:
	void channelsSelected(const QVector<unsigned> & ids);
	void channelsOpened(const QVector<unsigned> & ids);
	
protected:
	// virtual from parent class
	void resizeEvent(QResizeEvent *event);
	void keyPressEvent(QKeyEvent *event);
    void closeEvent(QCloseEvent *);
    void showEvent(QShowEvent *);
    void hideEvent(QHideEvent *);

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
	void setOverlayAlpha(int);
	void ovlUpdate();
	void overlayButPushed();
	void ovlFFChecked(bool);
	void ovlAlphaChanged(int);
	void ovlFpsChanged(int);
    void unsignedChecked(bool);
	
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
	void ovlSetNoData();
	
    volatile bool threadsafe_is_visible;

    DAQ::Params & params;
	const int nvai, nextra;
	int nblks, nbx, nby, nGraphsPerBlock, blocknx, blockny;
    bool treatDataAsUnsigned;
    QVector<Vec2> points;
    QVector<Vec4f> colors;
	QVector<double> chanVolts;
    QVector<int16> chanRawSamps;
    GLSpatialVis * graph;
    QFrame * graphFrame;
	QColor fg, fg2;
	QLabel *statusLabel;
	int mouseOverChan;
	QVector<unsigned> selIdxs;
	
	QToolBar *toolBar;
    QPushButton *colorBut;
	QSpinBox *sbCols, *sbRows;
    QCheckBox *overlayChk, *ovlFFChk, *unsignedChk;
	QLabel *ovlAlphaLbl;
	QPushButton *overlayBut;
	QSlider *overlayAlpha;
	QLabel *ovlfpsTit, *ovlfpsLimit;
	QSlider *ovlFps;
		
	StimGL_SpikeGL_Integration::FrameShare fshare;
	GLuint last_fs_frame_num;
	quint64 last_fs_frame_tsc;
	Avg frameDelayAvg;
	QString fdelayStr;
    QVector<int> sorting, revsorting, naming;

    QMutex mut;    
};


#endif
