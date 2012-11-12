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
#include "samplerate/samplerate.h"

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

void removeTempDataFiles()
{
	QStringList filters;
	filters << (TEMP_FILE_NAME_PREFIX "*" TEMP_FILE_NAME_SUFFIX);
	QStringList tempDataFiles = QDir::temp().entryList(filters);
	// remove *US*
	QFile::remove(QDir::tempPath() + "/" + QString(TEMP_FILE_NAME_PREFIX) + QString::number(QCoreApplication::applicationPid()) + TEMP_FILE_NAME_SUFFIX);
	// next, remove files that are >= 1 day old.  this is so that concurrent instances don't mess with each other
	for (int i = 0; i < tempDataFiles.size(); i++)
	{
		QString fname = QDir::tempPath() + "/" + tempDataFiles.at(i);
		QFileInfo fi (fname);
		if (fi.lastModified().daysTo(QDateTime::currentDateTime()) >= 1)
			QFile::remove(fname);
	}
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


// --- resampler stuff... ---
/* static */ QString Resampler::resample(const std::vector<int16> & input, std::vector<int16> & output, double ratio, int numChannelsPerFrame, Algorithm alg, bool isEndOfInput)
 {
	 if (ratio < 1e-6 || numChannelsPerFrame < 1 || !input.size() || ((int)alg) < 0 || ((int)alg) > 4) 
	 { output.clear(); return "Invalid parameter(s)."; }
	 std::vector<float> inf(input.size()), outf(input.size()*ratio + numChannelsPerFrame + 32);
	 SRC_DATA d;
	 memset(&d, 0, sizeof(d));
	 src_short_to_float_array(&input[0], (d.data_in=&inf[0]), input.size());
	 d.data_out = &outf[0];
	 d.input_frames = input.size()/numChannelsPerFrame;
	 d.output_frames = outf.size()/numChannelsPerFrame;
	 d.end_of_input = isEndOfInput ? 1 : 0;
	 d.src_ratio = ratio;
	 int errcode = src_simple(&d, (int)alg, numChannelsPerFrame);
	 if (errcode) { output.clear(); return src_strerror(errcode); }
	 output.resize(d.output_frames_gen*numChannelsPerFrame);
	 output.reserve(output.size());
	 src_float_to_short_array(&outf[0], &output[0], output.size());
	 return "";
 }

struct Resampler::Impl {
	double ratio;
	int numChannelsPerFrame;
	SRC_STATE *s;
};

Resampler::Resampler(double ratio, int numChannelsPerFrame, Algorithm alg)
{
	int err = 0;
	p = new Impl;
	p->ratio = ratio;
	p->s = src_new((int)alg, numChannelsPerFrame, &err);
	p->numChannelsPerFrame = numChannelsPerFrame;
	if (err) {
		QString error = src_strerror(err);	
		Error() << "INTERNAL ERROR: Resampler::Resampler() got an error from src_new() call!  Error was: " << error; 
		if (p->s) src_delete(p->s), p->s = 0;
	} 
}


Resampler::~Resampler()
{
	if (p->s) src_delete(p->s), p->s = 0;
	delete p, p = 0;
}

bool Resampler::chk() const
{
	if (!p->s) {
		Error() << "INTERNAL ERROR: Resampler instance is invalid!";
		return false;
	}
	return true;
}

void Resampler::changeRatio(double newRatio)
{
	if (!chk()) return;
	src_set_ratio(p->s, p->ratio=newRatio);
}

double Resampler::ratio() const
{
	if (!chk()) return 0.0;
	return p->ratio;
}

QString Resampler::resample(const std::vector<int16> & input, std::vector<int16> & output, bool isEndOfInput)
{
	 if (!chk() || p->ratio < 1e-6 || !input.size() || p->numChannelsPerFrame < 1) { output.clear(); return "Invalid parameter(s)."; }
	 std::vector<float> inf(input.size()), outf(input.size()*p->ratio + p->numChannelsPerFrame + 32);
	 SRC_DATA d;
	 memset(&d, 0, sizeof(d));
	 src_short_to_float_array(&input[0], (d.data_in=&inf[0]), input.size());
	 d.data_out = &outf[0];
	 d.input_frames = input.size()/p->numChannelsPerFrame;
	 d.output_frames = outf.size()/p->numChannelsPerFrame;
	 d.end_of_input = isEndOfInput ? 1 : 0;
	 d.src_ratio = p->ratio;
	 int errcode = src_process(p->s, &d);
	 if (errcode) { output.clear(); return src_strerror(errcode); }
	 output.resize(d.output_frames_gen*p->numChannelsPerFrame);
	 output.reserve(output.size());
	 src_float_to_short_array(&outf[0], &output[0], output.size());
	 return "";
}


} // end namespace Util
