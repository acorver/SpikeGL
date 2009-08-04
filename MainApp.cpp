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
#include <QDialog>
#include <QGLFormat>
#include <QGLContext>
#include "Icon.xpm"
#include "ParWindowIcon.xpm"
#include "ConfigureDialogController.h"
#include "ChanMappingController.h"
#include "GraphsWindow.h"
#include "Sha1VerifyTask.h"
#include "Par2Window.h"
#include "ui_StimGLIntegration.h"
#include "StimGL_LeoDAQGL_Integration.h"
#include "ui_TextBrowser.h"

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
    : QApplication(argc, argv, true), consoleWindow(0), debug(false), initializing(true), sysTray(0), nLinesInLog(0), nLinesInLogMax(1000), task(0), taskReadTimer(0), graphsWindow(0), notifyServer(0), fastSettleRunning(false), helpWindow(0), noHotKeys(false), pdWaitingForStimGL(false), precreateDialog(0), pregraphDummyParent(0), maxPreGraphs(64), tPerGraph(0.), acqStartingDialog(0)
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

    if (getNProcessors() > 1)
        setProcessAffinityMask(0x1); // set it to core 1

    consoleWindow->installEventFilter(this);
    consoleWindow->textEdit()->installEventFilter(this);

    consoleWindow->resize(800, 300);
    consoleWindow->show();

    setupStimGLIntegration();

    QTimer *timer = new QTimer(this);
    Connect(timer, SIGNAL(timeout()), this, SLOT(updateStatusBar()));
    timer->setSingleShot(false);
    timer->start(247); // update status bar every 247ms.. i like this non-round-numbre.. ;)
    
    pregraphTimer = new QTimer(this);
    Connect(pregraphTimer, SIGNAL(timeout()), this, SLOT(precreateGraphs()));
    pregraphTimer->setSingleShot(false);
    pregraphTimer->start(0);

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
    delete helpWindow, helpWindow = 0;
    delete pregraphDummyParent, pregraphDummyParent = 0;
    pregraphs.clear();
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

bool MainApp::isShiftPressed()
{
    return (keyboardModifiers() & Qt::ShiftModifier);
}

bool MainApp::processKey(QKeyEvent *event) 
{
    switch (event->key()) {
    case 'd':
    case 'D':
        toggleDebugAct->trigger();
        return true;
    case 'c':
    case 'C':
        hideUnhideConsoleAct->trigger();
        return true;
    case 'g':
    case 'G':
        hideUnhideGraphsAct->trigger();
        return true;
	case 'n':
	case 'N':
		newAcqAct->trigger();
		return true;
	case Qt::Key_Escape:
		stopAcq->trigger();
		return true;
    }

    return false;
}

bool MainApp::eventFilter(QObject *watched, QEvent *event)
{
    int type = static_cast<int>(event->type());
    if (type == QEvent::KeyPress) {
        // globally forward all keypresses
        // if they aren't handled, then return false for normal event prop.
        QKeyEvent *k = dynamic_cast<QKeyEvent *>(event);
        if (k && !noHotKeys 
            && watched != helpWindow && (!helpWindow || !Util::objectHasAncestor(watched, helpWindow)) 
            && watched != par2Win && (!par2Win || !Util::objectHasAncestor(watched, par2Win)) 
            && (watched == graphsWindow || watched == consoleWindow || watched == consoleWindow->textEdit() || ((!graphsWindow || watched != graphsWindow->saveFileLineEdit()) && Util::objectHasAncestor(watched, graphsWindow)))) {
            if (processKey(k)) {
                event->accept();
                return true;
            } 
        }
    }
	if (type == QEvent::Close) {
		if (watched == graphsWindow) {
			// request to close the graphsWindow.. this stops the acq -- ask the user to confirm.. do this after this event handler runs, so enqueue it with a timer
			QTimer::singleShot(1, this, SLOT(maybeCloseCurrentIfRunning()));
			event->ignore();
			return true;
		} else if ((precreateDialog && watched == precreateDialog) || (acqStartingDialog && watched == acqStartingDialog)) {
			event->ignore();
			return true;
		}
	}
    if (watched == consoleWindow) {
        ConsoleWindow *cw = dynamic_cast<ConsoleWindow *>(watched);
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
    return  QApplication::eventFilter(watched, event);
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
    StimGLIntegrationParams & p(stimGLIntParams);
    p.iface = settings.value("StimGLInt_Listen_Interface", "0.0.0.0").toString();
    p.port = settings.value("StimGLInt_Listen_Port",  LEODAQ_GL_NOTIFY_DEFAULT_PORT).toUInt();
    p.timeout_ms = settings.value("StimGLInt_TimeoutMS", LEODAQ_GL_NOTIFY_DEFAULT_TIMEOUT_MSECS ).toInt(); 
    
}

void MainApp::saveSettings()
{
    QSettings settings("janelia.hhmi.org", APPNAME);

    settings.beginGroup("MainApp");
    settings.setValue("debug", debug);
    mut.lock();
    settings.setValue("outDir", outDir);
    mut.unlock();
    StimGLIntegrationParams & p(stimGLIntParams);
    settings.setValue("StimGLInt_Listen_Interface", p.iface);
    settings.setValue("StimGLInt_Listen_Port",  p.port);
    settings.setValue("StimGLInt_TimeoutMS", p.timeout_ms); 
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

    Mainly MainApp::pickOutputDir() makes use of this class to prevent recursive calls into itself. 

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
    noHotKeys = true;
    if ( !(od = QFileDialog::getExistingDirectory(0, "Choose a directory to which to save output files", od, QFileDialog::DontResolveSymlinks|QFileDialog::ShowDirsOnly)).isNull() ) { 
        mut.lock();
        outDir = od;
        mut.unlock();
        saveSettings(); // just to remember the file *now*
    }
    noHotKeys = false;
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

    Connect( aoPassthruAct = new QAction("AO Passthru...", this),
             SIGNAL(triggered()), this, SLOT(respecAOPassthru()));
    aoPassthruAct->setEnabled(false);
    
    Connect( helpAct = new QAction("LeoDAQGL &Help", this), 
             SIGNAL(triggered()), this, SLOT(help()));
    Connect( aboutAct = new QAction("&About", this), 
             SIGNAL(triggered()), this, SLOT(about()));
    Connect(aboutQtAct = new QAction("About &Qt", this),
            SIGNAL(triggered()), this, SLOT(aboutQt()));
            
    Connect( newAcqAct = new QAction("New Acquisition... &N", this),
             SIGNAL(triggered()), this, SLOT(newAcq()));
    Connect( stopAcq = new QAction("Stop Running Acquisition ESC", this),
             SIGNAL(triggered()), this, SLOT(maybeCloseCurrentIfRunning()) );
    stopAcq->setEnabled(false);

    Connect( verifySha1Act = new QAction("Verify SHA1...", this),
             SIGNAL(triggered()), this, SLOT(verifySha1()) );

    Connect( par2Act = new QAction("PAR2 Redundancy Tool", this),
             SIGNAL(triggered()), this, SLOT(showPar2Win()) );

    Connect( stimGLIntOptionsAct = new QAction("StimGL Integration Options", this),
             SIGNAL(triggered()), this, SLOT(execStimGLIntegrationDialog()) );
}

void MainApp::newAcq() 
{
    if ( !maybeCloseCurrentIfRunning() ) return;
    if (DAQ::ProbeAllAIChannels().empty()) {
            QMessageBox::critical(0, "Analog Input Missing!", "Could not find any analog input channels on this system!  Therefore, data acquisition is unavailable!");
            return;
    }
    noHotKeys = true;
    int ret = configCtl->exec();
    noHotKeys = false;
    if (ret == QDialog::Accepted) {
        scan0Fudge = 0;
        scanCt = 0;
        lastScanSz = 0;
        tNow = getTime();
        taskShouldStop = false;
        pdWaitingForStimGL = false;
        last5PDSamples.reserve(5);
        last5PDSamples.clear();
        DAQ::Params & params (configCtl->acceptedParams);
        if (!params.stimGlTrigResave) {
            if (!dataFile.openForWrite(params)) {
                QMessageBox::critical(0, "Error Opening File!", QString("Could not open data file `%1'!").arg(params.outputFile));
                return;
            }
        }

		// acq starting dialog block -- show this dialog because the startup is kinda slow..
		if (acqStartingDialog) delete acqStartingDialog, acqStartingDialog = 0;
		acqStartingDialog = new QMessageBox ( QMessageBox::Information, "DAQ Task Starting Up", "DAQ task starting up, please wait...", QMessageBox::Ok, consoleWindow, Qt::WindowFlags(Qt::Dialog| Qt::MSWindowsFixedSizeDialogHint));
		acqStartingDialog->setWindowModality(Qt::ApplicationModal);
		QAbstractButton *but = acqStartingDialog->button(QMessageBox::Ok);
		if (but) but->hide();
		acqStartingDialog->show();
		// end acq starting dialog block

        preBuf.clear();
        preBuf.reserve(0);
        preBuf2.clear();
        preBuf2.reserve(0);
        if (params.usePD && (params.acqStartEndMode == DAQ::PDStart || params.acqStartEndMode == DAQ::PDStartEnd)) {
            const double sil = params.silenceBeforePD > 0. ? params.silenceBeforePD : DEFAULT_PD_SILENCE;
            if (params.stimGlTrigResave) pdWaitingForStimGL = true;
            int szSamps = params.nVAIChans*params.srate*sil;
            if (szSamps <= 0) szSamps = params.nVAIChans;
            if (szSamps % params.nVAIChans) 
                szSamps += params.nVAIChans - szSamps%params.nVAIChans;
            preBuf.reserve(szSamps*sizeof(int16));
            preBuf2.reserve(szSamps*sizeof(int16));
        }

        graphsWindow = new GraphsWindow(params, 0, dataFile.isOpen());
        graphsWindow->setAttribute(Qt::WA_DeleteOnClose, false);

        graphsWindow->setWindowIcon(appIcon);
        hideUnhideGraphsAct->setEnabled(true);
        graphsWindow->installEventFilter(this);

        if (!params.suppressGraphs) {
            graphsWindow->show();
        } else {
            graphsWindow->hide();
        }
        taskWaitingForStop = false;
        
        switch (params.acqStartEndMode) {
        case DAQ::Immediate: taskWaitingForTrigger = false; break;
        case DAQ::PDStartEnd: 
        case DAQ::PDStart:
        case DAQ::Timed:
            if (params.isImmediate) {
                taskWaitingForTrigger = false;
                startScanCt = 0;
                break;
            } else {
                startScanCt = i64(params.startIn * params.srate);
            }
            stopScanCt = params.isIndefinite ? 0x7fffffffffffffffLL : i64(startScanCt + params.duration*params.srate);
            
        case DAQ::StimGLStartEnd:
        case DAQ::StimGLStart:
            taskWaitingForTrigger = true; break;
        default:
            Error() << "Internal error params.acqStartEndMode is an illegal value!  FIXME!";
            break;
        }
        task = new DAQ::Task(params, this);
        taskReadTimer = new QTimer(this);
        Connect(task, SIGNAL(bufferOverrun()), this, SLOT(gotBufferOverrun()));
        Connect(task, SIGNAL(daqError(const QString &)), this, SLOT(gotDaqError(const QString &)));
        Connect(taskReadTimer, SIGNAL(timeout()), this, SLOT(taskReadFunc()));
        taskReadTimer->setSingleShot(false);
        taskReadTimer->start(1000/TASK_READ_FREQ_HZ);
        stopAcq->setEnabled(true);
        aoPassthruAct->setEnabled(params.aoPassthru);
        Connect(task, SIGNAL(gotFirstScan()), this, SLOT(gotFirstScan()));
        task->start();
        updateWindowTitles();
        Systray() << "DAQ task starting up ...";
        Status() << "DAQ task starting up ...";
        Log() << "DAQ task starting up ...";
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
    fastSettleRunning = false;
    if (taskReadTimer) delete taskReadTimer, taskReadTimer = 0;
    if (graphsWindow) delete graphsWindow, graphsWindow = 0;
    hideUnhideGraphsAct->setEnabled(false);
    Log() << "Task " << dataFile.fileName() << " stopped.";
    Status() << "Task stopped.";
    dataFile.closeAndFinalize();
    stopAcq->setEnabled(false);
    aoPassthruAct->setEnabled(false);
    taskWaitingForTrigger = false;
    scan0Fudge = 0;
    updateWindowTitles();
	if (acqStartingDialog) delete acqStartingDialog, acqStartingDialog = 0;
    Systray() << "Acquisition stopped";
}

bool MainApp::maybeCloseCurrentIfRunning() 
{
    if (!task) return true;
    int but;
    if (dataFile.isOpen()) 
        but = QMessageBox::question(0, "Stop Current Acquisition", QString("An acquisition is currently running and saving to %1.\nStop it before proceeding?").arg(dataFile.fileName()), QMessageBox::Yes, QMessageBox::No);
    else
        but = QMessageBox::question(0, "Stop Current Acquisition", QString("An acquisition is currently running.\nStop it before proceeding?"), QMessageBox::Yes, QMessageBox::No);        
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

/* static */
void MainApp::xferWBToScans(WrapBuffer & preBuf, std::vector<int16> & scans,
                            u64 & firstSamp, i64 & scan0Fudge)
{
    void *ptr=0;
    unsigned lenBytes=0;
    preBuf.dataPtr2(ptr, lenBytes);
    int added = 0,num;
    if (ptr) {
        scans.insert(scans.begin(), reinterpret_cast<int16 *>(ptr), reinterpret_cast<int16 *>(ptr)+(num=lenBytes/sizeof(int16)));
        added += num;
    }
    preBuf.dataPtr1(ptr, lenBytes);
    if (ptr) {
        scans.insert(scans.begin(), reinterpret_cast<int16 *>(ptr), reinterpret_cast<int16 *>(ptr)+(num=lenBytes/sizeof(int16)));
        added += num;
    }
    firstSamp -= u64(added);
    scan0Fudge -= added;
    preBuf.clear();    
}

///< called from a timer at 30Hz
void MainApp::taskReadFunc() 
{ 
    std::vector<int16> scans;
    u64 firstSamp;
    int ct = 0;
    const int ctMax = 10;
    double qFillPct;
    bool needToStop = false;
    static double lastSBUpd = 0;
    const DAQ::Params & p (configCtl->acceptedParams);
    while ((ct++ < ctMax || taskShouldStop) ///< on taskShouldStop, keep trying to empty queue!
           && !needToStop
           && task
           && task->dequeueBuffer(scans, firstSamp)) {
        tNow = getTime();
        lastScanSz = scans.size();
        scanCt = firstSamp/p.nVAIChans;
        if (taskWaitingForTrigger) { // task has been triggered , so save data, and graph it..
            if (!detectTriggerEvent(scans, firstSamp))
                preBuf.putData(&scans[0], scans.size()*sizeof(scans[0])); // save data to pre-buffer ringbuffer if not triggered yet
            if (tNow-lastSBUpd > 0.25) { // every 1/4th of a second
                if (p.acqStartEndMode == DAQ::Timed) {
                    Status() << "Acquisition will auto-start in " << (startScanCt-scanCt)/p.srate << " seconds.";
                    lastSBUpd = tNow;
                } else if (p.acqStartEndMode == DAQ::StimGLStart
                          || p.acqStartEndMode == DAQ::StimGLStartEnd) {
                    Status() << "Acquisition waiting for start trigger from StimGL program";
                } else if (p.acqStartEndMode == DAQ::StimGLStart
                           || p.acqStartEndMode == DAQ::StimGLStartEnd) {
                    Status() << "Acquisition waiting for start trigger from photo-diode";
                }
            }
        } 

        // normally always pre-buffer the scans in case we get a re-trigger event
        preBuf2.putData(&scans[0], scans.size()*sizeof(scans[0]));

			// not an else because both if clauses are true on first trigger
        
        if (!taskWaitingForTrigger) { // task not waiting from trigger, normal acq..
            if (preBuf.size()) {
                xferWBToScans(preBuf, scans, firstSamp, scan0Fudge);
            } 
 
            const u64 fudgedFirstSamp = firstSamp - scan0Fudge;            
            const u64 scanSz = scans.size();
            if (!dataFile.isOpen()) scan0Fudge = firstSamp + scanSz;

            if (!needToStop && !taskShouldStop && taskWaitingForStop) {
                needToStop = detectStopTask(scans, firstSamp);
            }

            if (dataFile.isOpen()) {
                if (fudgedFirstSamp != dataFile.sampleCount()) {
                    QString e = QString("Dropped scans?  Datafile scan count (%1) and daq task scan count (%2) disagree!\nAieeeee!!  Aborting acquisition!").arg(dataFile.sampleCount()).arg(firstSamp);
                    Error() << e;
                    stopTask();
                    QMessageBox::critical(0, "DAQ Error", e);
                    return;
                }
                dataFile.writeScans(scans);
            }
            qFillPct = (task->dataQueueSize()/double(task->dataQueueMaxSize)) * 100.0;
            if (graphsWindow && !graphsWindow->isHidden()) {            
                if (qFillPct > 70.0) {
                    Warning() << "Some scans were dropped from graphing due to DAQ task queue limit being nearly reached!  Try downsampling graphs or displaying fewer seconds per graph!";
                } else { 
                    graphsWindow->putScans(scans, firstSamp);
                }
            }
            
            if (tNow-lastSBUpd > 0.25) { // every 1/4th of a second
                QString taskEndStr = "";
                if (taskWaitingForStop && p.acqStartEndMode == DAQ::Timed) {
                    taskEndStr = QString(" - task will auto-stop in ") + QString::number((stopScanCt-scanCt)/p.srate) + " secs";
                }
                Status() << task->numChans() << "-channel acquisition running @ " << task->samplingRate()/1000. << " kHz - " << dataFile.sampleCount() << " samples read - " << qFillPct << "% buffer fill - " << dataFile.writeSpeedBytesSec()/1e6 << " MB/s disk speed (" << dataFile.minimalWriteSpeedRequired()/1e6 << " MB/s required)" <<  taskEndStr;
                lastSBUpd = tNow;
            }
        } 
    }
    if (taskShouldStop || needToStop)
        stopTask();
}

bool MainApp::detectTriggerEvent(std::vector<int16> & scans, u64 & firstSamp)
{
    bool triggered = false;
    DAQ::Params & p (configCtl->acceptedParams);
    switch (p.acqStartEndMode) {
    case DAQ::Timed:  
        if (p.isImmediate || scanCt >= startScanCt) {
            triggered = true;
        }
        break;
    case DAQ::PDStartEnd:
    case DAQ::PDStart: {
        // NB: photodiode channel is always the last channel
        const int sz = scans.size();
        if (p.idxOfPdChan < 0) {
            Error() << "INTERNAL ERROR, acqStartMode is PD based but no PD channel specified in DAQ params!";
            stopTask();
            return false;
        }
        for (int i = p.idxOfPdChan; i < sz; i += p.nVAIChans) {
            const int16 samp = scans[i];
            if (!pdWaitingForStimGL && samp > p.pdThresh) {
                if (last5PDSamples.size() >= 5) {
                    triggered = true, last5PDSamples.clear();
                    pdOffTimeSamps = p.srate * p.pdStopTime * p.nVAIChans;
                    lastSeenPD = firstSamp + u64(i);
                    // we triggered, so save samples up to this one
                    int len = i-int(p.idxOfPdChan);
                    if (len > 0) {
                        preBuf.putData(&scans[0], len*sizeof(scans[0]));
                        scans.erase(scans.begin(), scans.begin()+len);
                        firstSamp += len;
                        scanCt = firstSamp/p.nVAIChans;
                    }
                    i = sz; // break out of loop
                } else 
                    last5PDSamples.push_back(samp);
            } else
                last5PDSamples.clear();
        }
    }
        break;
    case DAQ::StimGLStart:
    case DAQ::StimGLStartEnd:
        // do nothing.. this gets set from stimGL_PluginStarted() slot outside this function..
        break;
    default: {        
        QString err =  "Unanticipated/illegal p.acqStartEndMode in detectTriggerEvent: " + QString::number((int)p.acqStartEndMode);
        Error() << err;
        int but = QMessageBox::critical(0, "Trigger Event Error", err, QMessageBox::Abort, QMessageBox::Ignore);
        if (but == QMessageBox::Abort) quit();
    }
    }
    if (triggered) {
        triggerTask();
    }
    
    //scan0Fudge = firstSamp + scans.size();
    scan0Fudge = firstSamp;

    return triggered;
}

bool MainApp::detectStopTask(const std::vector<int16> & scans, u64 firstSamp)
{
    bool stopped = false;
    DAQ::Params & p (configCtl->acceptedParams);
    switch (p.acqStartEndMode) {
    case DAQ::Timed:  
        if (!p.isIndefinite && scanCt >= stopScanCt) {
            stopped = true;
            Log() << "Triggered stop because acquisition duration has fully elapsed.";
        }
        break;
    case DAQ::PDStartEnd: {
        if (p.idxOfPdChan < 0) {
            Error() << "INTERNAL ERROR, acqEndMode is PD based but no PD channel specified in DAQ params!";
            return true;
        }
        const int sz = scans.size();
        for (int i = p.idxOfPdChan; i < sz; i += p.nVAIChans) {
            const int16 samp = scans[i];
            if (samp > p.pdThresh) {
                if (last5PDSamples.size() >= 3) 
                    lastSeenPD = firstSamp+u64(i), last5PDSamples.clear();
                else 
                    last5PDSamples.push_back(samp);
            } else
                last5PDSamples.clear();
        }
        if (firstSamp+u64(sz) - lastSeenPD > pdOffTimeSamps) { // timeout PD after X scans..
            stopped = true;
            Log() << "Triggered stop due to photodiode being off for >" << p.pdStopTime << " seconds.";
        }
    }
        break;
    case DAQ::StimGLStartEnd:
        // do nothing.. this gets set from stimGL_PluginStarted() slot outside this function..
        break;
    default: {        
        QString err =  "Unanticipated/illegal p.acqStartEndMode in detectStopTask: " + QString::number((int)p.acqStartEndMode);
        Error() << err;
        int but = QMessageBox::critical(0, "Trigger Event Error", err, QMessageBox::Abort, QMessageBox::Ignore);
        if (but == QMessageBox::Abort) quit();
    }
    }
    return stopped;
}

void MainApp::triggerTask()
{
    if (task && taskWaitingForTrigger) {
        taskWaitingForTrigger = false;
        updateWindowTitles();
        Systray() << "Acquisition triggered ON";
        Status() << "Task triggered";
        DAQ::Params & p(configCtl->acceptedParams);
        switch (p.acqStartEndMode) {
        case DAQ::PDStartEnd: 
        case DAQ::StimGLStartEnd:
            taskWaitingForStop = true; 
            Log() << "Acquisition triggered";
            break;
        case DAQ::Timed:
            if (!p.isIndefinite) 
                taskWaitingForStop = true;
            Log() << "Acquisition started due to auto-timed trigger";
            break;
        default: 
            taskWaitingForStop = false;
            break; 
        }        
    }
}

void MainApp::updateWindowTitles()
{
    const bool isOpen = dataFile.isOpen();
    QString stat = "";
    QString fname = isOpen ? dataFile.fileName() : "(no outfile)";
    if (task) {
        if (taskWaitingForTrigger)
            stat = "WAITING - " + fname;
        else
            stat = "RUNNING - " + fname;
    } else {
        if (initializing) stat = "INITIALIZING";
        else stat = "No Acquisition Running";
    }
    QString tit = QString(APPNAME) + " - " + stat;
    consoleWindow->setWindowTitle(tit);
    sysTray->contextMenu()->setTitle(tit);
    if (graphsWindow) {
        tit = QString(APPNAME) + " Graphs - " + stat;
        graphsWindow->setWindowTitle(tit);
        graphsWindow->setToggleSaveChkBox(isOpen);
        if (isOpen) graphsWindow->setToggleSaveLE(fname);
    }
}

void MainApp::verifySha1()
{
    noHotKeys = true;
    QString dataFile = QFileDialog::getOpenFileName ( consoleWindow, "Select data file for SHA1 verification", outputDirectory());
    noHotKeys = false;
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
    
    if (this->task && this->dataFile.isOpen() && QFileInfo(dataFile) == QFileInfo(this->dataFile.fileName())) {
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
    noHotKeys = true;
    QMessageBox::information(consoleWindow, fn + " SHA1 Verify", str);
    noHotKeys = false;
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
    noHotKeys = true;
    QMessageBox::warning(consoleWindow, fn + " SHA1 Verify", str);
    noHotKeys = false;
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

void MainApp::execStimGLIntegrationDialog()
{
    bool again = false;
    QDialog dlg(0);
    dlg.setWindowIcon(consoleWindow->windowIcon());
    dlg.setWindowTitle("StimGL Integration Options");    
    dlg.setModal(true);
    StimGLIntegrationParams & p (stimGLIntParams);
        
    Ui::StimGLIntegration controls;
    controls.setupUi(&dlg);
    controls.interfaceLE->setText(p.iface);
    controls.portSB->setValue(p.port);
    controls.timeoutSB->setValue(p.timeout_ms);    
    do {     
        again = false;
        noHotKeys = true;
        const int ret = dlg.exec();
        noHotKeys = false;
        if ( ret == QDialog::Accepted ) {
            p.iface = controls.interfaceLE->text();
            p.port = controls.portSB->value();
            p.timeout_ms = controls.timeoutSB->value();
            if (!setupStimGLIntegration(false)) {
                QMessageBox::critical(0, "Listen Error", "Notification server could not listen on " + p.iface + ":" + QString::number(p.port) + "\nTry the options again again!");
                loadSettings();
                again = true;
                continue;
            }
        } else {
            loadSettings();
            if (!notifyServer) setupStimGLIntegration();
            return;
        }
    } while (again);

    saveSettings();
}

bool MainApp::setupStimGLIntegration(bool doQuitOnFail)
{
    if (notifyServer) delete notifyServer;
    notifyServer = new StimGL_LeoDAQGL_Integration::NotifyServer(this);
    StimGLIntegrationParams & p (stimGLIntParams);
    if (!notifyServer->beginListening(p.iface, p.port, p.timeout_ms)) {
        if (doQuitOnFail) {
            int but = QMessageBox::critical(0, "Listen Error", "Notification server could not listen on port " + QString::number(p.port) + "\nAnother copy of this program might already be running.\nContinue anyway?", QMessageBox::Abort, QMessageBox::Ignore);
            if (but == QMessageBox::Abort) postEvent(this, new QEvent((QEvent::Type)QuitEventType)); // quit doesn't work here because we are in appliation c'tor
        }
        Error() << "Failed to start StimGLII integration notification server!";
        delete notifyServer, notifyServer = 0;
        return false;
    }
    Connect(notifyServer, SIGNAL(gotPluginStartNotification(const QString &, const QMap<QString, QVariant>  &)), this, SLOT(stimGL_PluginStarted(const QString &, const QMap<QString, QVariant>  &)));
    Connect(notifyServer, SIGNAL(gotPluginEndNotification(const QString &, const QMap<QString, QVariant>  &)), this, SLOT(stimGL_PluginEnded(const QString &, const QMap<QString, QVariant>  &)));
    return true;
}


QString MainApp::getNewDataFileName(const QString & suffix) const
{
    const DAQ::Params & p (configCtl->acceptedParams);
    if (p.stimGlTrigResave) {
        QFileInfo fi(p.outputFileOrig);

        QString prefix = fi.filePath();
        QString ext = fi.completeSuffix();
        prefix.chop(ext.length()+1);
        
        for (int i = 1; i > 0; ++i) {
            QString fn = prefix + "_" + suffix + "_" + QString::number(i) + "." + ext;
            if (!QFile::exists(fn)) return fn;
        }
    }
    return p.outputFile;
}

void MainApp::stimGL_PluginStarted(const QString &plugin, const QMap<QString, QVariant>  &pm)
{
    (void)pm;
    bool ignored = true;
    DAQ::Params & p (configCtl->acceptedParams);
    if (task && p.stimGlTrigResave) {
        if (dataFile.isOpen()) {
            scan0Fudge += dataFile.sampleCount();
            Log() << "Data file: " << dataFile.fileName() << " closed by StimulateOpenGL.";
            dataFile.closeAndFinalize();
        }
        QString fn = getNewDataFileName(plugin);
        if (!dataFile.openForWrite(p, fn)) {
            QMessageBox::critical(0, "Error Opening File!", QString("Could not open data file `%1'!").arg(fn));
        } else {
            Log() << "Data file: " << dataFile.fileName() << " opened by StimulateOpenGL.";
        }

       if (p.acqStartEndMode == DAQ::PDStart || p.acqStartEndMode == DAQ::PDStartEnd) {
           taskWaitingForTrigger = true; // turns on the detect trigger code again, so we can get a constant-offset PD signal
           pdWaitingForStimGL = false;
       }           
        updateWindowTitles();        
        ignored = false;
    }
    if (task 
        && taskWaitingForTrigger 
        && (p.acqStartEndMode == DAQ::StimGLStart || p.acqStartEndMode == DAQ::StimGLStartEnd)) {
        Log() << "Triggered start by Stim GL plugin `" << plugin << "'";
        scan0Fudge = scanCt*p.nVAIChans + lastScanSz;
        triggerTask();
        ignored = false;
    }

    Debug() << "Received notification that Stim GL plugin `" << plugin << "' started." << (ignored ? " Ignored!" : "");

    if (!ignored) stimGL_SaveParams(pm);
}

void MainApp::stimGL_SaveParams(const QMap<QString, QVariant> & pm) 
{
    if (dataFile.isOpen()) {
        for (QMap<QString, QVariant>::const_iterator it = pm.begin(); it != pm.end(); ++it) 
            dataFile.setParam(QString("StimGL_") + it.key(), it.value());   
    }
}

void MainApp::stimGL_PluginEnded(const QString &plugin, const QMap<QString, QVariant> & pm)
{
    (void)pm;
    bool ignored = true;
    DAQ::Params & p (configCtl->acceptedParams);
    if (task && taskWaitingForStop && p.acqStartEndMode == DAQ::StimGLStartEnd) {
        Log() << "Triggered stop by Stim GL plugin `" << plugin << "'";
        taskShouldStop = true;
        stimGL_SaveParams(pm);
        task->stop(); ///< stop the daq task now.. we will empty the queue later..
        Log() << "DAQ task no longer acquiring, emptying queue and saving to disk.";
        ignored = false;        
    } else if (task && p.stimGlTrigResave && dataFile.isOpen()) {
        stimGL_SaveParams(pm);
        Log() << "Data file: " << dataFile.fileName() << " closed by StimulateOpenGL.";
        dataFile.closeAndFinalize();
        updateWindowTitles();        
        ignored = false;
    }
    if (task && p.stimGlTrigResave && p.acqStartEndMode == DAQ::PDStart) {
        taskWaitingForTrigger = true;
        preBuf = preBuf2; // we will get a re-trigger soon, so preBuffer the scans we had previously..
        pdWaitingForStimGL = true;
        updateWindowTitles();        
        ignored = false;        
    }
    Debug() << "Received notification that Stim GL plugin `" << plugin << "' ended." << (ignored ? " Ignored!" : "");

}

void MainApp::doFastSettle()
{
    if (fastSettleRunning || !task) return;
    fastSettleRunning = true;
    Connect(task, SIGNAL(fastSettleCompleted()), this, SLOT(fastSettleCompletion()));
    task->requestFastSettle();
}

void MainApp::fastSettleCompletion()
{
    if (!fastSettleRunning) return;
    fastSettleRunning = false;
    disconnect(task, SIGNAL(fastSettleCompleted()), this, SLOT(fastSettleCompletion()));
}

void MainApp::gotFirstScan()
{
    if (taskWaitingForTrigger) {
        Systray() << "Acquisition waiting ...";
        Status() << "Task initiated, waiting for trigger event";
        Log() << "Acquisition initiated, waiting for trigger event";
    } else {
        Systray() << "Acquisition started";
        Status() << "Task started";
        Log() << "Acquisition started.";
    }
	delete acqStartingDialog, acqStartingDialog = 0;
}

void MainApp::toggleSave(bool s)
{
    const DAQ::Params & p (configCtl->acceptedParams);
    if (s && !dataFile.isOpen()) {
        if (!dataFile.openForWrite(p)) {
            QMessageBox::critical(0, "Error Opening File!", QString("Could not open data file `%1'!").arg(p.outputFile));
            return;
        }
        Log() << "Save file: " << dataFile.fileName() << " opened from GUI.";
        scan0Fudge = scanCt*p.nVAIChans + lastScanSz;
        //graphsWindow->clearGraph(-1);
        updateWindowTitles();
    } else if (!s && dataFile.isOpen()) {
        dataFile.closeAndFinalize();
        Log() << "Save file: " << dataFile.fileName() << " closed from GUI.";
        updateWindowTitles();
    }
}

void MainApp::respecAOPassthru()
{
    if (task) {
        configCtl->showAOPassThruDlg();
    }
}

void MainApp::help()
{
    if (!helpWindow) {
        Ui::TextBrowser tb;
        helpWindow = new QDialog(0);
        helpWindow->setWindowTitle("LeoDAQGL Help");
        tb.setupUi(helpWindow);
        tb.textBrowser->setSearchPaths(QStringList("qrc:/"));
        tb.textBrowser->setSource(QUrl("qrc:/LeoDAQGL-help-manual.html"));
    }
    helpWindow->show();
    helpWindow->setMaximumSize(helpWindow->size());
}

void MainApp::precreateOneGraph()
{
    const double t0 = getTime();
    // keep creating GLContexts until the creation count hits 64
    if (!pregraphDummyParent) pregraphDummyParent = new QWidget(0);
    QFrame *f = new QFrame(pregraphDummyParent);
    GLGraph *g = new GLGraph(f);
    QVBoxLayout *bl = new QVBoxLayout(f);
    bl->addWidget(g);
    bl->setSpacing(0);
    bl->setContentsMargins(0,0,0,0);

    tPerGraph = tPerGraph * pregraphs.count();
    tPerGraph += (getTime()-t0);
    pregraphs.push_back(f);
    tPerGraph /= pregraphs.count();
}

void MainApp::precreateGraphs()
{
    const int MAX_GRAPHS = maxPreGraphs;
    if (!precreateDialog) {
        precreateDialog = new QMessageBox(QMessageBox::Information, 
                                          "Precreating GL Graphs",
                                          QString("Precreating ") + QString::number(MAX_GRAPHS) + " graphs, please wait...",
                                          QMessageBox::NoButton,
                                          consoleWindow);
        precreateDialog->addButton("Quit", QMessageBox::RejectRole);
        Connect(precreateDialog, SIGNAL(rejected()), this, SLOT(quit()));
        Connect(precreateDialog, SIGNAL(accepted()), this, SLOT(quit()));
        precreateDialog->show();
    }
    if (int(pregraphs.count()) < MAX_GRAPHS) {
        precreateOneGraph();
        if (pregraphs.count() == MAX_GRAPHS) {
            Debug () << "Pre-created " << pregraphs.count() << " GLGraphs, creation time " << tPerGraph*1e3 << " msec avg per graph, done.";
            delete precreateDialog, precreateDialog = 0;
            delete pregraphTimer, pregraphTimer = 0;
            appInitialized();
        } 
    }
}

QFrame * MainApp::getGLGraphWithFrame()
{
    QFrame *f = 0;
    if (!pregraphs.count()) 
        precreateOneGraph(); // creates at least one GLGraph before returning
    f = pregraphs.front();
    pregraphs.pop_front();
    if (!f) Error() << "INTERNAL ERROR: Expected a valid QFrame from pregraphs list! Aiiieee...";
    return f;
}

void MainApp::putGLGraphWithFrame(QFrame *f)
{
    GLGraph *g = dynamic_cast<GLGraph *>(f->children().front());
    if (!g) {
        Error() << "INTERNAL ERROR: QFrame passed in to putGLGraphWithFrame does not contain a GLGraph child!";
        delete f;
        return;
    }
    f->setParent(pregraphDummyParent);
    g->reset(f);
    pregraphs.push_back(f);
    
}

void MainApp::appInitialized()
{
    initializing = false;
    Log() << "Application initialized";    
    Status() <<  APPNAME << " initialized.";
    updateWindowTitles();
}
