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

class QToolBar;
class QLabel;
class QAction;
class QFrame;
class QPushButton;

class SpatialVisWindow : public QMainWindow
{
    Q_OBJECT
public:
    SpatialVisWindow(DAQ::Params & params, QWidget *parent = 0);
    ~SpatialVisWindow();
	
    void putScans(const std::vector<int16> & scans, u64 firstSamp);
		
    // overrides parent -- applies event filtering to the doublespinboxes as well!
//    void installEventFilter(QObject * filterObj);
    
private slots:
    void updateGraph();
//    void selectGraph(int num);
	
    void mouseOverGraph(double x, double y);
    void mouseClickGraph(double x, double y);
    void mouseDoubleClickGraph(double x, double y);
    void updateMouseOver(); // called periodically every 1s
	
private:	
	int pos2ChanId(double x, double y) const;
    DAQ::Params & params;
    QVector<Vec2> points;
    QVector<Vec4f> colors;
	QVector<double> chanVolts;
    GLSpatialVis * graph;
    QFrame * graphFrame;
	QColor fg, fg2;
	QLabel *statusLabel;
	int mouseOverChan;
};


#endif
