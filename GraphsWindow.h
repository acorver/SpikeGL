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
class QFrame;
class QLineEdit;

class GraphsWindow : public QMainWindow
{
    Q_OBJECT
public:
    GraphsWindow(const DAQ::Params & params, QWidget *parent = 0);
    ~GraphsWindow();

    void putScans(std::vector<int16> & scans, u64 firstSamp);

    // clear a specific graph's points, or all if negative
    void clearGraph(int which = -1);

private slots:
    void updateGraphs();
    void downsampleChk(bool checked);
    void pauseGraph();
    void toggleMaximize();
    void selectGraph(int num);
    void graphSecsEdited(const QString &);
    void graphYScaleEdited(const QString &);
    void applyAll();

    void mouseOverGraph(double x, double y);
    void mouseClickGraph(double x, double y);
    void mouseDoubleClickGraph(double x, double y);

private:
    void setGraphTimeSecs(int graphnum, double t); // note you should call update_nPtsAllGs after this!  (Not auto-called in this function just in case of batch setGraphTimeSecs() in which case 1 call at end to update_nPtsAllGs() suffices.)
    void update_nPtsAllGs();
    
    void updateGraphCtls();
    
    DAQ::Params params;
    QWidget *graphsWidget;
    QToolBar *graphCtls;
    QLCDNumber *chanLCD;
    QLineEdit *graphYScale, *graphSecs;
    QVector<Vec2WrapBuffer> points;
    QVector<GLGraph *> graphs;
    QVector<QFrame *> graphFrames;
    QVector<bool> pausedGraphs;
    QVector<double> graphTimesSecs;
    QVector<i64> nptsAll;
    i64 nPtsAllGs; ///< sum of each element of nptsAll array above..
    double downsampleRatio, tNow, tLast, tAvg, tNum;
    int pdChan;
    QAction *pauseAct, *maxAct, *applyAllAct;
    GLGraph *maximized; ///< if not null, a graph is maximized 
};


#endif
