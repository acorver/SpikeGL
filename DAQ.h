#ifndef DAQ_H
#define DAQ_H
#include "LeoDAQGL.h"
#include "SampleBufQ.h"
#include <QMultiMap>
#include <QString>
#include <QThread>
#include <QMutex>
#include <QMutexLocker>
#include <vector>
#include <deque>
#ifdef Q_OS_WIN
// Argh!  Windows for some reason has this macro defined..
#undef min
#undef max
#endif

namespace DAQ
{
    struct Range {
        double min, max;
        Range() : min(0.), max(0.) {}
    };

    enum Mode {
        AI60Demux=0, AIRegular, AI120Demux, N_Modes, AIUnknown = N_Modes
    };

    const QString & ModeToString(Mode m);
    Mode StringToMode(const QString &);

    struct Params {
        QString outputFile, dev;
        Range range;
        unsigned doCtlChan;
        QString doCtlChanString;
        Mode mode;
        unsigned srate;
        bool extClock;
        QString aiString;
        QVector<unsigned> aiChannels;
        unsigned nVAIChans; ///< number of virtual (demuxed) AI chans
        bool aoPassthru;
        QMap<unsigned, unsigned> aoPassthruMap;
        QString aoPassthruString;
        /// etc...

        bool suppressGraphs;
    };

    //-------- NI DAQmx helper methods -------------

    typedef QMultiMap<QString,Range> DeviceRangeMap;

    /// if empty map returned, no devices with AI!
    DeviceRangeMap ProbeAllAIRanges();
    /// if empty map returned, no devices with AO!
    DeviceRangeMap ProbeAllAORanges();
    /// returns the NI channel list of DO chans for a devname, or empty list on failure
    QStringList GetDOChans(const QString & devname);

    /// returns true iff the device supports AI simultaneous sampling
    bool     SupportsAISimultaneousSampling(const QString & devname);
    
    double   MaximumSampleRate(const QString & devname, int nChans = 1);
    double   MinimumSampleRate(const QString & devname);

    QString  GetProductName(const QString &devname);
    //-------- END NI DAQmx helper methods -------------


    /** This class represents 1 daq task, running in a separate thread.  
        Data is enqueued and other threads may be alerted via siganl/slot 
        mechanism when there is more data. */
    class Task : public QThread, public SampleBufQ
    {
        Q_OBJECT
    public:
        Task(const Params & acqParams, QObject *parent = 0);
        virtual ~Task();

        void stop(); ///< stops and joins thr

        const QString & devString() const { return params.dev; }
        const QString & doCtlChan() const { return params.doCtlChanString; }
        Mode mode() const { return params.mode; }
        unsigned numChans() const { return params.aiChannels.size()*((params.mode==AI60Demux||params.mode==AI120Demux)?MUX_CHANS_PER_PHYS_CHAN:1); }
        unsigned samplingRate() const { return params.srate; }

        void setDO(bool onoff);

    signals:
        void bufferOverrun();

        void daqError(const QString &);

    protected: 
        void run();  ///< reimplemented from QThread
        void overflowWarning(); ///< reimplemented from SampleBufQ

    private:

        volatile bool pleaseStop;
        Params params;

        void daqThr();

        long long totalRead;

        friend struct DAQPvt;

    };


}
#endif
