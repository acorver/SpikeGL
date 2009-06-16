#ifndef GraphsWindow_H
#define GraphsWindow_H

#include <QMainWindow>
#include "DAQ.h"
#include "GLGraph.h"
#include "TypeDefs.h"
#include "Vec2WrapBuffer.h"
#include <QVector>
#include <vector>
#include "ChanMappingController.h"

class QToolBar;
class QLabel;
class PointProcThread;
class QAction;
class QFrame;
class QDoubleSpinBox;
class QCheckBox;
class HPFilter;

class GraphsWindow : public QMainWindow
{
    Q_OBJECT
public:
    GraphsWindow(DAQ::Params & params, QWidget *parent = 0, bool isSaving = true);
    ~GraphsWindow();

    void putScans(std::vector<int16> & scans, u64 firstSamp);

    // clear a specific graph's points, or all if negative
    void clearGraph(int which = -1);

    // overrides parent -- applies event filtering to the doublespinboxes as well!
    void installEventFilter(QObject * filterObj);
    
    void setToggleSaveChkBox(bool b);
    void setToggleSaveLE(const QString & fname);

private slots:
    void updateGraphs();
    void downsampleChk(bool checked);
    void hpfChk(bool checked);
    void pauseGraph();
    void toggleMaximize();
    void selectGraph(int num);
    void graphSecsChanged(double d);
    void graphYScaleChanged(double d);
    void applyAll();

    void mouseOverGraph(double x, double y);
    void mouseClickGraph(double x, double y);
    void mouseDoubleClickGraph(double x, double y);
    void updateMouseOver(); // called periodically every 1s
    void doGraphColorDialog();
    void toggleSaveChecked(bool b);

private:
    void setGraphTimeSecs(int graphnum, double t); // note you should call update_nPtsAllGs after this!  (Not auto-called in this function just in case of batch setGraphTimeSecs() in which case 1 call at end to update_nPtsAllGs() suffices.)
    void update_nPtsAllGs();
    
    void updateGraphCtls();
    void doPauseUnpause(int num, bool updateCtls = true);
    void computeGraphMouseOverVars(unsigned num, double & y,
                                   double & mean, double & stdev,
                                   const char * & unit);
    static int parseGraphNum(QObject *gl_graph_instance);
    bool isAuxChan(unsigned num) const;
    
    DAQ::Params & params;
    QWidget *graphsWidget;
    QToolBar *graphCtls;
    QLabel *chanLbl;
    QDoubleSpinBox *graphYScale, *graphSecs;
    QCheckBox *highPassChk, *toggleSaveChk;
    QLineEdit *saveFileLE;
    QPushButton *graphColorBut;
    QVector<Vec2WrapBuffer> points;
    QVector<GLGraph *> graphs;
    QVector<QFrame *> graphFrames;
    QVector<bool> pausedGraphs;
    QVector<double> graphTimesSecs;
    struct GraphStats {
        double s1; ///< sum of values
        double s2; ///< sum of squares of values
        unsigned num; ///< total number of values
        GraphStats()  { clear(); }
        void clear() { s1 = s2 = num = 0; }
        double mean() const { return s1/num; }
        double stdDev() const;
    };
    QVector<GraphStats> graphStats; ///< mean/stddev stuff
    QVector<i64> nptsAll;
    i64 nPtsAllGs; ///< sum of each element of nptsAll array above..
    double downsampleRatio, tNow, tLast, tAvg, tNum;
    int pdChan, firstExtraChan;
    QAction *pauseAct, *maxAct, *applyAllAct;
    GLGraph *maximized; ///< if not null, a graph is maximized 
    HPFilter *filter;
    Vec2 lastMousePos;
    int lastMouseOverGraph;
    int selectedGraph;
};


#endif
