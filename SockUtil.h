/*
 *  SockUtil.h
 *  SpikeGL
 *
 *  Created by calin on 5/18/10.
 *  Copyright 2010 Calin Culianu <calin.culianu@gmail.com>. All rights reserved.
 *
 */
#ifndef SOCK_UTIL_H
#define SOCK_UTIL_H
#include <QTcpSocket>
#include <QStringList>

namespace SockUtil 
{
    QString errorToString(QAbstractSocket::SocketError e);    
    void pushContext(const QString & n);
    void popContext();
    QString contextName();
    
    bool send(QTcpSocket & sock, const QString & msg, int timeout_msecs,
              QString * errStr_out = 0, bool debugPrintMsg = false);
    
    QString readLine(QTcpSocket & sock, int timeout_msecs, QString * errStr_out = 0);
    
    struct Context { Context(const QString &ctx) { pushContext(ctx); }  ~Context() { popContext(); } };
}

#endif

