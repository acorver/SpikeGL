#ifndef GraphsWindow_H
#define GraphsWindow_H

#include <QMainWindow>
#include "DAQ.h"
#include "GLGraph.h"
#include "TypeDefs.h"
#include "Vec2WrapBuffer.h"
#include <QVector>
#include <vector>

class QToolBar;
class QLCDNumber;
class PointProcThread;
class QAction;

class GraphsWindow : public QMainWindow
{
    Q_OBJECT
public:
    GraphsWindow(const DAQ::Params & params, QWidget *parent = 0);
    ~GraphsWindow();

    void putScans(std::vector<int16> & scans, u64 firstSamp);

    void setGraphTimeSecs(double t);

    // clear a specific graph's points, or all if negative
    void clearGraph(int which = -1);

private slots:
    void updateGraphs();
    void downsampleChk(bool checked);
    void pauseGraph();
    void toggleMaximize();

    void mouseOverGraph(double x, double y);
    void mouseClickGraph(double x, double y);
    void mouseDoubleClickGraph(double x, double y);

private:
    void updateGraphCtls();
    
    DAQ::Params params;
    QWidget *graphsWidget;
    QToolBar *graphCtls;
    QLCDNumber *chanLCD;
    QVector<Vec2WrapBuffer> points;
    QVector<GLGraph *> graphs;
    QVector<bool> pausedGraphs;

    double downsampleRatio, graphTimeSecs, tNow, tLast, tAvg, tNum;
    i64 npts;
    QAction *pauseAct, *maxAct;
    GLGraph *maximized; ///< if not null, a graph is maximized 
};


#endif
