/*
 *  CommandServer.h
 *  SpikeGL
 *
 *  Created by calin on 5/18/10.
 *  Copyright 2010 Calin Culianu <calin.culianu@gmail.com>. All rights reserved.
 *
 */
#ifndef COMMAND_SERVER_H
#define COMMAND_SERVER_H

#define DEFAULT_COMMAND_PORT 4142 /**< the port of the 'command' server */
#define DEFAULT_COMMAND_TIMEOUT_MS 10000

#include <QObject>
#include <QTcpServer>
#include <QEvent>
#include <QString>

class MainApp;
class CommandConnection;

class CommandServer : protected QTcpServer {
public:
    CommandServer(MainApp *parentApp);
    ~CommandServer();
    
    bool beginListening(const QString & iface = "0.0.0.0", unsigned short port = DEFAULT_COMMAND_PORT, unsigned timeout_ms = DEFAULT_COMMAND_TIMEOUT_MS);
   
protected:    
    void incomingConnection (int socketDescr); ///< reimplemented from QTcpServer    
    
public:
    static void deleteAllActiveConnections();

private:
    MainApp *app;
    int timeout_msecs;    
};


#endif


