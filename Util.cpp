#include <QObject>
#include <QString>
#include <QApplication>
#include <QMessageBox>
#include <cstdlib>
#include <ctime>
#include <QMutex>
#include <QTextEdit>
#include <QTime>
#include <QThread>
#include <iostream>
#include "Util.h"
#ifdef Q_OS_WIN
#include <windows.h>
#include <wingdi.h>
#endif
#ifdef Q_WS_MACX
#  include <gl.h>
#else
#  include <GL/gl.h>
#endif
#include "MainApp.h"
#include "ConfigureDialogController.h"

namespace Util {

void Connect(QObject *srco, const QString & src, QObject *desto, const QString & dest)
{
    if (!QObject::connect(srco, src.toUtf8(), desto, dest.toUtf8(), Qt::QueuedConnection)) {
        QString tmp;
        QMessageBox::critical(0, "Signal connection error", QString("Error connecting %1::%2 to %3::%4").arg( (tmp = srco->objectName()).isNull() ? "(unnamed)" : tmp ).arg(src.mid(1)).arg( (tmp = desto->objectName()).isNull() ? "(unnamed)" : tmp ).arg(dest.mid(1)));
        QApplication::exit(1);
        // only reached if event loop is not running
        std::exit(1);
    }
}

/// returns a QString signifying the last error error
QString glGetErrorString(int err)
{
    switch (err) {
    case GL_INVALID_OPERATION:
        return "Invalid Operation";
    case GL_INVALID_ENUM:
        return "Invalid Enum";
    case GL_NO_ERROR:
        return "No Error";
    case GL_INVALID_VALUE:
        return "Invalid Value";
    case GL_OUT_OF_MEMORY:
        return "Out of Memory";
    case GL_STACK_OVERFLOW:
        return "Stack Overflow";
    case GL_STACK_UNDERFLOW:
        return "Stack Underflow";
    }
    return QString("UNKNOWN: ") + QString::number(err);
}

QString baseName(const QString &fname)
{
    QStringList l = fname.split(".");
    if (l.size() <= 1) return fname;
    
    l.removeLast();
    return l.join(".");
}

MainApp *mainApp() 
{
    return MainApp::instance();
}

/// public global function.  I hate globals but necessary I guess
int getTaskReadFreqHz() 
{
	MainApp *app = mainApp();
	ConfigureDialogController *cfgctl = 0;
	if ( app 
		&& (cfgctl = app->configureDialogController())
		&& cfgctl->acceptedParams.lowLatency ){
		return DEF_TASK_READ_FREQ_HZ_*3;
	}
	return DEF_TASK_READ_FREQ_HZ_;
}
	
int ffs(int x)
{
        int r = 1;

        if (!x)
                return 0;
        if (!(x & 0xffff)) {
                x >>= 16;
                r += 16;
        }
        if (!(x & 0xff)) {
                x >>= 8;
                r += 8;
        }
        if (!(x & 0xf)) {
                x >>= 4;
                r += 4;
        }
        if (!(x & 3)) {
                x >>= 2;
                r += 2;
        }
        if (!(x & 1)) {
                x >>= 1;
                r += 1;
        }
        return r;
}

bool objectHasAncestor(QObject *o, const QObject *a)
{
    while (o) {
        o = o->parent();
        if (o == a) return true;        
    }
    return false;
}


Log::Log()
    : doprt(true), str(""), s(&str, QIODevice::WriteOnly)
{
}

Log::~Log()
{    
    if (doprt) {        
        s.flush(); // does nothing probably..
        QString theString = QString("[Thread ") + QString::number((unsigned long)QThread::currentThreadId()) + " "  + QDateTime::currentDateTime().toString("M/dd/yy hh:mm:ss.zzz") + "] " + str;

        if (mainApp()) {
            mainApp()->logLine(theString, color);
        } else {
            // just print to console for now..
            std::cerr << theString.toUtf8().constData() << "\n";
        }
    }
}

Debug::~Debug()
{
    if (!mainApp() || !mainApp()->isDebugMode())
        doprt = false;
    color = Qt::darkBlue;
}


Error::~Error()
{
    color = Qt::darkRed;
    if (mainApp() && mainApp()->isConsoleHidden())
        Systray(true) << str; /// also echo to system tray!
}

Warning::~Warning()
{
    color = Qt::darkMagenta;
    if (mainApp() && mainApp()->isConsoleHidden())
        Systray(true) << str; /// also echo to system tray!
}


double random(double min, double max)
{
    static bool seeded = false;
    if (!seeded) { seeded = true; qsrand(std::time(0));  }
    int r = qrand();
    return double(r)/double(RAND_MAX-1) * (max-min) + min;
}

Status::Status(int to)
    : to(to), str(""), s(&str, QIODevice::WriteOnly)
{
    s.setRealNumberNotation(QTextStream::FixedNotation);
    s.setRealNumberPrecision(2);
}

Status::~Status()
{
    if (!str.length()) return;
    if (mainApp()) mainApp()->setSBString(str, to);
    else {
        std::cerr << "STATUSMSG: " << str.toUtf8().constData() << "\n";
    }
}

Systray::Systray(bool err, int to)
    : Status(to), isError(err)
{
}

Systray::~Systray()
{
    if (mainApp()) mainApp()->sysTrayMsg(str, to, isError);
    else {
        std::cerr << "SYSTRAYMSG: " << str.toUtf8().constData() << "\n";
    }
    str = ""; // clear it for superclass c'tor
}

} // end namespace Util
