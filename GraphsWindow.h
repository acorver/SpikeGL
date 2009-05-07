#ifndef GraphsWindow_H
#define GraphsWindow_H

#include <QMainWindow>
#include "DAQ.h"
#include "GLGraph.h"
#include <vector>
#include "SampleBufQ.h"
#include <QMutex>

class QToolBar;
class QLCDNumber;
class PointProcThread;

class GraphsWindow : public QMainWindow
{
    Q_OBJECT
public:
    GraphsWindow(const DAQ::Params & params, QWidget *parent = 0);
    ~GraphsWindow();

    void putScans(std::vector<int16> & scans, u64 firstSamp);

private slots:
    void updateGraphs();
    void downsampleChk(bool checked);

private:
    DAQ::Params params;
    QWidget *graphsWidget;
    QToolBar *graphCtls;
    QLCDNumber *chanLCD;
    PointProcThread *pointProcThread;
    std::vector<std::vector<Vec2> > points;
    std::vector<GLGraph *> graphs;
    QMutex graphMut;
    volatile double downsampleRatio, graphTimeSecs, tNow, tLast, tAvg, tNum;
    friend class PointProcThread;
};


#endif
