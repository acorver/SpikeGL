#ifndef Util_H
#define Util_H

#include <QtCore>
#include <QMutex>
#include <QObject>
#include <QColor>
#include <QTextStream>
#include <vector>
#if defined(Q_OS_WIN32) || defined(Q_OS_WIN64)
/* For Qt4 -> Qt5 compat */
#  ifndef Q_OS_WIN
#    define Q_OS_WIN
#  endif
#  ifndef Q_OS_WINDOWS
#    define Q_OS_WINDOWS
#  endif
#  ifndef Q_WS_WIN
#    define Q_WS_WIN
#  endif
#  ifndef Q_WS_WINDOWS
#    define Q_WS_WINDOWS
#  endif
#  if defined(Q_OS_WIN32)
#    ifndef Q_WS_WIN32
#      define Q_WS_WIN32
#    endif
#  endif
#  if defined(Q_OS_WIN64)
#    ifndef Q_WS_WIN64
#      define Q_WS_WIN64
#    endif
#  endif
#endif

class MainApp;

#ifndef MIN
#define MIN(a,b) ( (a) <= (b) ? (a) : (b) )
#endif
#ifndef MAX
#define MAX(a,b) ( (a) >= (b) ? (a) : (b) )
#endif

#define STR1(x) #x
#define STR(x) STR1(x)

#define EPSILON 0.0000001
#define EPSILONf 0.0001

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
 void Connect(QObject *srcobj, const QString & src_signal, 
                    QObject *destobj, const QString & dest_slot);

 MainApp *mainApp();

 /// retrieve the task read freq to use for acquisition -- this affects how often we poll the board for data and also affects latency
 int getTaskReadFreqHz();
	
/// retrieve a time value from the system's high resolution timer
 double getTime();

/// retrieve a time value from the system's high resolution timer -- the absolute time since system boot is returns, in nanoseconds
/// implemented to attempt to do synch in the StimGL/SpikeGL frame share mechanism
  u64 getAbsTimeNS();
	
/// returns the number of real CPUs (cores) on the system
 unsigned getNProcessors(); 

/// returns the PID of the current process.
extern unsigned getPid();
}
/// sets the process affinity mask -- a bitset of which processors to run on
extern "C" void setProcessAffinityMask(unsigned mask);

namespace Util {
/// returns the current host name
 QString getHostName();

/// turns off Nagle algorithm for socket sock
 void socketNoNagle(int sock);

/// \brief std::rand() based random number from [min, max].
///
/// You don't normally want to use this.  See the RNG class instead.
 double random(double min = 0., double max = 1.);

/// returns a QString signifying the last error error
 QString glGetErrorString(int err);

/// returns the number of seconds since this machine last rebooted
 unsigned getUpTime();

/// `Pins' the current thread to a particular set of processors, that is, makes it only run on a particular set of processors.
/// returns 0 on error, or the previous mask on success.
 unsigned setCurrentThreadAffinityMask(unsigned cpu_mask);

/// Sets the application process to use 'realtime' priority, which means
/// it won't be pre-emptively multitasked by lower-priority processes.  Implemented in osdep.cpp
 void setRTPriority();

/// returns the filename without the last extension component, if any
 QString baseName(const QString &fname);

/// Enable/disable vertical sync in OpenGL.  Defaults to on on Windows, 
/// off on Linux. Make sure the GL context is current when you call this!
 void setVSyncMode(bool onoff, bool printToLog = false);

/// Returns true if the implementation has the named extension.
 bool hasExt(const char *ext_name);

/// Same as libc ffs, except we implement it ourselves because of Wind0ze
 int ffs(int x);

/// Returns true if object has parent or grandparent, etc equal to ancestor.  This is a way to test if an object is contained (in a nested fashion) in another object.
 bool objectHasAncestor(QObject *object, const QObject *ancestor);

/// Returns the amount of available space on the disk (in MB)
 quint64 availableDiskSpace();

 /// Removes all data temporary files (SpikeGL_DSTemp_*.bin) fromn the TEMP directory
 void removeTempDataFiles();

 /// true iff the difference between a and b is smaller than EPSILON (0.0000001)
 bool feq(double a, double b, double epsilon = EPSILON);
 bool feqf(float a, float b, float epsilon = EPSILONf);
	
/// rotate right 'moves' bits. Different from >> shift operator in that it rotates in bits from right side to left side
template<class T>
T ror(T x, unsigned int moves)
{
	return (x >> moves) | (x << (sizeof(T)*8 - moves));
}
/// rotate left 'moves' bits. Different from << shift operator in that it rotates in bits from left side to right side
template<class T>
T rol(T x, unsigned int moves)
{
	return (x << moves) | (x >> (sizeof(T)*8 - moves));
}
	
 /// Resample (internally uses Secret Rabbit Code samplerate lib)
 class Resampler {
public:
	 enum Algorithm { SincBest = 0, SincMedium, SincFastest, ZeroOrderHold, Linear, };

	 /// Do resampling using selected algorithm.  Note that ratio is outputrate / inputputrate. Ratio of 1.0 returns the same data, < 1.0 downsamples, > 1.0 upsamples
	 /// returns empty string on success, or an error message from SRC sample lib on failure (and output is set to 0 size in that case)
	 static QString resample(const std::vector<int16> & input, std::vector<int16> & output, double ratio, int numChannelsPerFrame = 1, Algorithm = SincFastest, bool isEndOfInput = false); 

	 Resampler(double ratio, int numChannelsPerFrame=1, Algorithm=SincFastest);
	 ~Resampler();

	 void changeRatio(double newRatio);
	 double ratio() const;
	 QString resample(const std::vector<int16> & input, std::vector<int16> & output, bool isEndOfInput = false);

 private:
	 bool chk() const;
	 struct Impl;
	 Impl *p; ///< pimpl idiom.. :)
 };


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
	
class Avg {
	double avg;
	unsigned navg, nlim;
public:
	Avg() { reset(); }
	void reset(unsigned n=10) { avg = 0.0, navg = 0, setN(n); }
	void setN(unsigned n) { if (!n) n = 1; nlim = n; if (navg >= nlim) navg = nlim; }
	unsigned N() const { return nlim; }
	double operator()(double x) { 
		if (navg >= nlim && navg) { avg = avg - avg/double(navg);  navg = nlim-1; }
		avg = avg + x/double(++navg);
		return avg;
	}
	double operator()() const { return avg; }
};

} // end namespace Util

using namespace Util;

#endif
