#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
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
    if (!t0) {
        t0 = ct;
    }
    return double(ct-t0)/double(freq);
}
/// sets the process affinity mask -- a bitset of which processors to run on
void setProcessAffinityMask(unsigned mask)
{
    if (!SetProcessAffinityMask(GetCurrentProcess(), mask)) {
        Error() << "Error from Win32 API when setting process affinity mask: " << GetLastError();
    } else {
        Log() << "Process affinity mask set to: " << QString().sprintf("0x%x",mask);
    }
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

/// sets the process affinity mask -- a bitset of which processors to run on
void setProcessAffinityMask(unsigned mask)
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (unsigned i = 0; i < sizeof(mask)*8; ++i) {
        if (mask & 1<<i) CPU_SET(i, &cpuset);
    }
    int err = sched_setaffinity(0, sizeof(cpuset), &cpuset);
    if (err) {
        Error() << "sched_setaffinity(" << QString().sprintf("0x%x",mask) << ") error: " << strerror(errno);
    } else {
        Log() << "Process affinity mask set to: " << QString().sprintf("0x%x",mask);
    }
}

#else /* !WIN and !LINUX */
void setRTPriority()
{
    Warning() << "Cannot set realtime priority -- unknown platform!";
}
} // end namespace util

/// sets the process affinity mask -- a bitset of which processors to run on
extern "C" void setProcessAffinityMask(unsigned mask)
{
    (void)mask;
    Warning() << "`Set process affinity mask' for this platform unimplemented -- ignoring.";
}

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


static const GLubyte *strChr(const GLubyte * str, GLubyte ch)
{
    while (str && *str && *str != ch) ++str;
    return str;
}

static const char *gl_error_str(GLenum err)
{
    static char unkbuf[64];
    switch(err) {
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
    default:
        qsnprintf(unkbuf, sizeof(unkbuf), "UNKNOWN: %d", (int)err);
        return unkbuf;
    }
    return 0; // not reached
}

bool hasExt(const char *ext_name)
{
    static const GLubyte * ext_str = 0;
#ifdef Q_WS_X11
    static const char *glx_exts = 0;
#endif
    static const GLubyte space = static_cast<GLubyte>(' ');
    if (!ext_str) 
        ext_str = glGetString(GL_EXTENSIONS);
    if (!ext_str) {
        Warning() << "Argh! Could not get GL_EXTENSIONS! (" << gl_error_str(glGetError()) << ")";
    } else {
        const GLubyte *cur, *prev, *s1;
        const char *s2;
        // loop through all space-delimited strings..
        for (prev = ext_str, cur = strChr(prev+1, space); *prev; prev = cur+1, cur = strChr(prev, space)) {
            // compare strings
            for (s1 = prev, s2 = ext_name; *s1 && *s2 && *s1 == *s2 && s1 < cur; ++s1, ++s2)
                ;
            if (*s1 == *s2 || (!*s2 && *s1 == space)) return true; // voila! found it!
        }
    }

#ifdef Q_WS_X11
    if (!glx_exts) {
     // nope.. not a standard gl extension.. try glx_exts
     Display *dis;
     int screen;

     dis = XOpenDisplay((char *)0);
     if (dis) {
         screen = DefaultScreen(dis);
         const char * glx_exts_tmp = glXQueryExtensionsString(dis, screen);
         if (glx_exts_tmp)
             glx_exts = strdup(glx_exts_tmp);
         XCloseDisplay(dis);
     }
    }
     if (glx_exts) {
         const char *prev, *cur, *s1, *s2; 
         const char space = ' ';
         // loop through all space-delimited strings..
         for (prev = glx_exts, cur = strchr(prev+1, space); *prev; prev = cur+1, cur = strchr(prev, space)) {
        // compare strings
             for (s1 = prev, s2 = ext_name; *s1 && *s2 && *s1 == *s2 && s1 < cur; ++s1, ++s2)
            ;
             if (*s1 == *s2 ||  (!*s2 && *s1 == space)) return true; // voila! found it!
         }
     }
#endif
    return false;
}

#ifdef Q_WS_X11
void setVSyncMode(bool onoff, bool prt)
{
    if (hasExt("GLX_SGI_swap_control")) {
        if (prt)
            Log() << "Found `swap_control' GLX-extension, turning " << (onoff ? "on" : "off") <<  " \"wait for vsync\"";
        int (*func)(int) = (int (*)(int))glXGetProcAddressARB((const GLubyte *)"glXSwapIntervalSGI");
        if (func) {
            func(onoff ? 1 : 0);
        } else
            Error() <<  "GLX_SGI_swap_control func not found!";
    } else
        Warning() << "Missing `swap_control' GLX-extension, cannot change vsync!";
}
#elif defined(Q_WS_WIN) /* Windows */
typedef BOOL (APIENTRY *wglswapfn_t)(int);

void setVSyncMode(bool onoff, bool prt)
{
    wglswapfn_t wglSwapIntervalEXT = (wglswapfn_t)QGLContext::currentContext()->getProcAddress( "wglSwapIntervalEXT" );
    if( wglSwapIntervalEXT ) {
        wglSwapIntervalEXT(onoff ? 1 : 0);
        if (prt)
            Log() << "VSync mode " << (onoff ? "enabled" : "disabled") << " using wglSwapIntervalEXT().";
    } else {
        Warning() << "VSync mode could not be changed because wglSwapIntervalEXT is missing.";
    }
}
#elif defined (Q_WS_MACX)

void setVSyncMode(bool onoff, bool prt)
{
    GLint tmp = onoff ? 1 : 0;
    AGLContext ctx = aglGetCurrentContext();
    if (aglEnable(ctx, AGL_SWAP_INTERVAL) == GL_FALSE)
        Warning() << "VSync mode could not be changed becuse aglEnable AGL_SWAP_INTERVAL returned false!";
    else {
        if ( aglSetInteger(ctx, AGL_SWAP_INTERVAL, &tmp) == GL_FALSE )
            Warning() << "VSync mode could not be changed because aglSetInteger returned false!";
        else if (prt)
            Log() << "VSync mode " << (onoff ? "enabled" : "disabled") << " using aglSetInteger().";
    }
}

#else
#  error Unknown platform, need to implement setVSyncMode()!
#endif

} // end namespace Util
