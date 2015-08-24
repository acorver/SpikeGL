/*! \mainpage SpikeGL
 *
 * \section intro_sec Introduction
 *
 * This is documentation for the SpikeGL program.  It is mainly
 * sourcecode documentation. 
 */

#ifndef MainApp_H
#define MainApp_H

class QTextEdit;
class ConsoleWindow;
class GLWindow;
class QTcpServer;
class QKeyEvent;
class QSystemTrayIcon;
class QAction;
class ConfigureDialogController;
class GraphsWindow;
class Par2Window;
class QDialog;
class QFrame;
class QTimer;
class QMessageBox;
class CommandConnection;
class FileViewerWindow;
class PostJuly2011Remuxer;
class SpatialVisWindow;
class Bug_ConfigDialog;
class Bug_Popout;
class FG_ConfigDialog;

#include <QApplication>
#include <QColor>
#include <QMutex>
#include <QMutexLocker>
#include <QByteArray>
#include <QIcon>
#include <QList>
#include <QString>
#include <QMap>
#include <QSet>
#include <QDialog>
#include "Util.h"
#include "DAQ.h"
#include "DataFile.h"
#include "TempDataFile.h"
#include "WrapBuffer.h"
#include "StimGL_SpikeGL_Integration.h"
#include "CommandServer.h"

#ifdef Q_OS_WIN
#include <windows.h>

class DataFile_Fn_Shm : public DataFile
{
public:
	DataFile_Fn_Shm();
	~DataFile_Fn_Shm();
	/// overrides parent, adds stuff to write to shm for fast access to filename
    bool openForWrite(const DAQ::Params & params, const QString & filename_override = "");
	/// overrides parent, adds stuff to write to shm for fast access to filename
    bool closeAndFinalize();

private:
	HANDLE hMapFile;
	void *pBuf;
	static const unsigned BUF_SIZE;
	static const char szName[1024];
};
#else
#  define DataFile_Fn_Shm DataFile
#endif

/**
   \brief The central class to the program that more-or-less encapsulates most objects and data in the program.

   This class inherits from QApplication for simplicity.  It is sort of a 
   central place that other parts of the program use to find settings,
   pointers to other windows, and various other application-wide data.
*/   
class MainApp : public QApplication
{
    Q_OBJECT

    friend int main(int, char **);
    MainApp(int & argc, char ** argv);  ///< only main can constuct us
public:

    /// Returns a pointer to the singleton instance of this class, if one exists, otherwise returns 0
    static MainApp *instance() { return singleton; }

    virtual ~MainApp();
    
    /// Returns a pointer to the application-wide ConsoleWindow instance
    ConsoleWindow *console() const { return const_cast<ConsoleWindow *>(consoleWindow); }

    /// Returns true iff the console is not visible
    bool isConsoleHidden() const;

    /// Returns true iff the application's console window has debug output printing enabled
    bool isDebugMode() const;
    
    /// Returns true if the application is currently acquiring data
    bool isAcquiring() const { return task; }
    
    /// Returns the current scan number that was most recently read from the daq hardware and enqueued, or 0 if no task is running
    u64 currentDAQScan() const { if (isAcquiring() && task) return task->lastReadScan(); return 0; }

    /// Returns true if the application is currently saving data
    bool isSaving() const;
    
	/// Returns true iff the per-channel save checkbox option is enabled
	bool isSaveCBEnabled() const;

	/// Returns true if the enable datastream facility checkbox option is enabled
	bool isDSFacilityEnabled() const;

    /// Set the save file
    void setOutputFile(const QString &);
    /// Query the save file -- note this is the save file as set in the config control params -- not necessarily an indicator that a save is happening.  for that, see getCurrentSaveFile()
    QString outputFile() const;
	/// Query the absolute path of the current save file.  Returns QString::null if not saving.  This is the actual file we are currently saving to. Note this is reentrant and threadsafe, unlike the above functions
	QString getCurrentSaveFile() const;
    
    /// Returns the directory under which all plugin data files are to be saved.
    QString outputDirectory() const { QMutexLocker l(&mut); return outDir; }
    /// Set the directory under which all plugin data files are to be saved. NB: dpath must exist otherwise it is not set and false is returned
    bool setOutputDirectory(const QString & dpath);

    /// Thread-safe logging -- logs a line to the log window in a thread-safe manner
    void logLine(const QString & line, const QColor & = QColor());

    /// Used to catch various events from other threads, etc
    bool eventFilter ( QObject * watched, QEvent * event );

    enum EventsTypes {
        LogLineEventType = QEvent::User, ///< used to catch log line events see MainApp.cpp 
        StatusMsgEventType, ///< used to indicate the event contains a status message for the status bar
        QuitEventType, ///< so we can post quit events..
    };

    /// Use this function to completely lock the mouse and keyboard -- useful if performing a task that must not be interrupted by the user.  Use sparingly!
    static void lockMouseKeyboard();
    /// Undo the damage done by a lockMouseKeyboard() call
    static void releaseMouseKeyboard();

    /// Returns true if and only if the application is still initializing and not done with its startup.  This is mainly used by the socket connection code to make incoming connections stall until the application is finished initializing.
    bool busy() const { return initializing; }

    QString sbString() const;
    void setSBString(const QString &statusMsg, int timeout_msecs = 0);
    
    void sysTrayMsg(const QString & msg, int timeout_msecs = 0, bool iserror = false);

    bool isShiftPressed();

    QString getNewDataFileName(const QString & stimglSuffix = "") const;

    /** Attempts to pop a QFrame containing a pre-created GLGraph off the 
        internal precreate list.  If the internal list is empty, simply
        creates a new graph with frame and returns it.  */
    QFrame *getGLGraphWithFrame(bool noGLGraph = false);

    /** Puts a QFrame with GLGraph * child back into the internal list, 
        returns the current count of QFrames */
    void putGLGraphWithFrame(QFrame *);

	/// The configure dialog controller -- an instance of this is always around 
	ConfigureDialogController *configureDialogController() { return configCtl; }
	
	/// Get a reference to the temp data file -- used by CommandServer to call readScans(),etc for the Matlab data read API
	const TempDataFile & tempDataFile() const { return tmpDataFile; }

	/// Open a data file for perusal using the already-existing FileViewerWindow reuseWindow.  Called from File->Open slot for the file viewer window.
	void fileOpen(FileViewerWindow *reuseWindow);
	
    DAQ::NITask * niTask() { return (!task ? 0 : dynamic_cast<DAQ::NITask *>(task)); }
    DAQ::BugTask * bugTask() { return (!task ? 0 : dynamic_cast<DAQ::BugTask *>(task)); }
    DAQ::FGTask * fgTask() { return (!task ? 0 : dynamic_cast<DAQ::FGTask *>(task)); }

    // WindowMenu stuff
    void windowMenuRemove(QWidget *w);
    void windowMenuAdd(QWidget *w);

public slots:    
    /// Set/unset the application-wide 'debug' mode setting.  If the application is in debug mode, Debug() messages are printed to the console window, otherwise they are not
    void toggleDebugMode(); 
	/// Like above, but set with Ctrl+D
	void toggleExcessiveDebugMode();
    /// Pops up the application "About" dialog box
    void about();

    /// Pops up the help browser featuring the help document
    void help();

    /// \brief Prompts the user to pick a save directory.
    /// @see setOutputDirectory 
    /// @see outputDirectory
    void pickOutputDir();
    /// Toggles the console window hidden/shown state
    void hideUnhideConsole();

    /// Toggles the graphs window hidden/shown state
    void hideUnhideGraphs();

    /// new acquisition!
    void newAcq();
	
	/// new *bug3/telemetry* acquisition!
	void bugAcq();

	/// new *framegrabber* acquisition!
	void fgAcq();

    /// check if everything is ok, ask user, then quit
    void maybeQuit();
    /// returns true if task was closed or it wasn't running, otherwise returns false if user opted to not quit running task
    bool maybeCloseCurrentIfRunning();

    void updateWindowTitles();

    void verifySha1();

    /// called by a button in the graphs window
    void doFastSettle();

    /// called by control in graphs window -- toggles datafile save on/off
    void toggleSave(bool);

    /// called by menu option
    void respecAOPassthru();

    /// called by a timer func once all init is done
    void appInitialized();
   
protected slots:
    /// Called from a timer every ~250 ms to update the status bar at the bottom of the console window
    void updateStatusBar();

    void gotBufferOverrun();
    void gotDaqError(const QString & e);
    void gotDaqWarning(const QString & e);
    void taskReadFunc(); ///< called from a timer at 30Hz

    void sha1VerifySuccess();
    void sha1VerifyFailure();
    void sha1VerifyCancel();

    void showPar2Win();
	void par2WinClosed();

    void execStimGLIntegrationDialog();    
    void execCommandServerOptionsDialog();
	void execDSTempFileDialog();

    void stimGL_PluginStarted(const QString &, const QMap<QString, QVariant>  &);
    void stimGL_SaveParams(const QString & unused, const QMap<QString, QVariant> & pm);
    void stimGL_PluginEnded(const QString &, const QMap<QString, QVariant>  &);

    void fastSettleCompletion();
    void precreateGraphs();
    void gotFirstScan();

protected:
    void customEvent(QEvent *); ///< actually implemented in CommandServer.cpp since it is used to handle events from network connection threads

private slots:
    void par2WinForCommandConnectionEnded(); ///< implemented in CommandServer.cpp
    void par2WinForCommandConnectionGotLines(const QString & lines); ///< implemented in CommandServer.cpp
    void par2WinForCommandConnectionError(const QString & lines); ///< implemented in CommandServer.cpp
    void fastSettleDoneForCommandConnections();
	void toggleShowChannelSaveCB();
	void toggleEnableDSFacility();
	void windowMenuActivate(QWidget *w = 0);
	void windowMenuAboutToShow();
	void helpWindowClosed();
	void bringAllToFront();
	void fileOpen(); ///< slot triggered from the File->Open menu to open a new data file for perusal in the app.	
	void optionsSortGraphsByElectrode(); ///< slot triggered by Options->sort graph by electrode menu action
    void gotManualTrigOverride(bool); ///< sent from GraphsWindow UI when user wants to temporarily manually override all the triggers and save immediately

private:
    /// Display a message to the status bar
    void statusMsg(const QString & message, int timeout_msecs = 0);
    void initActions(); 
    void initShortcuts(); 
    void loadSettings();
    void saveSettings();
    void createIcons();
    bool processKey(QKeyEvent *);
    void stopTask();
    bool setupStimGLIntegration(bool doQuitOnFail=true);
    bool setupCommandServer(bool doQuitOnFail=true);
    bool detectTriggerEvent(const std::vector<int16> & scans, u64 firstSamp, i32 & triggerOffset);
    void triggerTask();
    bool detectStopTask(const std::vector<int16> & scans, u64 firstSamp);
    static void prependPrebufToScans(const WrapBuffer & wb, std::vector<int16> & scans, int & numAdded, int skip);
    void precreateOneGraph(bool noGLGraph = false);
    bool startAcq(QString & errTitle, QString & errMsg);
	void showPrecreateDialog();
	void precreateDone();
	void startAcqWithPossibleErrDialog();
	
    void putRestarts(const DAQ::Params & p, u64 firstSamp, u64 restartSize) const;
	
    QMap<QString, QVariant> queuedParams;    
    QMap<Par2Window *, CommandConnection *> par2WinConnMap;
    QSet<CommandConnection *> fastSettleConns;  ///< connections waiting for fast settle...
    
    mutable QMutex mut; ///< used to lock outDir for now
    ConfigureDialogController *configCtl;
	Bug_ConfigDialog *bugConfig;
	FG_ConfigDialog *fgConfig;


    ConsoleWindow *consoleWindow;
    bool debug, saveCBEnabled;
    volatile bool initializing;
    QColor defaultLogColor;
    QString outDir, lastOpenFile;
    QString sb_String;
    int sb_Timeout;
    QSystemTrayIcon *sysTray;
    
    struct GenericServerParams {
        QString iface;
        unsigned short port;
        int timeout_ms;        
    };
    
    struct StimGLIntegrationParams : public GenericServerParams {
    } stimGLIntParams;
    
    struct CommandServerParams : public GenericServerParams {
        bool enabled;
    } commandServerParams;
    
#ifndef Q_OS_WIN
    unsigned refresh;
#endif
    static MainApp *singleton;
    unsigned nLinesInLog, nLinesInLogMax;

    double tNow;
    u64 lastSeenPD, pdOffTimeSamps;
    DAQ::Task *task;
    bool taskWaitingForTrigger, taskHasManualTrigOverride, taskWaitingForStop,
        taskShouldStop; ///< used for StimGL trigger to stop the task when the queue empties
    i64 scanCt, startScanCt, stopScanCt, lastScanSz, stopRecordAtSamp;
    DataFile_Fn_Shm dataFile; ///< the OUTPUT save file (this member var never used for input)
    std::vector<int16> lastNPDSamples;
    QTimer *taskReadTimer;
    GraphsWindow *graphsWindow;
	SpatialVisWindow *spatialWindow;
	Bug_Popout *bugWindow;
    Par2Window *par2Win;
    StimGL_SpikeGL_Integration::NotifyServer *notifyServer;
    CommandServer *commandServer;
    bool fastSettleRunning;
    QDialog *helpWindow;

    WrapBuffer preBuf;
    bool noHotKeys, pdWaitingForStimGL;	
    bool dsFacilityEnabled;
    
    QMessageBox *precreateDialog;
    QTimer *pregraphTimer;
    QWidget *pregraphDummyParent;
    QList<QFrame *> pregraphs;
    int maxPreGraphs;
    double tPerGraph;

	QMessageBox *acqStartingDialog;

	TempDataFile tmpDataFile;

	QList<QWidget *> windows;
	QMap<QWidget *, QAction *> windowActions;
	
	bool acqWaitingForPrecreate;
	
	PostJuly2011Remuxer *addtlDemuxTask;
	bool doBugAcqInstead, doFGAcqInstead, m_sortGraphsByElectrodeId;
	
public:

/// Main application actions!
    QAction 
        *quitAct, *toggleDebugAct, *toggleExcessiveDebugAct, *chooseOutputDirAct, *hideUnhideConsoleAct, 
        *hideUnhideGraphsAct, *aboutAct, *aboutQtAct, *newAcqAct, *stopAcq, *verifySha1Act, *par2Act, *stimGLIntOptionsAct, *aoPassthruAct, *helpAct, *commandServerOptionsAct,
		*showChannelSaveCBAct, *enableDSFacilityAct, *fileOpenAct, *tempFileSizeAct, *bringAllToFrontAct,
	    *sortGraphsByElectrodeAct, *bugAcqAct, *fgAcqAct;

/// Appliction icon! Made public.. why the hell not?
    QIcon appIcon, bugIcon;
	
/// Used by GraphsWindow to re-sort graphs according to current app state...
	bool sortGraphsByElectrodeId() const; 
};

class HelpWindow : public QDialog
{
	Q_OBJECT
public:
	HelpWindow(QWidget *parent = 0) : QDialog(parent) {}
	
signals:
	void closed();
	
protected:
	void closeEvent(QCloseEvent *e);
};

#endif
