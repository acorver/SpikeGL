/*! \mainpage LeoDAQGL
 *
 * \section intro_sec Introduction
 *
 * This is documentation for the LeoDAQGL program.  It is mainly
 * sourcecode documentation. 
 */

#ifndef MainApp_H
#define MainApp_H

#include <QApplication>
#include <QColor>
#include <QMutex>
#include <QMutexLocker>
#include <QByteArray>
#include <QIcon>
#include "Util.h"
#include "DAQ.h"
#include "DataFile.h"
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
#include "StimGL_LeoDAQGL_Integration.h"

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

public slots:    
    /// Set/unset the application-wide 'debug' mode setting.  If the application is in debug mode, Debug() messages are printed to the console window, otherwise they are not
    void toggleDebugMode(); 
    /// Pops up the application "About" dialog box
    void about();
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
   
protected slots:
    /// Called from a timer every ~250 ms to update the status bar at the bottom of the console window
    void updateStatusBar();

    void gotBufferOverrun();
    void gotDaqError(const QString & e);
    void taskReadFunc(); ///< called from a timer at 30Hz

    void sha1VerifySuccess();
    void sha1VerifyFailure();
    void sha1VerifyCancel();

    void showPar2Win();

    void execStimGLIntegrationDialog();

    void stimGL_PluginStarted(const QString &, const QMap<QString, QVariant>  &);
    void stimGL_PluginEnded(const QString &, const QMap<QString, QVariant>  &);

    void fastSettleCompletion();

private:
    /// Display a message to the status bar
    void statusMsg(const QString & message, int timeout_msecs = 0);
    void initActions(); 
    void initShortcuts(); 
    void loadSettings();
    void saveSettings();
    void createAppIcon();
    bool processKey(QKeyEvent *);
    void stopTask();
    bool setupStimGLIntegration(bool doQuitOnFail=true);
    void detectTriggerEvent(const std::vector<int16> & scans, u64 firstSamp);
    void triggerTask();
    bool detectStopTask(const std::vector<int16> & scans, u64 firstSamp);

    mutable QMutex mut; ///< used to lock outDir param for now
    ConfigureDialogController *configCtl;

    ConsoleWindow *consoleWindow;
    bool debug;
    volatile bool initializing;
    QColor defaultLogColor;
    QString outDir;
    QString sb_String;
    int sb_Timeout;
    QSystemTrayIcon *sysTray;
    struct StimGLIntegrationParams {
        QString iface;
        unsigned short port;
        int timeout_ms;
    } stimGLIntParams;
#ifndef Q_OS_WIN
    unsigned refresh;
#endif
    static MainApp *singleton;
    unsigned nLinesInLog, nLinesInLogMax;

    double tNow;
    u64 lastSeenPD, pdOffTimeSamps;
    DAQ::Task *task;
    bool taskWaitingForTrigger, taskWaitingForStop, 
        taskShouldStop; ///< used for StimGL trigger to stop the task when the queue empties
    i64 scan0Fudge, scanCt, startScanCt, stopScanCt, lastScanSz;
    DataFile dataFile;
    std::vector<int16> last5PDSamples;
    QTimer *taskReadTimer;
    GraphsWindow *graphsWindow;
    Par2Window *par2Win;
    StimGL_LeoDAQGL_Integration::NotifyServer *notifyServer;
    bool fastSettleRunning;

public:

/// Main application actions!
    QAction 
        *quitAct, *toggleDebugAct, *chooseOutputDirAct, *hideUnhideConsoleAct, 
        *hideUnhideGraphsAct, *aboutAct, *aboutQtAct, *newAcqAct, *stopAcq, *verifySha1Act, *par2Act, *stimGLIntOptionsAct;

/// Appliction icon! Made public.. why the hell not?
    QIcon appIcon;
};

#endif
