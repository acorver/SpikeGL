#ifndef GraphsWindow_H
#define GraphsWindow_H

#include <QMainWindow>
#include "DAQ.h"
#include "GLGraph.h"
#include "TypeDefs.h"
#include "VecWrapBuffer.h"
#include <QVector>
#include <vector>
#include "ChanMappingController.h"
#include <QSet>
#include <QMutex>

class QToolBar;
class QLabel;
class QAction;
class QFrame;
class QDoubleSpinBox;
class QCheckBox;
class HPFilter;
class QLed;
class QPushButton;
class QTabWidget;
class QTimer;
class QStackedWidget;
class QComboBox;

class GraphsWindow : public QMainWindow
{
    Q_OBJECT
public:
    GraphsWindow(DAQ::Params & params, QWidget *parent = 0, bool isSaving = true);
    ~GraphsWindow();

    bool threadsafeIsVisible() const { return threadsafe_is_visible; }

    void putScans(const std::vector<int16> & scans, u64 firstSamp);
    void putScans(const int16 *data, unsigned data_size_samps, u64 firstSamp);

    // clear a specific graph's points, or all if negative
    void clearGraph(int which = -1);

    // overrides parent -- applies event filtering to the doublespinboxes as well!
    void installEventFilter(QObject * filterObj);
    void removeEventFilter(QObject * filterObj);

    void setToggleSaveChkBox(bool b);
    void setToggleSaveLE(const QString & fname);

    const QLineEdit *saveFileLineEdit() const  { return saveFileLE; }


	void hideUnhideSaveChannelCBs();
	
	void sortGraphsByElectrodeId();
	void sortGraphsByIntan();
	
	int numGraphsPerTab() const { return NUM_GRAPHS_PER_GRAPH_TAB; }
	int numGraphTabs() const { return nGraphTabs; }
	int numColsPerGraphTab() const { return nColsGraphTab; }
	int numRowsPerGraphTab() const { return nRowsGraphTab; }

    const DAQ::Params & daqParams() const { return params; }

    const QVector<int> & currentSorting() const { return sorting; }
    const QVector<int> & currentNaming() const { return naming; }

signals:
	void tabChanged(int tabNum);
    void manualTrig(bool);
    void sortingChanged(const QVector<int> & sorting, const QVector<int> & naming);

public slots:
    void setSGLTrig(bool);
    void setPDTrig(bool);
    void setTrigOverrideEnabled(bool);

    void openGraphsById(const QVector<unsigned> & electrode_ids); ///< creates a "custom" page and opens the first numGraphsPerTab() graphs in the list on that page!
    void setDownsampling(bool checked);
    void setDownsamplingCheckboxEnabled(bool en);

protected:
    void showEvent(QShowEvent *);
    void hideEvent(QHideEvent *);

private slots:
    void updateGraphs();
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

	void saveGraphChecked(bool b);

	void tabChange(int);
    void changeToCustomTab(const QVector<unsigned> & ids_to_place_on_custom_tab);

	void saveFileLineEditChanged(const QString & newtext);
	
	void updateTabsWithHighlights();

    void manualTrigOverrideChanged(bool);
	
private:
    void setGraphTimeSecs(int graphnum, double t); // note you should call update_nPtsAllGs after this!  (Not auto-called in this function just in case of batch setGraphTimeSecs() in which case 1 call at end to update_nPtsAllGs() suffices.)
    void update_nPtsAllGs();
    
    void updateGraphCtls();
    void doPauseUnpause(int num, bool updateCtls = true);
    void computeGraphMouseOverVars(unsigned num, double & y,
                                   double & mean, double & stdev, double & rms,
                                   const char * & unit);
    static int parseGraphNum(QObject *gl_graph_instance);
    void sharedCtor(DAQ::Params & p, bool isSaving);

	void retileGraphsAccordingToSorting();
	void setupGraph(int num, int firstExtraChan);
	
	QString getGraphSettingsKey() const;
	void loadGraphSettings();
	void saveGraphSettings();


	static int NumGraphsPerGraphTab[DAQ::N_Modes];
	static void SetupNumGraphsPerGraphTab();

    int getNumGraphsPerGraphTab() const;

    volatile bool threadsafe_is_visible;

    int NUM_GRAPHS_PER_GRAPH_TAB, nGraphTabs, nColsGraphTab, nRowsGraphTab;

	
    DAQ::Params & params;
	QTabWidget *tabWidget;
	QStackedWidget *stackedWidget; QComboBox *stackedCombo;
    QVector<QWidget *> graphTabs;
    QToolBar *graphCtls;
    QPushButton *chanBut;
	QLabel *chanLbl;
    QDoubleSpinBox *graphYScale, *graphSecs;
    QCheckBox *highPassChk, *toggleSaveChk, *downsampleChk;
    QLineEdit *saveFileLE;
    QPushButton *graphColorBut;
    QVector<Vec2fWrapBuffer> points;
    QVector<GLGraph *> graphs;
	QVector<QCheckBox *> chks; /// checkboxes for above graphs!
    QVector<QFrame *> graphFrames;
    QVector<bool> pausedGraphs;
    QVector<double> graphTimesSecs;
    struct GraphStats {
        double s1; ///< sum of values
        double s2; ///< sum of squares of values
        unsigned num; ///< total number of values
        GraphStats()  { clear(); }
        void clear() { s1 = s2 = num = 0; }
        double mean() const { return s1/double(num); }
        double rms2() const { return s2/double(num); }
        double rms() const;
        double stdDev() const;
    };
    QVector<GraphStats> graphStats; ///< mean/stddev stuff
	QVector<GLGraphState> graphStates; ///< used to maintain internal glgraph state for graph re-use...
    QVector<i64> nptsAll;
    i64 nPtsAllGs; ///< sum of each element of nptsAll array above..
    volatile double downsampleRatio;
    double tNow, tLast, tAvg, tNum;
    int pdChan, firstExtraChan;
    QAction *pauseAct, *maxAct, *applyAllAct;
    GLGraph *maximized; ///< if not null, a graph is maximized 
    HPFilter *filter;
    Vec2 lastMousePos;
    int lastMouseOverGraph;
    int selectedGraph;
	QLed *stimTrigLed, *pdTrigLed;
    QCheckBox *trigOverrideChk;
    bool modeCaresAboutSGL, modeCaresAboutPD;
    bool suppressRecursive;
	QVector <int> sorting, naming;
	QSet<GLGraph *> extraGraphs;
	QTimer *tabHighlightTimer;
    std::vector<int16> scanTmp;

    mutable QMutex graphsMut; ///< recursive mutex.  locked whenever this class accesses graph data.  used because we are transitioning over to a threaded graphing data reader model as of Feb. 2016
};


#endif
