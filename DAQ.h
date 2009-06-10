#ifndef DAQ_H
#define DAQ_H
#include "LeoDAQGL.h"
#include "SampleBufQ.h"
#include "ChanMap.h"
#include <QMultiMap>
#include <QString>
#include <QThread>
#include <QMutex>
#include <QMutexLocker>
#include <vector>
#include <deque>
#include <QMap>
#include <QStringList>
#ifdef HAVE_NIDAQmx
#include "NI/NIDAQmx.h"
#endif
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

    enum AcqStartEndMode { 
        /* these correspond to items in 'acqStartEndCB' combobox in 
           the Ui::ConfigureDialog form */
        Immediate=0, PDStartEnd, PDStart, Timed, StimGLStartEnd, StimGLStart, 
        N_AcqStartEndModes
    };

    enum TermConfig {
        Default = -1,
        RSE = 10083,
        NRSE = 10078,
        Diff = 10106,
        PseudoDiff = 12529
    };

    TermConfig StringToTermConfig(const QString & txt);
    QString TermConfigToString(TermConfig t);

    const QString & ModeToString(Mode m);
    Mode StringToMode(const QString &);

    const QString & AcqStartEndModeToString(AcqStartEndMode m);

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
        unsigned nExtraChans; ///< the number of extra channels (PD, etc) that aren't part of the demux.. if not in MUX mode, this is always 0
        bool aoPassthru;
        QString aoDev;
        Range aoRange;
        unsigned aoSrate; ///< for now, always the same as srate
        QMap<unsigned, unsigned> aoPassthruMap;
        QVector<unsigned> aoChannels; ///< the AO channels from the above map, plus possibly the photodiode-passthru channel
        QString aoPassthruString;
        /// etc...

        /// index into the acqStartEndCB in Ui::ConfigureDialog
        AcqStartEndMode acqStartEndMode;

        // for acqStartEndMode == timed
        bool isIndefinite, isImmediate;
        double startIn, duration;

        // for threshold crossing of PD chan
        int16 pdThresh;
        bool usePD;
        int pdChan, idxOfPdChan;
        int pdPassThruToAO; ///< if negative, don't, else the channel id
        double pdStopTime; ///< iff PDEnd mode, the amount of time in seconds that need to be elapsed before we pronounce the PD signal as gone and we stop the task

        bool suppressGraphs;

        TermConfig aiTerm;

        unsigned fastSettleTimeMS; ///< defaults to 15ms

        double auxGain;

        ChanMap chanMap;
    };

    //-------- NI DAQmx helper methods -------------

    typedef QMultiMap<QString,Range> DeviceRangeMap;

    /// if empty map returned, no devices with AI!
    DeviceRangeMap ProbeAllAIRanges();
    /// if empty map returned, no devices with AO!
    DeviceRangeMap ProbeAllAORanges();

    typedef QMap<QString, QStringList> DeviceChanMap;
    
    /// returns a list of Devicename->ai channel names for all devices containing AI subdevices in the system
    DeviceChanMap ProbeAllAIChannels();
    /// returns a list of Devicename->ai channel names for all devices containing AO subdevices in the system
    DeviceChanMap ProbeAllAOChannels();

    /// returns the NI channel list of DO chans for a devname, or empty list on failure
    QStringList GetDOChans(const QString & devname);

    /// returns the NI channel list of AI chans for a devname, or empty list on failure
    QStringList GetAIChans(const QString & devname);
    /// returns the NI channel list of AO chans for a devname, or empty list on failure
    QStringList GetAOChans(const QString & devname);

    /// returns the number of physical channels in the AI subdevice for this device, or 0 if AI not supported on this device
    unsigned GetNAIChans(const QString & devname);
    /// returns the number of physical channels in the AO subdevice for this device, or 0 if AO not supported on this device
    unsigned GetNAOChans(const QString & devname);

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

    public slots:
        void requestFastSettle(); ///< task needs to be running and it will then be done synchronously with the task

    signals:
        void bufferOverrun();

        void daqError(const QString &);

        void fastSettleCompleted();

    protected: 
        void run();  ///< reimplemented from QThread
        void overflowWarning(); ///< reimplemented from SampleBufQ

    private:

        volatile bool pleaseStop;
        Params params;
        volatile unsigned fast_settle; ///< if >0, do fast settle
        bool muxMode;

        void daqThr();

        long long totalRead;

        friend struct DAQPvt;

    };
#ifdef HAVE_NIDAQmx
    class AOWriteThread : public QThread, public SampleBufQ {
        Q_OBJECT
    public:
        AOWriteThread(QObject * parent, 
                      TaskHandle & taskHandle,
                      int32 aoBufferSize,
                      const Params & params);
        ~AOWriteThread();
        void stop();
    signals:
        void daqError(const QString &);
    protected:
        void run();
    private:
        volatile bool pleaseStop;
        TaskHandle & taskHandle;
        int32 aoBufferSize;
        const Params & params;
    };
#endif

}
#endif
