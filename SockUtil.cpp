/*
 *  SockUtil.cpp
 *  SpikeGL
 *
 *  Created by calin on 5/18/10.
 *  Copyright 2010 Calin Culianu <calin.culianu@gmail.com>. All rights reserved.
 *
 */
#include "SockUtil.h"
#include "Util.h"
#include <QMutex>
#include <QThreadStorage>

#define LINELEN 65536

namespace SockUtil 
{
    QString errorToString(QAbstractSocket::SocketError e) {
        switch (e) {
            case QAbstractSocket::ConnectionRefusedError: 
                return "The connection was refused by the peer (or timed out).";
            case QAbstractSocket::RemoteHostClosedError:
                return "The remote host closed the connection.";
            case QAbstractSocket::HostNotFoundError:
                return "The host address was not found.";
            case QAbstractSocket::SocketAccessError:
                return "The socket operation failed because the application lacked the required privileges.";
            case QAbstractSocket::SocketResourceError:
                return "The local system ran out of resources (e.g., too many sockets).";
            case QAbstractSocket::SocketTimeoutError:
                return "The socket operation timed out.";
            case QAbstractSocket::DatagramTooLargeError:
                return "The datagram was larger than the operating system's limit.";
            case QAbstractSocket::AddressInUseError:
                return "The address specified to bind() is already in use and was set to be exclusive.";
            case QAbstractSocket::SocketAddressNotAvailableError:
                return "The address specified to bind() does not belong to the host.";
            case QAbstractSocket::UnsupportedSocketOperationError:
                return "The requested socket operation is not supported by the local operating system (e.g., lack of IPv6 support).";
            case QAbstractSocket::ProxyAuthenticationRequiredError:
                return "The socket is using a proxy, and the proxy requires authentication.";
            default:
                return "An unidentified error occurred.";
        }
        return QString::null; // not reached
    }
    
    //QMutex *sockContextMut = 0;
    QThreadStorage<QStringList *> sockContextNames;
    
    void pushContext(const QString & n) { 
        //if (!sockContextMut) sockContextMut = new QMutex;
        //QMutexLocker l(sockContextMut);
        if (!sockContextNames.hasLocalData()) {
            sockContextNames.setLocalData(new QStringList);
        }
        sockContextNames.localData()->push_back(n); 
    }
    void popContext() { 
        //if (!sockContextMut) sockContextMut = new QMutex;
        //QMutexLocker l(sockContextMut);
        if (sockContextNames.hasLocalData() && sockContextNames.localData()->count()) {
            sockContextNames.localData()->pop_back(); 
        }
    }
    QString contextName() {
        //if (!sockContextMut) sockContextMut = new QMutex;
        //QMutexLocker l(sockContextMut);
        return sockContextNames.hasLocalData() && sockContextNames.localData()->count() ? sockContextNames.localData()->back() : "Unknown Context";
    }    
    
    bool send(QTcpSocket & sock, const QString & msg, int timeout_msecs, QString * errStr_out, bool debugPrint)
    {
        if (debugPrint) {
            Debug() << "Sending '" << msg.trimmed() << "'";
        }
        sock.write(msg.toUtf8());
        if (sock.bytesToWrite() && !sock.waitForBytesWritten(timeout_msecs)) {
            QString estr = errorToString(sock.error());
            if (errStr_out) *errStr_out = estr;
            Error() << contextName()  << " failed write with socket error: " << estr;
            return false;
        }        
        //Debug() << "notify_write: " << msg;
        return true;
    }
    
    QString readLine(QTcpSocket & sock, int timeout_msecs, QString * errStr_out) 
    {
        // read greeting from server        
        for (int ct = 0; !sock.canReadLine(); ++ct) {
            if (!sock.isValid() || sock.state() != QAbstractSocket::ConnectedState || !sock.waitForReadyRead(timeout_msecs) || ct >= 3) {
                if (!errStr_out) Error() << contextName() << " timeout or peer shutdown";
                else *errStr_out = "timeout or peer shutdown";
                return QString::null;
            }
        }
        QString line = sock.readLine(LINELEN);
        line = line.trimmed();
        //Debug() << "notify_read: " << line;
        return line;
    }    
}
