#ifndef Util_H
#define Util_H

#include <QtCore>
#include <QMutex>
#include <QObject>
#include <QColor>
#include <QTextStream>
class MainApp;

#ifndef MIN
#define MIN(a,b) ( (a) <= (b) ? (a) : (b) )
#endif
#ifndef MAX
#define MAX(a,b) ( (a) >= (b) ? (a) : (b) )
#endif

#define STR1(x) #x
#define STR(x) STR1(x)

#ifdef Q_OS_WIN
#  define PATH_SEPARATOR "/" /**< new qt uses this */
#else
#  define PATH_SEPARATOR "/"
#endif

#include "TypeDefs.h"

/// Various global utility functions used application-wide.
namespace Util 
{

/** Just like QObject::connect except it will popup an error box if there
    was a problem connecting, and exit the program immediately */
extern void Connect(QObject *srcobj, const QString & src_signal, 
                    QObject *destobj, const QString & dest_slot);

extern MainApp *mainApp();

/// retrieve a time value from the system's high resolution timer
extern double getTime();

/// returns the number of real CPUs (cores) on the system
extern unsigned getNProcessors(); 

/// returns the current host name
extern QString getHostName();

/// turns off Nagle algorithm for socket sock
extern void socketNoNagle(int sock);

/// \brief std::rand() based random number from [min, max].
///
/// You don't normally want to use this.  See the RNG class instead.
extern double random(double min = 0., double max = 1.);

/// returns a QString signifying the last error error
extern QString glGetErrorString(int err);

/// returns the number of seconds since this machine last rebooted
extern unsigned getUpTime();

/// `Pins' the current thread to a particular set of processors, that is, makes it only run on a particular set of processors.
/// returns 0 on error, or the previous mask on success.
extern unsigned setCurrentThreadAffinityMask(unsigned cpu_mask);

/// Sets the application process to use 'realtime' priority, which means
/// it won't be pre-emptively multitasked by lower-priority processes.  Implemented in osdep.cpp
extern void setRTPriority();

/// returns the filename without the last extension component, if any
extern QString baseName(const QString &fname);

/// Enable/disable vertical sync in OpenGL.  Defaults to on on Windows, 
/// off on Linux. Make sure the GL context is current when you call this!
extern void setVSyncMode(bool onoff, bool printToLog = false);

/// Returns true if the implementation has the named extension.
extern bool hasExt(const char *ext_name);

/// Same as libc ffs, except we implement it ourselves because of Wind0ze
extern int ffs(int x);

/// Returns true if object has parent or grandparent, etc equal to ancestor.  This is a way to test if an object is contained (in a nested fashion) in another object.
extern bool objectHasAncestor(QObject *object, QObject *ancestor);

/// Super class of Debug, Warning, Error classes.  
class Log 
{
public:
    Log();
    virtual ~Log();
    
    template <class T> Log & operator<<(const T & t) {  s << t; return *this;  }
protected:
    bool doprt;
    QColor color;
    QString str;

private:    
    QTextStream s;
};

/** \brief Stream-like class to print a debug message to the app's console window
    Example: 
   \code 
        Debug() << "This is a debug message"; // would print a debug message to the console window
   \endcode
 */
class Debug : public Log
{
public:
    virtual ~Debug();
};

/** \brief Stream-like class to print an error message to the app's console window
    Example: 
   \code 
        Error() << "This is an ERROR message!!"; // would print an error message to the console window
   \endcode
 */
class Error : public Log
{
public:
    virtual ~Error();
};

/** \brief Stream-like class to print a warning message to the app's console window

    Example:
  \code
        Warning() << "This is a warning message..."; // would print a warning message to the console window
   \endcode
*/
class Warning : public Log
{
public:
    virtual ~Warning();
};

/// Stream-like class to print a message to the app's status bar
class Status
{
public:
    Status(int timeout = 0);
    virtual ~Status();
    
    template <class T> Status & operator<<(const T & t) {  s << t; return *this;  }
protected:
    int to;
    QString str;
    QTextStream s;
};

class Systray : public Status
{
public:
    Systray(bool iserror = false, int timeout = 0);
    virtual ~Systray();
protected:
    bool isError;
};

}

using namespace Util;

#endif
