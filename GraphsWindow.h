#ifndef GraphsWindow_H
#define GraphsWindow_H

#include <QMainWindow>
#include "DAQ.h"
#include "GLGraph.h"
#include <vector>
#include "TypeDefs.h"

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

    void setGraphTimeSecs(double t);

private slots:
    void updateGraphs();
    void downsampleChk(bool checked);
private:
    DAQ::Params params;
    QWidget *graphsWidget;
    QToolBar *graphCtls;
    QLCDNumber *chanLCD;
    std::vector<std::vector<Vec2> > points;
    std::vector<unsigned> pointsP0;
    std::vector<GLGraph *> graphs;
    double downsampleRatio, graphTimeSecs, tNow, tLast, tAvg, tNum;
    i64 npts, nptsTotal;
};


#endif
