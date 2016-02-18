/*
 *  CommandServer.cpp
 *  SpikeGL
 *
 *  Created by calin on 5/18/10.
 *  Copyright 2010 Calin Culianu <calin.culianu@gmail.com>. All rights reserved.
 *
 */

#include "CommandServer.h"
#include "MainApp.h"
#include "Util.h"
#include <QThread>
#include <QTcpSocket>
#include "SockUtil.h"
#include "Version.h"
#include <QDir>
#include <QDirIterator>
#include "Sha1VerifyTask.h"
#include <QMutex>
#include <QWaitCondition>
#include "ConfigureDialogController.h"
#include "Par2Window.h"


CommandServer::CommandServer(MainApp *parent)
: QTcpServer(parent), timeout_msecs(DEFAULT_COMMAND_TIMEOUT_MS)
{
}

CommandServer :: ~CommandServer()
{
    Log() << "Command server stopped.";
}

bool CommandServer :: beginListening(const QString & iface, unsigned short port, unsigned tout)
{
    timeout_msecs = tout;        
    QHostAddress haddr;
    if (iface == "0.0.0.0") haddr = QHostAddress(QHostAddress::Any);
    else if (iface == "localhost" || iface == "127.0.0.1") haddr = QHostAddress(QHostAddress::LocalHost);
    else haddr = iface;
    if (!listen(haddr, port)) {
        Error() << "Command server could not listen: " << errorString();
        return false;
    }
    Log() << "Command server started, listening on " << haddr.toString() << ":" << port;
    return true;
}


class CommandConnection : public QThread
{
    volatile bool stop;
    QTcpSocket *sock;
#if QT_VERSION >= 0x050000
    qintptr sockFd;
#else
    int sockFd;
#endif
    int timeout;
    QString resp; ///< response to client
    QString errMsg; ///< errMsg response to client

    /// event processing to main thread stuff
    QMutex mut;
    QWaitCondition cond;
    volatile bool gotResponse, lastResponse;
    QVariant evtResponse;
    
    bool processLine(const QString & line);
    void sendOK();
    void sendError(const QString &);
    
public:
#if QT_VERSION >= 0x050000
    CommandConnection(qintptr sockFd, int timeout);
#else
    CommandConnection(int sockFd, int timeout);
#endif
    ~CommandConnection();
        
    void setResponseAndWake(const QVariant & r = QVariant()); 
    void appendContinuousResponseAndWake(const QString & l = QString::null);
    
protected:
    void run();         
    
    friend struct ConnSha1Verifier;
    
    void progress(int pct);
    
    /// sends the event e to the app and blocks the called on a wait condition until a response is received
    /// after the return of this function, one can be sure the event was sent and the response was received
    void postEventToAppAndWaitForReply(QEvent *); 
    /// similar to above but used when the main app will be sending us continuous output (such as for the par2 subprocess)
    /// in that case, we are continuously receiving output until the lastResponse flag is set
    void postEventToAppAndWaitForContinuousReplies(QEvent *e);
};

#if QT_VERSION >= 0x050000
void CommandServer::incomingConnection (qintptr socketDescr)
#else
void CommandServer::incomingConnection(int socketDescr)
#endif
{
    CommandConnection *conn = new CommandConnection(socketDescr, timeout_msecs);
    conn->start();
}

/* static */
void CommandServer::deleteAllActiveConnections() 
{
    QObjectList objs = mainApp()->children();
    for (QObjectList::iterator it = objs.begin(); it != objs.end(); ++it) {
        CommandConnection *c = dynamic_cast<CommandConnection *>(*it);
        if (c) delete c;
    }
}

#if QT_VERSION >= 0x050000
CommandConnection::CommandConnection(qintptr sockFd, int timeout)
#else
CommandConnection::CommandConnection(int sockFd, int timeout)
#endif
    : QThread(mainApp()), stop(false), sock(0), sockFd(sockFd), timeout(timeout)
{    
}

#ifdef Q_OS_WIN
#  include <winsock.h>
#  include <io.h>
#  include <windows.h>
#else
#  include <sys/socket.h>
#endif
#ifndef SHUT_RDWR 
#define SHUT_RDWR 2
#endif

CommandConnection::~CommandConnection()
{
    stop = true;
#ifndef Q_OS_WIN
    if (sock && sock->state() == QAbstractSocket::ConnectedState) 
    /* HACK!!! Argh! Force blocking read to return early.. */
        shutdown(sock->socketDescriptor(), SHUT_RDWR); 
    if (isRunning()) wait(timeout+10);
    if (isRunning()) terminate();
    if (sock) delete sock, sock = 0;
#else // windows
    if (isRunning()) terminate();
    // NB: sock is possibly leaked here!
#endif
    Debug() << "deleted command connection object";
}

void CommandConnection::run() {
    sock = new QTcpSocket;
    sock->setSocketDescriptor(sockFd);
    sock->moveToThread(this);
    QString connName = sock->peerAddress().toString() + ":" + QString::number(sock->peerPort());
    Log() << "New command connection from " << connName ;
    SockUtil::Context ctx(QString("Command connection from ") + connName);
#if QT_VERSION >= 0x040600
    sock->setSocketOption(QAbstractSocket::LowDelayOption, 1); // turn off Nagle algorithm
#else
    Util::socketNoNagle(sock->socketDescriptor());
#endif
    int errCt = 0;
    static const int max_errCt = 5;
    while (!stop && sock->isValid() && errCt < max_errCt) {
        QString line = SockUtil::readLine(*sock, timeout, &errMsg);
        if (line.isNull()) {
            Debug() << connName << ": " << errMsg;
            break;
        }
        Debug() << "Got line: " << line;
        if (line.length()) {
            if ( processLine(line) ) {
                sendOK();
                errCt = 0;
            } else {
                sendError(errMsg);
                ++errCt;
            }
        }
    }
    Debug() << SockUtil::contextName() << " ended";
    if (!stop) {
        if (sock) delete sock, sock = 0;
        deleteLater();
    }
}

struct ConnSha1Verifier : public Sha1Verifier {    
    CommandConnection *conn;
    
    ConnSha1Verifier(const QString & f, const Params & p, CommandConnection * conn) 
        : Sha1Verifier(f, "", p), conn(conn), highestProg(-1) {}
    
    int highestProg;
    
    void progress(int pct) { if (pct > highestProg) conn->progress(pct), highestProg = pct; }
};


/** ----------------------------------------------------------------------------
 Command Event handling for communicating with  main appliation thread 
 ----------------------------------------------------------------------------
 */
enum EvtType {
    E_Min = QEvent::User+100,
    E_IsConsoleHidden = E_Min,
    E_ConsoleHide,
    E_ConsoleUnhide,
    E_GetParams,
    E_SetParams,
    E_StartACQ,
    E_StopACQ,
    E_Par2,
    E_CommConnEnded,
    E_IsSaving,
    E_SetSaving,
    E_SetSaveFile,
    E_FastSettle,
    E_GetScanCount,
    E_GetChannelSubset
};

struct CustomEvt : QEvent
{
    CommandConnection *conn;
    QVariant param;
    CustomEvt(int type, CommandConnection *conn) : QEvent((QEvent::Type)type), conn(conn) {}
};

bool CommandConnection::processLine(const QString & line) 
{
    QStringList toks = line.split(QRegExp("\\s+"),  QString::SkipEmptyParts);
    errMsg = "";
    if (toks.empty()) return false;
    QString cmd = toks.front().toUpper();
    toks.pop_front();
    bool ret = true;
    resp = QString::null;

    if (cmd == "NOOP") {
        // do nothing, will just send OK in caller
    } else if (cmd == "GETVERSION") {
        resp.sprintf("%s\n", VERSION_STR);
    } else if (cmd == "GETTIME") {
        resp.sprintf("%6.3f\n", Util::getTime());
    } else if (cmd == "GETSAVEDIR") {
        resp = mainApp()->outputDirectory() + "\n";
    } else if (cmd == "SETSAVEDIR") {
        QString dpath = line.mid(cmd.length()).trimmed();
        QFileInfo info(dpath);
        if (info.exists() && info.isDir()) {
            mainApp()->setOutputDirectory(dpath); // note, not saved to settings..
            Log() << "Remote host set output dir: " << dpath;
        } else {
            ret = false;
            errMsg.sprintf("\"%s\" is not a directory or does not exist.", dpath.toUtf8().constData());
        }         
    } else if (cmd == "GETDIR") {
        QDir dir (mainApp()->outputDirectory());
        if (!dir.exists()) {
            errMsg = QString("Directory does not exist: ") + mainApp()->outputDirectory();
            ret = false;
        } else {
            QDirIterator it(mainApp()->outputDirectory());
            while (it.hasNext()) {
                it.next();
                QFileInfo fi = it.fileInfo();
                QString entry = fi.fileName();
                if (fi.isDir()) entry += "/"; 
                if (entry.startsWith("ERROR")) entry = QString(" ") + entry; ///< prepend space to prevent matlab from barfing
                if ( ! SockUtil::send(*sock, entry + "\n", timeout, &errMsg, true) ) {
                    sock->abort(), ret = true;
                    break;
                }
            }
        }
    } else if (cmd == "VERIFYSHA1") {
        QString fpath = line.mid(cmd.length()).trimmed();
        if (!QFileInfo(fpath).isAbsolute())
            fpath = mainApp()->outputDirectory() + "/" + fpath;
        QFileInfo pickedFI(fpath);
        Params p;
                
        if (pickedFI.isDir()) {
            ret = false;
            errMsg = "specified file is a directory";
        } else if (!pickedFI.exists()) {
            ret = false;
            errMsg = "specified file does not exist";
        } else {
            if (pickedFI.suffix() != "meta") {
                pickedFI.setFile(pickedFI.path() + "/" + pickedFI.completeBaseName() + ".meta");
            } else {
                fpath = ""; 
            }
            if (!pickedFI.exists()) {
                errMsg =  ".meta file for this file does not exist";
                ret = false;
            } else if (!p.fromFile(pickedFI.filePath())) {
                errMsg = pickedFI.fileName() + " could not be read";
                ret = false;
            } else { // A-OK.. proceed
                if (!fpath.length()) fpath = p["outputFile"].toString();
            
                ConnSha1Verifier v(fpath, p, this);
                Sha1Verifier::Result res = v.verify();
                if (res != Sha1Verifier::Success) {
                    ret = false;
                    errMsg = "Sha1 sum does not match sum in meta file";
                }
            }
        }
    } else if (cmd == "ISACQ") {
        resp = mainApp()->isAcquiring() ? "1\n" : "0\n";
    } else if (cmd == "ISINITIALIZED") {
        resp = mainApp()->busy() ? "0\n" : "1\n";
    } else if (cmd == "ISCONSOLEHIDDEN") {
        QEvent *e = new CustomEvt(E_IsConsoleHidden,this);
        postEventToAppAndWaitForReply(e); // resp will be filled in for us
    } else if (cmd == "CONSOLEHIDE") {
        QEvent *e = new CustomEvt(E_ConsoleHide,this);
        postEventToAppAndWaitForReply(e); // resp will be filled in for us        
    } else if (cmd == "CONSOLEUNHIDE") {
        QEvent *e = new CustomEvt(E_ConsoleUnhide,this);
        postEventToAppAndWaitForReply(e); // resp will be filled in for us    
    } else if (cmd == "GETPARAMS") {
        QEvent *e = new CustomEvt(E_GetParams, this);
        postEventToAppAndWaitForReply(e);
    } else if (cmd == "SETPARAMS") {
        if ( SockUtil::send(*sock, "READY\n", timeout, 0, true) ) {
            QString str = "", line;
            while (!(line = SockUtil::readLine(*sock, timeout, 0)).isNull()) {
                if (line.length() == 0) break; // done on blank line
                str += line.trimmed() + "\n";
            }
            CustomEvt *e = new CustomEvt(E_SetParams, this);
            e->param = str;
            postEventToAppAndWaitForReply(e);
            if (errMsg.length()) ret = false;        
        }
    } else if (cmd == "STARTACQ") {
        CustomEvt *e = new CustomEvt(E_StartACQ, this);
        postEventToAppAndWaitForReply(e);
        if (errMsg.length()) ret = false;
    } else if (cmd == "STOPACQ") {
        CustomEvt *e = new CustomEvt(E_StopACQ, this);
        postEventToAppAndWaitForReply(e);
    } else if (cmd == "PAR2") {
        if (toks.count() >= 2) {
            toks.front() = toks.front().trimmed().toLower(); // force lcase for cmd                
            CustomEvt *e = new CustomEvt(E_Par2, this);
            e->param = toks;
            postEventToAppAndWaitForContinuousReplies(e);
        } else {
            ret = false;
            errMsg = "PAR2 command requires at least 2 arguments";
        }
    } else if (cmd == "ISSAVING") {
        QEvent *e = new CustomEvt(E_IsSaving,this);
        postEventToAppAndWaitForReply(e); // resp will be filled in for us
    } else if (cmd == "SETSAVING") {
        if (toks.size() > 0) {
            CustomEvt *e = new CustomEvt(E_SetSaving, this);
            e->param = toks.front().toInt();
            postEventToAppAndWaitForReply(e); // resp will be filled in for us            
        } else {
            ret = false;
            errMsg = "Pass a boolean flag to SETSAVING command.";
        }
    } else if (cmd == "SETSAVEFILE") {
        CustomEvt *e = new CustomEvt(E_SetSaveFile,this);
        e->param = toks.join(" ").trimmed();
        postEventToAppAndWaitForReply(e);
	} else if (cmd == "GETCURRENTSAVEFILE") {
		resp = mainApp()->getCurrentSaveFile();
		if (resp.isNull()) resp = "";
		resp = resp + "\n";
    } else if (cmd == "FASTSETTLE") {
        CustomEvt *e = new CustomEvt(E_FastSettle,this);
        postEventToAppAndWaitForReply(e);
    } else if (cmd == "GETDAQDATA") {
        if (!mainApp()->isDSFacilityEnabled())
        {
            Warning() << (errMsg = "Matlab data API facility not enabled");
			ret = false;
        }
        else if (2 <= toks.size())
        {
            bool bitArrayInitialized = false;
            QBitArray channelSubset;
            if (3 <= toks.size())
            {
                QStringList strList = toks.at(2).split('#', QString::SkipEmptyParts);
                unsigned int chanNo = mainApp()->configureDialogController()->acceptedParams.nVAIChans;
                unsigned int chanToSet = ~0;
                for (int i = 0, n = strList.size(); i < n; ++i)
                    if (chanNo > (chanToSet = strList.at(i).toInt()))
                    {
                        if (!bitArrayInitialized)
                        {
                            channelSubset.resize(chanNo);
                            channelSubset.fill(false);
                            bitArrayInitialized = true;
                        }
                        channelSubset.setBit(chanToSet);
                    }

				if (!bitArrayInitialized) 					
                        Warning() << "Input channel_subset is invalid";
            }
			unsigned downsample = 1;

			if (4 <= toks.size()) 
				downsample = toks.at(3).toUInt();

            if (!bitArrayInitialized)
                channelSubset = mainApp()->configureDialogController()->acceptedParams.demuxedBitMap;

            QVector<int16> matrix;
			if (!mainApp()->tempDataFile().readScans(matrix, 
												   toks.at(0).toLongLong(),
                                                   toks.at(1).toLongLong(),
												   channelSubset,
												   downsample)) {
				ret = false;
				errMsg = "Could not read scans as specified.  Check the parameters specified and try again.";				
				 
			} else {
				if (!matrix.isEmpty())
				{
					const int chans = channelSubset.count(true);
					const int scans = matrix.size() / chans;

					SockUtil::send(*sock, QString().sprintf("BINARY DATA %d %d\n", chans, scans), timeout, &errMsg, true);
					sock->write(QByteArray::fromRawData(reinterpret_cast<char *>(&matrix[0]), matrix.size() * sizeof(int16)));
				}
				else
				{
					Warning() << (errMsg = "Matlab API: no data read from temp data file");
					ret = false;
				}
			}
        }
    } else if (cmd == "GETSCANCOUNT") {
		if (!mainApp()->isDSFacilityEnabled())
        {
            Warning() << "Matlab data API facility not enabled, returning 0 for scan count";
			resp = "0\n";
        } else {
			QEvent *e = new CustomEvt(E_GetScanCount, this);
			postEventToAppAndWaitForReply(e);
		}
    } else if (cmd == "GETCHANNELSUBSET") {
		QEvent *e = new CustomEvt(E_GetChannelSubset, this);
        postEventToAppAndWaitForReply(e);
    }
    else if (cmd == "BYE" || cmd == "QUIT" || cmd == "EXIT" || cmd == "CLOSE") {
        Debug() << "Client requested shutdown, closing connection..";
        sock->close();
    } else {
        ret = false;
        errMsg = "Unrecognized command.";
    }
    
    if (!resp.isNull()) {
        if ( ! SockUtil::send(*sock, resp, timeout, &errMsg, true) )
            sock->abort(), ret = false;
    }
    
    return ret;
}

void CommandConnection::sendOK() {
    if (!sock->isValid()) return;
    if ( ! SockUtil::send(*sock, "OK\n", timeout, 0, true) )
        sock->abort();
}

void CommandConnection::sendError(const QString & errMsg) {
    if (!sock->isValid()) return;
    QString msg = QString("ERROR %1\n").arg(errMsg);
    if ( ! SockUtil::send(*sock, msg, timeout, 0, true) )
        sock->abort();
}

// called from sha1 verifier pretty much
void CommandConnection::progress(int pct) {
    SockUtil::send(*sock, QString().sprintf("%d\n", pct), timeout, 0, true);
}

void CommandConnection::postEventToAppAndWaitForReply(QEvent *e) {
    int evtType = (int)e->type();
    resp = QString::null;    
    gotResponse = false;
    evtResponse.clear();
    mainApp()->postEvent(mainApp(), e);
    mut.lock();
    if (!gotResponse) cond.wait(&mut);
    mut.unlock();
    
    if (gotResponse && evtResponse.isValid()) {
        switch(evtType) {
            case E_IsConsoleHidden:
                resp = QString().sprintf("%d\n",evtResponse.toInt());
                break;
            case E_GetParams:
                resp = evtResponse.toString();
                break;
            case E_SetParams:
                errMsg = evtResponse.toString();
                break;
            case E_StartACQ:
                errMsg = evtResponse.toString();
                break;
            case E_IsSaving:
                resp = QString().sprintf("%d\n",evtResponse.toInt());
                break;
            case E_GetScanCount:
                resp = QString().sprintf("%d\n",evtResponse.toInt());
                break;
            case E_GetChannelSubset:
                resp = evtResponse.toString();
                break;
        }
    }
    
    evtResponse.clear();
}

void CommandConnection::postEventToAppAndWaitForContinuousReplies(QEvent *e) {
    int evtType = (int)e->type();
    QString output = resp = QString::null;
    gotResponse = false;
    lastResponse = false;
    evtResponse.clear();    
    
    mainApp()->postEvent(mainApp(), e);
    e = 0; // invalidate pointer as it will be deleted spuriously..
    
    do {
        mut.lock();
        if (!gotResponse && !lastResponse) cond.wait(&mut);
        
        if (gotResponse && evtResponse.isValid()) {
            switch(evtType) {
                case E_Par2:
                    output = evtResponse.toString();
                    evtResponse.clear();
                    break;
            }
        }
        gotResponse = false;
        mut.unlock();
        
        if (!SockUtil::send(*sock, output, timeout, 0, true)) {
            sock->abort();
            mut.lock();
            lastResponse = true; // tell main thread to abort..            
            mut.unlock();
            postEventToAppAndWaitForReply(new CustomEvt(E_CommConnEnded, this));    ///< tell app we died so it deletes the hidden window and subprocess right away         
        }
        
    } while (!lastResponse);
    evtResponse.clear();
}

void MainApp::customEvent(QEvent *e) { ///< yes, we are implementing part of this class here!
    CommandConnection *conn = ((int)e->type()) >= E_Min ? ((CustomEvt *)e)->conn : NULL;
    if (!conn) {
        Error() << "MainApp::customEvent Connection is NULL!";
        return;
    }
    
    switch((int)e->type()) {
        case E_IsConsoleHidden:
            conn->setResponseAndWake(isConsoleHidden());
            e->accept();
            break;            
        case E_ConsoleHide:
            if (!isConsoleHidden()) hideUnhideConsole();
            conn->setResponseAndWake();
            e->accept();
            break;            
        case E_ConsoleUnhide:
            if (isConsoleHidden()) hideUnhideConsole();
            conn->setResponseAndWake();
            e->accept();
            break;      
        case E_GetParams:
            conn->setResponseAndWake(configCtl ? configCtl->acqParamsToString() : QString::null);
            e->accept();
            break;
        case E_SetParams:
            conn->setResponseAndWake(configCtl ? configCtl->acqParamsFromString(((CustomEvt *)e)->param.toString()) : QString::null);
            e->accept();
            break;
        case E_StartACQ:
            {
				doBugAcqInstead = false;
				doFGAcqInstead = false;
                QString errTitle, errMsg, err;
                if (! startAcq(errTitle, errMsg) ) {
                    err = errTitle + " " + errMsg;
                }
                conn->setResponseAndWake(err); /* err null on no error! */
            }
            e->accept();
            break;
        case E_StopACQ:
            stopTask();
            conn->setResponseAndWake();
            e->accept();
            break;
        case E_Par2: 
            {
                QStringList toks = ((CustomEvt *)e)->param.toStringList();
                if (toks.count() >= 2) {
                    Par2Window *win = new Par2Window(0);
                    win->hide(); // make sure it's hidden!
                    par2WinConnMap[win] = conn;
                    QString cmd = toks.front();
                    toks.pop_front();
                    QString file = toks.join(" ");
                    if (!QFileInfo(file).isAbsolute()) {
                        file = outputDirectory() + "/" + file;
                    }
                    Connect(win, SIGNAL(subprocessEnded()), this, SLOT(par2WinForCommandConnectionEnded()));
                    Connect(win, SIGNAL(gotLines(const QString &)), this, SLOT(par2WinForCommandConnectionGotLines(const QString &)));
                    Connect(win, SIGNAL(subprocessError(const QString &)), this, SLOT(par2WinForCommandConnectionError(const QString &)));
                    QString err = win->startOperation(cmd, file);
                    if (!err.isNull()) {
                        conn->appendContinuousResponseAndWake(err);
                        conn->appendContinuousResponseAndWake(QString::null);
                        par2WinConnMap.remove(win);
                        delete win, win = 0;
                    }
                }                
            }
            e->accept();
            break;
        case E_CommConnEnded:
            {
                for (QMap<Par2Window *, CommandConnection *>::iterator it = par2WinConnMap.begin();
                     it != par2WinConnMap.end(); ++it) {
                    if (it.value() == conn) {
                        delete it.key();
                        par2WinConnMap.erase(it);
                        Debug() << "CommandConnection ended, removing par2Window->conn mapping and deleting hidden window";
                        break;
                    }      
                }
            }
            e->accept();
            break;
        case E_IsSaving:
            conn->setResponseAndWake(isSaving());
            e->accept();
            break;
        case E_SetSaving:
            toggleSave(static_cast<CustomEvt *>(e)->param.toBool());
            conn->setResponseAndWake();
            e->accept();
            break;
        case E_SetSaveFile:
            setOutputFile(static_cast<CustomEvt *>(e)->param.toString());
            conn->setResponseAndWake();
            e->accept();
            break;
        case E_FastSettle:
            if (task) {
                fastSettleConns.insert(conn);
                Connect(task, SIGNAL(fastSettleCompleted()), this, SLOT(fastSettleDoneForCommandConnections()));
                doFastSettle();
            }
            e->accept();
            break;
        case E_GetScanCount:
            conn->setResponseAndWake(tempDataFile().getScanCount());
            e->accept();
            break;
        case E_GetChannelSubset:
            conn->setResponseAndWake(tempDataFile().getChannelSubset());
            e->accept();
            break;
        default:
            e->ignore();
            Warning() << "Unknown event type: " << (int)e->type();
            break;            
    }
}

void MainApp::par2WinForCommandConnectionEnded() {
    Par2Window * win = (Par2Window *)sender();
    CommandConnection *conn = par2WinConnMap[win];
    if (!conn) {
        Debug() << "Par2Window " << win << " lacks corresponding conn object, probably connection ended earlier";
    } else {
        conn->appendContinuousResponseAndWake(); // says "that's all folks!!"
    }    
    disconnect(win, 0, 0, 0);
    win->deleteLater();
    par2WinConnMap.remove(win);
}

void MainApp::par2WinForCommandConnectionError(const QString & e) {
    Par2Window * win = (Par2Window *)sender();
    CommandConnection *conn = par2WinConnMap[win];
    if (!conn) {
        Debug() << "Par2Window " << win << " lacks corresponding conn object, probably connection ended earlier";
    } else {
        conn->appendContinuousResponseAndWake(e + "\n"); 
        conn->appendContinuousResponseAndWake(); // says "that's all folks!!"
    }    
    disconnect(win, 0, 0, 0);
    win->deleteLater();
    par2WinConnMap.remove(win);
}


void MainApp::par2WinForCommandConnectionGotLines(const QString & lines) {
    Par2Window * win = (Par2Window *)sender();
    CommandConnection *conn = par2WinConnMap[win];
    if (!conn) {
        Debug() << "Par2Window " << win << " lacks corresponding conn object, probably connection ended earlier";        
        disconnect(win, 0, 0, 0);
        delete win;
        par2WinConnMap.remove(win);
    } else {
        conn->appendContinuousResponseAndWake(lines); 
    }    
}

void MainApp::fastSettleDoneForCommandConnections() {
    QSet<CommandConnection *> conns = fastSettleConns;
    fastSettleConns.clear();
    if (task) disconnect(task, SIGNAL(fastSettleCompleted()), this, SLOT(fastSettleDoneForCommandConnections()));
    for (QSet<CommandConnection *>::iterator it = conns.begin(); it != conns.end(); ++it) {
        (*it)->setResponseAndWake();
    }
}

void CommandConnection::setResponseAndWake(const QVariant & r) 
{ mut.lock(); evtResponse = r; gotResponse = true; cond.wakeOne(); mut.unlock(); }

void CommandConnection::appendContinuousResponseAndWake(const QString & l) 
{ 
    mut.lock(); 
    QString evr = evtResponse.toString();
    evr.append(l);
    evtResponse = evr;
    gotResponse = true; 
    if (l.isNull()) lastResponse = true;
    cond.wakeOne(); 
    mut.unlock(); 
}

