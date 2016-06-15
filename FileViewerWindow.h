/*
 *  FileViewerWindow.h
 *  SpikeGL
 *
 *  Created by calin on 10/26/10.
 *  Copyright 2010 Calin Culianu <calin.culianu@gmail.com>. All rights reserved.
 *
 */
#ifndef FileViewerWindow_H
#define FileViewerWindow_H
#include <QMainWindow>
#include "DataFile.h"
#include "VecWrapBuffer.h"
#include <QPair>
#include "ChanMap.h"
#include <QBitArray>

class GLGraph;
class QScrollArea;
class QSpinBox;
class QDoubleSpinBox;
class QSlider;
class QLabel;
class QToolBar;
class QPushButton;
class QTimer;
class QMenu;
class QAction;
class ExportDialogController;
struct ExportParams;
class QFrame;
class QCheckBox;
class HPFilter;
class QEvent;
class TaggableLabel;
class QComboBox;

/// The class that handles the window you get when opening files.
class FileViewerWindow : public QMainWindow
{
	Q_OBJECT
public:
	FileViewerWindow();
	~FileViewerWindow();

	/// Call this method to associate a file with this class and open it and view it.  
	/// Ok to call it multiple times to open new files using same window.
	bool viewFile(const QString & fileName, QString *errMsg_out = 0);
	
	/// Returns the filename with path of the .bin file that is opened by this instance, or null QString if nothing is open
	QString file() const { if (dataFile.isOpen()) return  dataFile.fileName(); return QString::null; }

	/// Offer the user opportunity to abort a close operation, otherwise closes the window
	bool queryCloseOK();
	
protected:
	void resizeEvent(QResizeEvent *);
	void showEvent(QShowEvent *);
	bool eventFilter(QObject *obj, QEvent *event);

private slots:
	void setFilePos(int pos); // TODO/FIXME/XXX: this should take a 64-bit parameter but we need to connect to Qt gui stuff that is 32 bit
	void setFilePosSecs(double);
	void setXScaleSecs(double);
	void setYScale(double);
	void setNDivs(int);
	void colorSchemeMenuSlot();
	void setAuxGain(double);
	void mouseOverGraph(double,double);
	void mouseOverGraphInWindowCoords(int,int);
	void clickedGraphInWindowCoords(int,int);
    void toggleMaximize(); // maximize/minimize
	void showAllGraphs();
	void hideUnhideGraphSlot();
	void hideCloseTimeout();
	void viewModeMenuSlot();
	void resizeIt();
	void updateData();
	void mouseClickSlot(double,double);
	void mouseReleaseSlot(double,double);
	void exportSlot();
    void selectGraph(int graphNum); ///< pass in a graph id (which is relative to current page)
	void hpfChk(bool);
	void dcfChk(bool);
	void hpfLblClk();
	void dcfLblClk();
	void applyAllSlot();
	void fileOpenMenuSlot();
	void fileOptionsMenuSlot();
	void clickedCloseLbl(GLGraph *g);
	void sortGraphsByIntan();
	void sortGraphsByElectrode();
    void graphsPerPageChanged(int);
    void repaginate();
    void pageChanged(int);
    void updateSelection(); ///< calls updateSelection(true)

private:
	void loadSettings();
	void saveSettings();
	void layoutGraphs();
	double timeFromPos(qint64 p) const;
	qint64 posFromTime(double) const;
	void configureMiscControls(bool blockSignals = false);
	qint64 nScansPerGraph() const;
	QPair<double, double> yVoltsAfterGain(int whichGraph) const;
	void applyColorScheme(GLGraph *);
    void hideGraph(int n); ///< pass in a graph id (which is relative to current page)
    void showGraph(int n); ///< pass in a graph id (which is relative to current page)
	void mouseOverGraphInWindowCoords(GLGraph *, int,int);
	void setFilePos64(qint64 pos, bool noupdate = false);
	void printStatusMessage();
	void doExport(const ExportParams &);
    int graphsPerPage() const { return n_graphs_pg; }
    int currentGraphsPage() const { return curr_graph_page; }
    int g2i(int g) const { return currentGraphsPage()*graphsPerPage() + g; }
    int i2g(int i) const { return i - currentGraphsPage()*graphsPerPage(); }
    void redoGraphs(); ///< used when graphs per page changes and also as a setup function when opening a new file. deletes all old graph data and reestablishes graph data structures
    void updateSelection(bool do_opengl_update);

	QString generateGraphNameString(unsigned graphNum, bool verbose = true) const;
	
	enum ViewMode { Tiled = 0, Stacked, StackedLarge, StackedHuge, N_ViewMode } viewMode;
	static const QString viewModeNames[];
	
	enum ColorScheme { Ice = 0, Fire, Green, BlackWhite, Classic, N_ColorScheme, DefaultScheme = Ice } colorScheme;
	static const QString colorSchemeNames[];
	
	DataFile dataFile;
		
	QScrollArea *scrollArea; ///< the central widget
	QWidget *graphParent;

    struct GraphParams {
        double yZoom, gain;
        bool filter300Hz, dcFilter;
        QString objname;
        GraphParams() : yZoom(1.0), gain(1.0), filter300Hz(false), dcFilter(true) {}
    };


    /*-- Below are: INDEXED by numChans! */
    QVector<QAction *> graphHideUnhideActions; ///< indexed by numChans!
    QBitArray hiddenGraphs; ///< indexed by numChans!
    QVector<GraphParams> graphParams; ///< per-graph params
    QVector<int> graphSorting; ///< used for sort by electrode id/sort by intan feature  to sort the graphs.  read by layoutGraphs()


    /*-- Below two are: INDEXED BY graphsPerPage(), not numChans.. graphs on screen are a subset of all channels as of June 2016 */
    QVector<GLGraph *> graphs; ///< indexed by graphsPerPage()
    QVector<QFrame *> graphFrames; ///< indexed by graphsPerPage()
    QVector<Vec2fWrapBuffer> graphBufs; ///< indexed by graphsPerPage()!
    // BELOW TWO MEMBERS ARE BY GRAPH ID, NOT CHANNEL INDEX!
    int maximizedGraph; ///< if non-negative, we are maximized on a particular graph
    int selectedGraph;

    QVector<QVector<Vec2f> > scratchVecs;
    QSpinBox *posScansSB, *graphPgSz;
	QDoubleSpinBox *posSecsSB;
	QSlider *posSlider;
	QLabel *totScansLbl;
	QLabel *totSecsLbl;
	QToolBar *toolBar;
	QDoubleSpinBox *xScaleSB, *yScaleSB, *auxGainSB;
	QLabel *xDivLbl, *yDivLbl;
	QSpinBox *nDivsSB;
	TaggableLabel *closeLbl;
	QLabel *graphNameLbl;
	QCheckBox *highPassChk, *dcfilterChk;
	bool electrodeSort;

    QMenu *channelsMenu;
	QAction *colorSchemeActions[N_ColorScheme];	
	QAction *viewModeActions[N_ViewMode];
	QAction *sortByElectrode, *sortByIntan;
	QTimer *hideCloseTimer;

	bool didLayout;
	
	// misc graph zoom/view/etc settings
	double nSecsZoom, defaultYZoom, defaultGain;
	unsigned nDivs;
	
	qint64 pos, ///< in scan counts
	       pscale; ///< scaling factor for the QSlider since it uses 32-bit values and file pos can theoretically be 64-bit
	qint64 selectionBegin, selectionEnd; ///< selection position (in scans) and number of scans.  if begin is negative, no selection
	qint64 saved_selectionBegin, saved_selectionEnd;
	ChanMap chanMap;
	double mouseOverT, mouseOverV;
	int mouseOverGNum;
	
	bool mouseButtonIsDown, dontKillSelection;
	
    QAction *exportAction, *exportSelectionAction, *maxunmaxAction;
	ExportDialogController *exportCtl;
	
	
	HPFilter *hpfilter;
	double arrowKeyFactor, pgKeyFactor;

    QComboBox *pageCB;
    QLabel *maximizedLbl, *pgLbl, *allChannelsHiddenLbl;
    int n_graphs_pg, curr_graph_page;
};


#endif

