#include "StimGL_SpikeGL_Integration.h"
#include "Util.h"
#include "SockUtil.h"
#include <QApplication>
#include <QTcpSocket>
#include <QHostAddress>
#include <QSharedMemory>

#define GREETING_STRING "HELLO I'M SPIKEGL"
#define PLUGIN_START_STRING "PLUGIN START" 
#define PLUGIN_END_STRING   "PLUGIN END"
#define PLUGIN_PARAMS_STRING "PLUGIN PARAMS"
#define PLUGIN_PARAMS_END_STRING "END PLUGIN PARAMS"
#define OK_STRING "OK"

typedef QMap<QString, QVariant> PM;
Q_DECLARE_METATYPE(PM);

namespace StimGL_SpikeGL_Integration 
{
    
    static bool doNotify (bool isStart,   
                          bool isEnd,
                          const QString & pname, 
                          const QMap<QString, QVariant>  &pparms,
                          QString *errStr_out,
                          const QString & host,
                          unsigned short port, 
                          int timeout_msecs)
    {
        Debug() << "Notifying SpikeGL of `" << pname << "' " << (isStart ? "start" : (isEnd ? "end" : "params")) << "...";
        SockUtil::Context ctx("Notify SpikeGL");
        QTcpSocket sock;

        sock.connectToHost(host, port);
        if (!sock.waitForConnected(timeout_msecs) || !sock.isValid()) {
            QString estr = SockUtil::errorToString(sock.error());
            if (errStr_out) *errStr_out = estr;            
            QString logoutput = SockUtil::contextName() + " failed to notify; " + estr;
            if (sock.error() == QAbstractSocket::ConnectionRefusedError) 
                Debug() << logoutput;
            else
                Error() << logoutput;
            return false;
        }
        
        QString line = SockUtil::readLine(sock, timeout_msecs, errStr_out);
        if (line.isNull()) return false;
        if (isStart || isEnd)  {
            if (!SockUtil::send(sock, QString(isStart ? PLUGIN_START_STRING : PLUGIN_END_STRING) + " " + pname.trimmed() + "\n", timeout_msecs, errStr_out)) 
                return false;
        }
        if (!SockUtil::send(sock, QString(PLUGIN_PARAMS_STRING) + "\n", timeout_msecs, errStr_out))  return false;
        for (QMap<QString, QVariant> ::const_iterator it = pparms.begin();
             it != pparms.end();
             ++it) {
            if (!SockUtil::send(sock, QString("%1 = %2\n").arg(it.key()).arg(it.value().toString()), timeout_msecs, errStr_out)) return false;
        }
        if (!SockUtil::send(sock, QString("%1\n").arg(PLUGIN_PARAMS_END_STRING), timeout_msecs, errStr_out)) return false;
        line = SockUtil::readLine(sock, timeout_msecs, errStr_out);
        if (line.isNull()) return false; 
        if (!line.startsWith(OK_STRING)) {
            QString estr = QString("Did not read OK from SpikeGL after sending plugin ") + (isStart ? "start" : "end") + " msg!";
            if (errStr_out) *errStr_out = estr;
            Error() << "Notify SpikeGL failed: " << estr;
            return false;
        }
        Debug() << "SpikeGL notified of plugin `" << pname << "' " << (isStart ? "start" : "end") << " via socket!";
        return true;
    }

    bool Notify_PluginStart( const QString & pname, 
                             const QMap<QString, QVariant>  &pparms,
                             QString *errStr_out,
                             const QString & host,
                             unsigned short port, 
                             int timeout_msecs)
    {
        return doNotify(true,  false, pname, pparms, errStr_out, host, port, timeout_msecs);
    }

    bool Notify_PluginEnd  (const QString & pname, 
                            const QMap<QString, QVariant>  &pparms,
                            QString *errStr_out,
                            const QString & host,
                            unsigned short port, 
                            int timeout_msecs)
    {
        return doNotify(false,  true, pname, pparms, errStr_out, host, port, timeout_msecs);
    }

    bool Notify_PluginParams  (const QString & pname, 
                               const QMap<QString, QVariant>  &pparms,
                               QString *errStr_out,
                               const QString & host,
                               unsigned short port, 
                               int timeout_msecs)
    {
        return doNotify(false, false, pname, pparms, errStr_out, host, port, timeout_msecs);
    }
    
    void NotifyServer::processConnection(QTcpSocket & sock)
    {
        QString line;
        SockUtil::Context ctx("NotifyServerThread");
        line = QString(GREETING_STRING) + "\n";
        bool isStart = false, isEnd = false, isParams = false;

        if (!SockUtil::send(sock, line, timeout_msecs)) return;
        if ((line = SockUtil::readLine(sock, timeout_msecs)).isNull()) return;
        if (   !(isStart = line.startsWith(PLUGIN_START_STRING)) 
            && !(isEnd = line.startsWith(PLUGIN_END_STRING))
            && !(isParams = line.startsWith(PLUGIN_PARAMS_STRING)) ) {
            Error() << SockUtil::contextName() << " parse error expected: {" << PLUGIN_START_STRING << "|" << PLUGIN_END_STRING << "} got: " << line;
            return;
        }
        if (isStart)
            line = line.mid(QString(PLUGIN_START_STRING).length());
        else if (isEnd)
            line = line.mid(QString(PLUGIN_END_STRING).length());
        line = line.trimmed();
        QString pluginName = "unknown";
        if (isStart || isEnd) {
            pluginName = line;
            if ((line = SockUtil::readLine(sock, timeout_msecs)).isNull()) return;
            if (!line.startsWith(PLUGIN_PARAMS_STRING)) {
                Error() << SockUtil::contextName() << " parse error expected: " << PLUGIN_PARAMS_STRING << "got: " << line;
                return;
            }
        }
        // at this point all modes read params ...
        QMap<QString, QVariant>  params;
        while (  !(line = SockUtil::readLine(sock, timeout_msecs)).startsWith(PLUGIN_PARAMS_END_STRING)) {
            QStringList l = line.trimmed().split("=");
            if (l.count() < 2) { Debug() << "skipping params line that does not contain '=': " << line; continue; }
            QString n = l.front().trimmed();
            l.pop_front();
            QString v = l.join("=").trimmed();
            params[n] = v;
        }
        line = QString(OK_STRING) + "\n";
        if (!SockUtil::send(sock, line, timeout_msecs)) return;
        emitGotPluginNotification(isStart, isEnd, pluginName, params);
        //Log() << "Received plugin " << (isStart ? "start" : "end") << " notificaton from StimGL for plugin " << pluginName;
    }
 
    void NotifyServer::gotNewConnection() 
    {
        QTcpSocket *sock = srv.nextPendingConnection();
        processConnection(*sock);
        sock->close();
        delete sock;
    }

    static bool mapMetaTypeRegistered = false;

    NotifyServer::NotifyServer(QObject *parent) 
        : QObject(parent)
    {
        if (!mapMetaTypeRegistered) {
            qRegisterMetaType<QMap<QString, QVariant> >();
            mapMetaTypeRegistered = true;
        }
        Connect(&srv, SIGNAL(newConnection()), this, SLOT(gotNewConnection()));
    }

    NotifyServer::~NotifyServer()
    {
    }

    
    bool NotifyServer::beginListening(const QString & iface, unsigned short port, int tout) {
        timeout_msecs = tout;        
        QHostAddress haddr;
        if (iface == "0.0.0.0") haddr = QHostAddress(QHostAddress::Any);
        else if (iface == "localhost" || iface == "127.0.0.1") haddr = QHostAddress(QHostAddress::LocalHost);
        else haddr = iface;
        if (!srv.listen(haddr, port)) {
            Error() << "NotifyServer could not listen: " << srv.errorString();
            return false;
        }
        Log() << "StimGL notification server listening on " << haddr.toString() << ":" << port;
        return true;
    }

    void NotifyServer::emitGotPluginNotification(bool isStart, bool isEnd, const QString &p, const QMap<QString, QVariant>  &pp) {
        if (isStart)
            emit gotPluginStartNotification(p, pp);
        else if (isEnd)
            emit gotPluginEndNotification(p, pp);
        else
            emit gotPluginParamsNotification(p, pp);
    } 
	
	
	FrameShare::FrameShare() 
	{
		qsm = new QSharedMemory(QString("%1").arg(FRAME_SHARE_SHM_MAGIC));
		shm = 0;
		createdByThisInstance = false;
		if (!qsm->isAttached() && !qsm->attach() && qsm->create(FRAME_SHARE_SHM_SIZE)) {
			if (lock()) {
				shm = (volatile FrameShareShm *)qsm->data();
				memset((void *)shm, 0, FRAME_SHARE_SHM_SIZE);
				shm->magic = FRAME_SHARE_SHM_MAGIC;
				unlock();
				createdByThisInstance = true;
			} else {
				Error() << "INTERNAL ERROR: Could not lock frame share shm after create!";
			}
		}
		if (!qsm->isAttached())
			Error() << "INTERNAL ERROR: 'frame share' shared memory segment cannot be attached/created due to the following reason: " << qsm->errorString();
		else  {
			shm = const_cast<volatile FrameShareShm *>(reinterpret_cast<FrameShareShm *>(qsm->data()));
			if (qsm->size() != FRAME_SHARE_SHM_SIZE) {
				Error() << "INTERNAL ERROR: 'frame share' shared memory segment attached correctly but it has an incorrect size. Detaching.";
				detach();
				shm = 0;
			} else if (shm->magic != FRAME_SHARE_SHM_MAGIC) {
				Error() << "INTERNAL ERROR: 'frame share' shared memory segment attached correctly but it appears corrupted. Detaching.";
				detach();
				shm = 0;
			} 
		}
	}
	
	FrameShare::~FrameShare() 
	{ 
		detach();
		delete qsm, qsm = 0;
	}
	
	void FrameShare::detach()
	{
		if (qsm->isAttached()) qsm->detach();
		shm = 0;
	}
	
	bool FrameShare::lock() 
	{
		return qsm->lock();
	}
	
	bool FrameShare::unlock()
	{
		return qsm->unlock();
	}
	
	int FrameShare::size() const { return qsm->size(); }
}
