#include "Util.h"

#include <qglobal.h>
#include <QGLContext>

#ifdef Q_OS_WIN
#include <winsock.h>
#include <io.h>
#include <windows.h>
#include <wingdi.h>
#include <GL/gl.h>
#endif

#ifdef Q_WS_X11
#include <GL/gl.h>
#include <GL/glx.h>
// for XOpenDisplay
#include <X11/Xlib.h>
// for sched_setscheduler
#endif

#ifdef Q_WS_MACX
#include <agl.h>
#include <gl.h>
#endif

#ifdef Q_OS_LINUX
#include <sched.h>
// for getuid, etc
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#endif

#include <string.h>
#include <iostream>
#include <QHostInfo>

namespace {
    struct Init {
        Init() {
            getTime(); // make the gettime function remember its t0
        }
    };
    Init init;
};

namespace Util {

#ifdef Q_OS_WIN
void setRTPriority()
{
    Log() << "Setting process to realtime";
    if ( !SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS) ) 
        Error() << "SetPriorityClass() call failed: " << (int)GetLastError();    
}
double getTime()
{
    static __int64 freq = 0;
    static __int64 t0 = 0;
    __int64 ct;

    if (!freq) {
        QueryPerformanceFrequency((LARGE_INTEGER *)&freq);
    }
    QueryPerformanceCounter((LARGE_INTEGER *)&ct);   // reads the current time (in system units)
    if (!t0) t0 = ct;
    return double(ct-t0) / double(freq);
}

#elif defined(Q_OS_LINUX)
void setRTPriority()
{
    if (geteuid() == 0) {
        Log() << "Running as root, setting priority to realtime";
        if ( mlockall(MCL_CURRENT|MCL_FUTURE) ) {
            int e = errno;
            Error() <<  "Error from mlockall(): " <<  strerror(e);
        }
        struct sched_param p;
        p.sched_priority = sched_get_priority_max(SCHED_RR);
        if ( sched_setscheduler(0, SCHED_RR, &p) ) {
            int e = errno;
            Error() << "Error from sched_setscheduler(): " <<  strerror(e);
        }
    } else {
        Warning() << "Not running as root, cannot set priority to realtime";
    }    
}

double getTime()
{
        static double t0 = -9999.;
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        double t = double(ts.tv_sec) + double(ts.tv_nsec)/1e9;
        if (t0 < 0.) t0 = t; 
        return t-t0;
}
#else /* !WIN and !LINUX */
void setRTPriority()
{
    Warning() << "Cannot set realtime priority -- unknown platform!";
}
} // end namespace util
#include <QTime>
namespace Util {
double getTime()
{
    static QTime t;
    static bool started = false;
    if (!started) { t.start(); started = true; }
    return double(t.elapsed())/1000.0;
}

#endif 

#ifdef Q_OS_WIN
unsigned getNProcessors()
{
    static int nProcs = 0;
    if (!nProcs) {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        nProcs = si.dwNumberOfProcessors;
    }
    return nProcs;
}
#elif defined(Q_OS_LINUX)
} // end namespace util
#include <unistd.h>
namespace Util {
unsigned getNProcessors()
{
    static int nProcs = 0;
    if (!nProcs) {
        nProcs = sysconf(_SC_NPROCESSORS_ONLN);
    }
    return nProcs;
}
#elif defined(Q_OS_DARWIN)
} // end namespace util
#include <CoreServices/CoreServices.h>
namespace Util {
unsigned getNProcessors() 
{
    static int nProcs = 0;
    if (!nProcs) {
        nProcs = MPProcessorsScheduled();
    }
    return nProcs;
}
#else
unsigned getNProcessors()
{
    return 1;
}
#endif

QString getHostName()
{
    return QHostInfo::localHostName();
}

#ifdef Q_OS_WIN

void socketNoNagle(int sock)
{
    BOOL flag = 1;
    int ret = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char *>(&flag), sizeof(flag));
    if (ret) Error() << "Error turning off nagling for socket " << sock;
}
#else
} // end namespace util
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <netinet/tcp.h>
#  include <arpa/inet.h>
namespace Util {
void socketNoNagle(int sock)
{
    long flag = 1;
    int ret = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<void *>(&flag), sizeof(flag));
    if (ret) Error() << "Error turning off nagling for socket " << sock;
}
#endif

#ifdef Q_OS_WIN
unsigned getUpTime()
{
    return GetTickCount() / 1000;
}
#elif defined(Q_OS_LINUX)
} // end namespace Util
#include <sys/sysinfo.h>
namespace Util {
unsigned getUpTime()
{
    struct sysinfo si;
    sysinfo(&si);
    return si.uptime;
}
#else
unsigned getUpTime()
{
    return getTime();
}
#endif


#ifdef Q_OS_WIN

unsigned setCurrentThreadAffinityMask(unsigned mask)
{
	HANDLE h = GetCurrentThread();
	DWORD_PTR prev_mask = SetThreadAffinityMask(h, (DWORD_PTR) mask);
	return static_cast<unsigned>(prev_mask);
}

#else
unsigned  setCurrentThreadAffinityMask(unsigned mask)
{
	(void)mask;
	Error() << "setCurrentThreadAffinityMask() unimplemented on this platform!";
	return 0;
}
#endif

}
