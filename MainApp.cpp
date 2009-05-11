#include <QTextEdit>
#include <QMessageBox>
#include "MainApp.h"
#include "ConsoleWindow.h"
#include "Util.h"
#include "Version.h"
#include <qglobal.h>
#include <QEvent>
#include <cstdlib>
#include <QSettings>
#include <QMetaType>
#include <QStatusBar>
#include <QTimer>
#include <QKeyEvent>
#include <QFileDialog>
#include <QTextStream>
#include <QFile>
#include <QFileInfo>
#include <QPixmap>
#include <QIcon>
#include <QDir>
#include <QDesktopWidget>
#include <QMutexLocker>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QProgressDialog>
#include "Icon.xpm"
#include "ParWindowIcon.xpm"
#include "ConfigureDialogController.h"
#include "GraphsWindow.h"
#include "Sha1VerifyTask.h"
#include "Par2Window.h"

Q_DECLARE_METATYPE(unsigned);

namespace {
    struct Init {
        Init() {
            qRegisterMetaType<unsigned>("unsigned");
        }
    };

    Init * volatile init = 0;


    class LogLineEvent : public QEvent
    {
    public:
        LogLineEvent(const QString &str, const QColor & color)
            : QEvent((QEvent::Type)MainApp::LogLineEventType), str(str), color(color)
        {}
        QString str;
        QColor color;
    };

    class StatusMsgEvent : public QEvent
    {
    public:
        StatusMsgEvent(const QString &msg, int timeout)
            : QEvent((QEvent::Type)MainApp::StatusMsgEventType), msg(msg), timeout(timeout)
        {}
        QString msg;
        int timeout;
    };

};

MainApp * MainApp::singleton = 0;

MainApp::MainApp(int & argc, char ** argv)
    : QApplication(argc, argv, true), debug(false), initializing(true), nLinesInLog(0), nLinesInLogMax(1000), task(0), taskReadTimer(0), graphsWindow(0)
{
    sb_Timeout = 0;
    if (singleton) {
        QMessageBox::critical(0, "Invariant Violation", "Only 1 instance of MainApp allowed per application!");
        std::exit(1);
    }
    singleton = this;
    if (!::init) ::init = new Init;
    setQuitOnLastWindowClosed(false);
    loadSettings();

    initActions();

    createAppIcon();
    
    configCtl = new ConfigureDialogController(this);

    installEventFilter(this); // filter our own events

    consoleWindow = new ConsoleWindow;
    defaultLogColor = consoleWindow->textEdit()->textColor();
    consoleWindow->setAttribute(Qt::WA_DeleteOnClose, false);

    sysTray = new QSystemTrayIcon(this);
    sysTray->setContextMenu(new QMenu(consoleWindow));
    sysTray->contextMenu()->addAction(hideUnhideConsoleAct);
    sysTray->contextMenu()->addAction(hideUnhideGraphsAct);
    sysTray->contextMenu()->addSeparator();
    sysTray->contextMenu()->addAction(aboutAct);
    sysTray->contextMenu()->addSeparator();
    sysTray->contextMenu()->addAction(quitAct);
    sysTray->setIcon(appIcon);
    sysTray->show();

    par2Win = new Par2Window(0);
    par2Win->setAttribute(Qt::WA_DeleteOnClose, false); 
    par2Win->setWindowTitle(QString(APPNAME) + " - Par2 Redundancy Tool");
    par2Win->setWindowIcon(QPixmap(ParWindowIcon_xpm));

    Log() << "Application started";

    consoleWindow->installEventFilter(this);
    consoleWindow->textEdit()->installEventFilter(this);

    consoleWindow->resize(800, 300);
    consoleWindow->show();



#ifdef Q_OS_WIN
    initializing = false;
    Log() << "Application initialized";    
#endif
    Status() <<  APPNAME << " initialized.";

    QTimer *timer = new QTimer(this);
    Connect(timer, SIGNAL(timeout()), this, SLOT(updateStatusBar()));
    timer->setSingleShot(false);
    timer->start(247); // update status bar every 247ms.. i like this non-round-numbre.. ;)

    updateWindowTitles();
}

MainApp::~MainApp()
{
    stopTask();
    Log() << "Application shutting down..";
    Status() << "Application shutting down.";
    saveSettings();
    delete par2Win, par2Win = 0;
    delete configCtl, configCtl = 0;
    delete sysTray, sysTray = 0;
    singleton = 0;
}

bool MainApp::isDebugMode() const
{
    // always true for now..
    return debug;
}

bool MainApp::isConsoleHidden() const 
{
    return !consoleWindow || consoleWindow->isHidden();
}

void MainApp::toggleDebugMode()
{
    debug = !debug;
    Log() << "Debug mode: " << (debug ? "on" : "off");
    saveSettings();    
}

void MainApp::processKey(QKeyEvent *event) 
{
    switch (event->key()) {
    case 'd':
    case 'D':
        toggleDebugAct->trigger();
        event->accept();
        break;
    case 'c':
    case 'C':
        hideUnhideConsoleAct->trigger();
        event->accept();
        break;
    case 'g':
    case 'G':
        hideUnhideGraphsAct->trigger();
        event->accept();
        break;
    }
}

bool MainApp::eventFilter(QObject *watched, QEvent *event)
{
    int type = static_cast<int>(event->type());
    if (type == QEvent::KeyPress) {
        // globally forward all keypresses
        // if they aren't handled, then return false for normal event prop.
        QKeyEvent *k = dynamic_cast<QKeyEvent *>(event);
        if (k && (watched == graphsWindow || watched == consoleWindow || watched == consoleWindow->textEdit())) {
            processKey(k);
            if (k->isAccepted()) {
                return true;
            }
        }
    } 
    ConsoleWindow *cw = dynamic_cast<ConsoleWindow *>(watched);
    if (cw) {
        if (type == LogLineEventType) {
            LogLineEvent *evt = dynamic_cast<LogLineEvent *>(event);
            if (evt && cw->textEdit()) {
                QTextEdit *te = cw->textEdit();
                QColor origcolor = te->textColor();
                te->setTextColor(evt->color);
                te->append(evt->str);

                // make sure the log textedit doesn't grow forever
                // so prune old lines when a threshold is hit
                nLinesInLog += evt->str.split("\n").size();
                if (nLinesInLog > nLinesInLogMax) {
                    const int n2del = MAX(nLinesInLogMax/10, nLinesInLog-nLinesInLogMax);
                    QTextCursor cursor = te->textCursor();
                    cursor.movePosition(QTextCursor::Start);
                    for (int i = 0; i < n2del; ++i) {
                        cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor);
                    }
                    cursor.removeSelectedText(); // deletes the lines, leaves a blank line
                    nLinesInLog -= n2del;
                }

                te->setTextColor(origcolor);
                te->moveCursor(QTextCursor::End);
                te->ensureCursorVisible();
                return true;
            } else {
                return false;
            }
        } else if (type == StatusMsgEventType) {/*
            StatusMsgEvent *evt = dynamic_cast<StatusMsgEvent *>(event);
            if (evt && cw->statusBar()) {
                cw->statusBar()->showMessage(evt->msg, evt->timeout);
                return true;
            } else {
                return false;
            }
                                                */
            return true;
        }
    }
    if (watched == this) {
        if (type == QuitEventType) {
            quit();
            return true;
        }
    }
    // otherwise do default action for event which probably means
    // propagate it down
    return QApplication::eventFilter(watched, event);
}

void MainApp::logLine(const QString & line, const QColor & c)
{
    qApp->postEvent(consoleWindow, new LogLineEvent(line, c.isValid() ? c : defaultLogColor));
}

void MainApp::loadSettings()
{
    QSettings settings("janelia.hhmi.org", APPNAME);

    settings.beginGroup("MainApp");
    debug = settings.value("debug", true).toBool();
    mut.lock();
#ifdef Q_OS_WIN
    outDir = settings.value("outDir", "c:/users/code").toString();
#else
    outDir = settings.value("outDir", QDir::homePath() ).toString();
#endif
    mut.unlock();

}

void MainApp::saveSettings()
{
    QSettings settings("janelia.hhmi.org", APPNAME);

    settings.beginGroup("MainApp");
    settings.setValue("debug", debug);
    mut.lock();
    settings.setValue("outDir", outDir);
    mut.unlock();
}


void MainApp::statusMsg(const QString &msg, int timeout)
{
    if (consoleWindow && consoleWindow->statusBar()) {
        consoleWindow->statusBar()->showMessage(msg, timeout);
    }
}

void MainApp::sysTrayMsg(const QString & msg, int timeout_msecs, bool iserror)
{
    if (sysTray) {
        sysTray->showMessage(APPNAME, msg, iserror ? QSystemTrayIcon::Critical : QSystemTrayIcon::Information, timeout_msecs);
    }
}

QString MainApp::sbString() const
{
    QMutexLocker ml(&mut);
    return sb_String;
}

void MainApp::setSBString(const QString &msg, int timeout) 
{
    QMutexLocker ml(&mut);
    sb_String = msg;
    sb_Timeout = timeout;
    sysTray->setToolTip(msg);
}

void MainApp::updateStatusBar()
{
    statusMsg(sb_String, sb_Timeout);
}

/** \brief A helper class that helps prevent reentrancy into certain functions.

    Mainly MainApp::loadStim(), MainApp::unloadStim(), and MainApp::pickOutputDir() make use of this class to prevent recursive calls into themselves. 

    Functions that want to be mutually exclusive with respect to each other
    and non-reentrant with respect to themselves need merely construct an 
    instance of this class as a local variable, and
    then reentrancy into the function can be guarded by checking against
    this class's operator bool() function.
*/
struct ReentrancyPreventer
{
    static volatile int ct;
    /// Increments a global counter.
    /// The global counter is 1 if only 1 instance of this class exists throughout the application, and >1 otherwise.
    ReentrancyPreventer() { ++ct; }
    /// Decrements the global counter.  
    /// If it reaches 0 this was the last instance of this class and there are no other ones active globally.
    ~ReentrancyPreventer() {--ct; }
    /// Returns true if the global counter is 1 (that is, only one globally active instance of this class exists throughout the application), and false otherwise.  If false is returned, you can then abort your function early as a reentrancy condition has been detected.
    operator bool() const { return ct == 1; }
};
volatile int ReentrancyPreventer::ct = 0;

void MainApp::about()
{
    QMessageBox::about(consoleWindow, "About "APPNAME, 
                       VERSION_STR 
                       "\n\n(C) 2009 Calin A. Culianu <cculianu@yahoo.com>\n\n"
                       "Developed for the Anthony Leonardo lab at\n"
                       "Janelia Farm Research Campus, HHMI\n\n"
                       "Software License: GPL v2 or later");
}

bool MainApp::setOutputDirectory(const QString & dpath)
{
    QDir d(dpath);
    if (!d.exists()) return false;
    mut.lock();
    outDir = dpath;
    mut.unlock();
    return true;
}

void MainApp::pickOutputDir()
{
    ReentrancyPreventer rp; if (!rp) return;
    mut.lock();
    QString od = outDir;
    mut.unlock();
    if ( !(od = QFileDialog::getExistingDirectory(0, "Choose a directory to which to save output files", od, QFileDialog::DontResolveSymlinks|QFileDialog::ShowDirsOnly)).isNull() ) { 
        mut.lock();
        outDir = od;
        mut.unlock();
        saveSettings(); // just to remember the file *now*
    }
}

void MainApp::createAppIcon()
{
    appIcon.addPixmap(QPixmap(Icon_xpm));
}

void MainApp::hideUnhideConsole()
{
    if (consoleWindow) {
        if (consoleWindow->isHidden()) {
            consoleWindow->show();
        } else {
            bool hadfocus = ( focusWidget() == consoleWindow );
            consoleWindow->hide();
            if (graphsWindow && hadfocus) graphsWindow->setFocus(Qt::OtherFocusReason);
        }
    }
}

void MainApp::hideUnhideGraphs()
{
    if (graphsWindow) {
        if (graphsWindow->isHidden()) {
            graphsWindow->clearGraph(-1);
            graphsWindow->show();
        } else {
            bool hadfocus = ( focusWidget() == graphsWindow );
            graphsWindow->hide();
            if (consoleWindow && hadfocus) consoleWindow->setFocus(Qt::OtherFocusReason);
        }
    }

}


void MainApp::initActions()
{
    Connect( quitAct = new QAction("&Quit", this) , 
             SIGNAL(triggered()), this, SLOT(maybeQuit()));
    Connect( toggleDebugAct = new QAction("&Debug Mode D", this) ,
             SIGNAL(triggered()), this, SLOT(toggleDebugMode()));
    Connect( chooseOutputDirAct = new QAction("Choose &Output Directory...", this),
             SIGNAL(triggered()), this, SLOT(pickOutputDir()));
    Connect( hideUnhideConsoleAct = new QAction("Hide/Unhide &Console C", this),
             SIGNAL(triggered()), this, SLOT(hideUnhideConsole()));
    Connect( hideUnhideGraphsAct = new QAction("Hide/Unhide &Graphs G", this),
             SIGNAL(triggered()), this, SLOT(hideUnhideGraphs()));
    hideUnhideGraphsAct->setEnabled(false);
    
    Connect( aboutAct = new QAction("&About", this), 
             SIGNAL(triggered()), this, SLOT(about()));
    Connect(aboutQtAct = new QAction("About &Qt", this),
            SIGNAL(triggered()), this, SLOT(aboutQt()));
            
    Connect( newAcqAct = new QAction("New Acquisition...", this),
             SIGNAL(triggered()), this, SLOT(newAcq()));
    Connect( stopAcq = new QAction("Stop Running Acquisition", this),
             SIGNAL(triggered()), this, SLOT(maybeCloseCurrentIfRunning()) );
    stopAcq->setEnabled(false);

    Connect( verifySha1Act = new QAction("Verify SHA1...", this),
             SIGNAL(triggered()), this, SLOT(verifySha1()) );

    Connect( par2Act = new QAction("PAR2 Redundancy Tool", this),
             SIGNAL(triggered()), this, SLOT(showPar2Win()) );
}

void MainApp::newAcq() 
{
    if ( !maybeCloseCurrentIfRunning() ) return;
    int ret = configCtl->exec();
    Debug() << "New Acq dialog exec code: " << ret;
    if (ret == QDialog::Accepted) {
        DAQ::Params & params (configCtl->acceptedParams);
        if (!dataFile.openForWrite(params)) {
            QMessageBox::critical(0, "Error Opening File!", QString("Could not open data file `%1'!").arg(params.outputFile));
            return;
        }
        graphsWindow = new GraphsWindow(params, 0);
        graphsWindow->setAttribute(Qt::WA_DeleteOnClose, false);

        graphsWindow->setWindowIcon(appIcon);
        hideUnhideGraphsAct->setEnabled(true);
        graphsWindow->installEventFilter(this);
        if (!params.suppressGraphs) {
            graphsWindow->show();
        } else {
            graphsWindow->hide();
        }
        task = new DAQ::Task(params, this);
        taskReadTimer = new QTimer(this);
        Connect(task, SIGNAL(bufferOverrun()), this, SLOT(gotBufferOverrun()));
        Connect(task, SIGNAL(daqError(const QString &)), this, SLOT(gotDaqError(const QString &)));
        Connect(taskReadTimer, SIGNAL(timeout()), this, SLOT(taskReadFunc()));
        taskReadTimer->setSingleShot(false);
        taskReadTimer->start(1000/TASK_READ_FREQ_HZ);
        stopAcq->setEnabled(true);
        task->start();
        updateWindowTitles();
        Systray() << "Acquisition started";
        Status() << "Task started";
    }
}

/// check if everything is ok, ask user, then quit
void MainApp::maybeQuit()
{
    if (task) {
        int but = QMessageBox::question(0, "Confirm Exit", "An acquisition is curently running!\nStop the acquisition and quit?", QMessageBox::Yes, QMessageBox::No);
        if (but != QMessageBox::Yes) return;
    }
    quit();
}

void MainApp::stopTask()
{
    if (!task) return;
    delete task, task = 0;  
    if (taskReadTimer) delete taskReadTimer, taskReadTimer = 0;
    if (graphsWindow) delete graphsWindow, graphsWindow = 0;
    hideUnhideGraphsAct->setEnabled(false);
    Log() << "Task " << dataFile.fileName() << " stopped.";
    Status() << "Task stopped.";
    dataFile.closeAndFinalize();
    stopAcq->setEnabled(false);
    updateWindowTitles();
    Systray() << "Acquisition stopped";

}

bool MainApp::maybeCloseCurrentIfRunning() 
{
    if (!task) return true;
    int but = QMessageBox::question(0, "Stop Current Acquisition", QString("An acquisition is currently running and saving to %1.\nStop it before proceeding?").arg(dataFile.fileName()), QMessageBox::Yes, QMessageBox::No);
    if (but == QMessageBox::Yes) {
        stopTask();
        return true;
    }
    return false;
}

void MainApp::gotBufferOverrun()
{
    Warning() << "Buffer overrun! Aieeeee! TODO FIXME!!!";
}

void MainApp::gotDaqError(const QString & e)
{
    QMessageBox::critical(0, "DAQ Error", e);
    stopTask();
}

///< called from a timer at 30Hz
void MainApp::taskReadFunc() 
{ 
    std::vector<int16> scans;
    u64 firstSamp;
    int ct = 0;
    const int ctMax = 10;
    double qFillPct;
    while (ct++ < ctMax && task->dequeueBuffer(scans, firstSamp)) {
        if (firstSamp != dataFile.sampleCount()) {
            QString e = QString("Dropped scans?  Datafile scan count (%1) and daq task scan count (%2) disagree!\nAieeeee!!  Aborting acquisition!").arg(dataFile.sampleCount()).arg(firstSamp);
            Error() << e;
            stopTask();
            QMessageBox::critical(0, "DAQ Error", e);
            return;
        }
        dataFile.writeScans(scans);
        qFillPct = (task->dataQueueSize()/double(task->dataQueueMaxSize)) * 100.0;
        if (graphsWindow && !graphsWindow->isHidden()) {            
            if (qFillPct > 70.0) {
                Warning() << "Some scans were dropped from graphing due to DAQ task queue limit being nearly reached!  Try downsampling graphs or displaying fewer seconds per graph!";
            } else { 
                graphsWindow->putScans(scans, firstSamp);
            }
        }
        static double lastSBUpd = 0;
        double tNow = getTime();

        if (tNow-lastSBUpd > 0.25) { // every 1/4th of a second
            Status() << task->numChans() << "-channel acquisition running @ " << task->samplingRate()/1000. << " kHz - " << dataFile.sampleCount() << " samples read - " << qFillPct << "% buffer fill - " << dataFile.writeSpeedBytesSec()/1e6 << " MB/s disk speed (" << dataFile.minimalWriteSpeedRequired()/1e6 << " MB/s required)";
            lastSBUpd = tNow;
        }
    }
}

void MainApp::updateWindowTitles()
{
    QString stat = "";
    if (task) {
        stat = "RUNNING - " + dataFile.fileName();
    } else {
        stat = "No Acquisition Running";
    }
    QString tit = QString(APPNAME) + " - " + stat;
    consoleWindow->setWindowTitle(tit);
    sysTray->contextMenu()->setTitle(tit);
    if (graphsWindow) {
        tit = QString(APPNAME) + " Graphs - " + stat;
        graphsWindow->setWindowTitle(tit);
    }
}

void MainApp::verifySha1()
{
    QString dataFile = QFileDialog::getOpenFileName ( consoleWindow, "Select data file for SHA1 verification", outputDirectory());
    if (dataFile.isNull()) return;
    QFileInfo pickedFI(dataFile);
    Params p;


    if (pickedFI.suffix() != "meta") {
        pickedFI.setFile(pickedFI.path() + "/" + pickedFI.completeBaseName() + ".meta");
    } else {
        dataFile = ""; 
    }
    if (!pickedFI.exists()) {
        QMessageBox::critical(consoleWindow, ".meta file does not exist!", "SHA1 verification requires the meta-file for the data file to exist.\n`" + pickedFI.fileName() + "' does not exist or is not readable!");
        return;
    }
    if (!p.fromFile(pickedFI.filePath())) {
        QMessageBox::critical(consoleWindow, pickedFI.fileName() + " could not be read!", "SHA1 verification requires the meta-file for the data file to exist.\n`" + pickedFI.fileName() + "' could not be read!");
        return;
    }
    if (!dataFile.length()) dataFile = p["outputFile"].toString();
    
    if (this->task && QFileInfo(dataFile) == QFileInfo(this->dataFile.fileName())) {
        QMessageBox::critical(consoleWindow, "Acquisition is running on file", "Cannot verify SHA1 hash on this file as it is currently open and being used as the datafile for the currently-running acquisition!");
        return;
    }

    // now, spawn a new thread for the task..
    Sha1VerifyTask *task = new Sha1VerifyTask(dataFile, p, this);
    Connect(task, SIGNAL(success()), this, SLOT(sha1VerifySuccess()));
    Connect(task, SIGNAL(failure()), this, SLOT(sha1VerifyFailure()));
    Connect(task, SIGNAL(canceled()), this, SLOT(sha1VerifyCancel()));
    task->prog->show();
    task->start();
}

void MainApp::sha1VerifySuccess()
{
    Sha1VerifyTask *task = dynamic_cast<Sha1VerifyTask *>(sender());
    QString fn;
    if (task) fn = task->dataFileNameShort;    
    QString str = fn + " SHA1 sum verified ok!";
    Log() << str;
    QMessageBox::information(consoleWindow, fn + " verified", str);
    if (task) delete task;
    else Error() << "sha1VerifySuccess error, no task!";
}

void MainApp::sha1VerifyFailure()
{
    Sha1VerifyTask *task = dynamic_cast<Sha1VerifyTask *>(sender());
    QString err;
    if (task) err = task->extendedError;
    QString fn;
    if (task) fn = task->dataFileNameShort;    
    QString str = fn + " verify error:\n" + err;
    Warning() << str;
    QMessageBox::warning(consoleWindow, fn + " verify error", str);
    if (task) delete task;
    else Error() << "sha1VerifyFailure error, no task!";
}

void MainApp::sha1VerifyCancel()
{
    Sha1VerifyTask *task = dynamic_cast<Sha1VerifyTask *>(sender());
    QString fn;
    if (task) fn = task->dataFileNameShort;    
    QString str = fn + " SHA1 verify canceled.";
    Log() << str;
    if (task) delete task;
    else Error() << "sha1VerifyCancel error, no task!";
}

void MainApp::showPar2Win()
{
    par2Win->show();
}
