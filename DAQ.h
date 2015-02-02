#ifndef DAQ_H
#define DAQ_H
#include "SpikeGL.h"
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
#include <QVector>
#include <QPair>
#include <QProcess>
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
		bool operator==(const Range &rhs) const { return min == rhs.min && max == rhs.max; }
		bool operator!=(const Range &rhs) const { return !((*this) == rhs); }
    };

    enum Mode {
        AI60Demux=0, AIRegular, AI120Demux, JFRCIntan32, AI128Demux, AI256Demux, AI64Demux, AI96Demux, AI128_32_Demux, N_Modes, AIUnknown = N_Modes
    };

	extern const unsigned ModeNumChansPerIntan[N_Modes];
	extern const unsigned ModeNumIntans[N_Modes];
	
    enum AcqStartEndMode { 
        /* these correspond to items in 'acqStartEndCB' combobox in 
           the Ui::ConfigureDialog form */
        Immediate=0, PDStartEnd, PDStart, Timed, StimGLStartEnd, StimGLStart, 
		AITriggered, /* use physical AI line for triggering */
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
        QString outputFile, outputFileOrig, dev, dev2;
		bool dualDevMode, secondDevIsAuxOnly;
        bool stimGlTrigResave;
        Range range;
        unsigned doCtlChan;
        QString doCtlChanString;
        Mode mode;
        double srate, aoSrate;
        bool extClock;
		QString aoClock;
        QString aiString, aiString2;
        QVector<unsigned> aiChannels, aiChannels2;
        QString subsetString; ///< subset of demuxed AI chans to actually save/graph.
        QBitArray demuxedBitMap; ///< bitmap of the demuxed AI chans to actually save/graph.  Derived from subsetString above.
        unsigned nVAIChans, ///< number of virtual (demuxed) AI chans
		         nVAIChans1, nVAIChans2;  ///< same, but per dev
        unsigned nExtraChans1, nExtraChans2; ///< the number of extra channels (PD, etc) that aren't part of the demux.. if not in MUX mode, this is always 0
        bool aoPassthru;
        QString aoDev;
        Range aoRange;
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
		unsigned pdThreshW; /**< Number of samples that the signal must be past 
						         threshhold thold crossing.  Default 5. */
        bool usePD;
        int pdChan, idxOfPdChan;
		bool pdChanIsVirtual; ///< default false, if true, PD/trigger channel is virtual (demuxed) electrode id, if false, old behavior of physical channel
		bool pdOnSecondDev;
        int pdPassThruToAO; ///< if negative, don't, else the channel id
        double pdStopTime; ///< iff PDEnd mode, the amount of time in seconds that need to be elapsed before we pronounce the PD signal as gone and we stop the task
        double silenceBeforePD; ///< Time, in seconds, of silence before the photodiode signal in PD trigger mode only!  This time window is also applied to the file end as well.

        bool suppressGraphs, lowLatency;

        TermConfig aiTerm;

        unsigned fastSettleTimeMS; ///< defaults to 15ms

        double auxGain;

        ChanMap chanMap;

		bool doPreJuly2011IntanDemux; /**< This affects how we demux the AI channels as they come in
									       off the card -- Pre July 2011 demux lead to scans demuxed as:
									       
									           Intan0_CH0, Intan1_CH0, ... IntanN_ChanM, extrachan1, extrachan2
									   
										   Whereas now, the new scheme is:
									   
									           Intan0_CH0, Intan0_CH1, ... Intan0_ChanM, Intan1_Chan0, ... IntanN_ChanM, extrachan1, extrachan2
									   */
		
		unsigned aiBufferSizeCS; ///< a value from 1 -> 100, which represents the centiseconds (.01 sec or 10 ms) worth of time to make the analog input buffer
		unsigned aoBufferSizeCS; ///< same as above, but for the AO buffer size
		
        int cludgyFilenameCounterOverride; ///< Here it is. Yet more insane complexity and non-obvious behavior to the UI! Hurray!
		
		bool resumeGraphSettings;

		bool autoRetryOnAIOverrun; ///< if true, auto-restart the acquisition every time there is a buffer overrun error from the NI DAQ drivers. Note that if we get more than 2 failures in a 1s period, the acquisition is aborted anyway.
        int overrideGraphsPerTab; ///< if nonzero, the number of graphs per tab to display, 0 implies use mode-specific limits

		struct Bug {
			bool enabled; // if true, acquisition is in bug mode
			int rate; // 0 = Low, 1 = Medium, 2 = High
			int whichTTLs; // bitset of which TTLs to save/graph, TTLs from 1->12 maps to bits #0->11
			int ttlTrig; // the TTL chanel to use for a trigger, or -1 if not using ttl to trigger
			int clockEdge; // 0 = rising, 1 = falling
			int hpf; // if nonzero, the high pass filter is enabled at set to filter past this many Hz
			bool snf; // if true, use the software notch filter at 60Hz
			void reset() { rate = 2; whichTTLs = 0; ttlTrig = -1; clockEdge = 0; hpf = 0; snf = false; enabled = false; }
		} bug;
		
        mutable QMutex mutex;
        void lock() const { mutex.lock(); }
        void unlock() const { mutex.unlock(); }
		
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


    class AOWriteThread;

	class Task : public QThread, public SampleBufQ
	{
		Q_OBJECT
	public:
		Task(QObject *parent, const QString & name, unsigned maxQSize);
		virtual ~Task(); ///< default impl. does nothing.  probably should make it call stop() in a subclass..
        virtual void stop() = 0;
		
        virtual unsigned numChans() const = 0;
        virtual unsigned samplingRate() const = 0;
		        
        u64 lastReadScan() const;
	protected:
		/// reimplemented from QThread, just calls daqThr
        void run() { daqThr(); }  
		virtual void daqThr() = 0; ///< reimplement this!
		
	signals:
        void bufferOverrun();
        void gotFirstScan();
        void daqError(const QString &);
		void daqWarning(const QString &);
		
	protected:
        u64 totalRead;
        mutable QMutex totalReadMut;
	};
	
	
    /** This class represents 1 ni-based daq task, running in a separate thread.  
        Data is enqueued and other threads may be alerted via siganl/slot 
        mechanism when there is more data. */
    class NITask : public Task
    {
        Q_OBJECT
    public:
        NITask(const Params & acqParams, QObject *parent);
		~NITask(); ///< calls stop()

        void stop(); ///< stops and joins thr

        const QString & devString() const { return params.dev; }
        const QString & doCtlChan() const { return params.doCtlChanString; }
        Mode mode() const { return params.mode; }
        unsigned numChans() const { return params.nVAIChans; }
        unsigned samplingRate() const { return params.srate; }

        void setDO(bool onoff);
        
    public slots:
        void requestFastSettle(); ///< task needs to be running and it will then be done synchronously with the task

    signals:
        void fastSettleCompleted();

    protected: 
        void overflowWarning(); ///< reimplemented from SampleBufQ
		void daqThr(); ///< remplemented from DAQ::Task

    private:

        volatile bool pleaseStop;
        const Params & params;
        volatile unsigned fast_settle; ///< if >0, do fast settle
        bool muxMode;

        friend struct DAQPvt;

        static void recomputeAOAITab(QVector<QPair<int, int> > & aoAITab, QString & aoChan, const Params & p);

        static int computeTaskReadFreq(double srate);
		
		static void mergeDualDevData(std::vector<int16> & output, const std::vector<int16> & data, const std::vector<int16> & data2, int NCHANS1, int NCHANS2, int nExtraChans, int nExtraChans2);
		/// only used in Windows / Real (non fake) mode to break up the incoming data into manageable chunks
		void breakupDataIntoChunksAndEnqueue(std::vector<int16> & data, u64 sampCount, bool putFakeDataOnOverrun);

        AOWriteThread *aoWriteThr;
        QVector<QPair<int,int> > aoAITab;
        u64 aoSampCount;

#ifdef HAVE_NIDAQmx
		// used for task create/destroy
		TaskHandle taskHandle, taskHandle2;
#endif
		QString chan, chan2;
		const char *clockSource;
		float64 sampleRate,min,max,timeout;
		u64 bufferSize, bufferSize2;
        // DAQMx error macro related.. for task create/destroy/etc
        int32 error;
        const char *callStr;
        char errBuff[2048];
        QSet<int32> acceptableRetryErrors;
        int nReadRetries;
        double lastEnq;

		bool createAITasks();
		bool startAITasks();
		void destroyAITasks();
#ifdef HAVE_NIDAQmx
        // returns 0 if all ok, -1 if unrecoverable error, 1 if had "buffer overflow error" and tried did acq restart..
        int doAIRead(TaskHandle th, u64 samplesPerChan, std::vector<int16> & data, unsigned long oldS, int32 pointsToRead, int32 & pointsRead);
#endif
        void fudgeDataDueToReadRetry();
		
	};

#ifdef HAVE_NIDAQmx
	struct DAQPvt {
		static int32 everyNSamples_func (TaskHandle taskHandle, int32 everyNsamplesEventType, uint32 nSamples, void *callbackData); 
	};
#endif
		
	class BugTask : public Task {
		Q_OBJECT
	public:
		BugTask(const Params & acqParams, QObject * parent);
		~BugTask(); ///< calls stop
        void stop();
		
        unsigned numChans() const;
        unsigned samplingRate() const;
	protected:
		void daqThr(); ///< Reimplemented from DAQ::Task
	protected slots:
		void slaveProcStateChanged(QProcess::ProcessState);
	private:
		bool setupExeDir(QString * err = 0) const;
		/// returns a strig of the form "c:\temp\bug3_spikegl\"
		static QString exeDir();
		static QString exePath();
		static QString exeName();
		
		void processLine(const QString & lineUntrimmed, QMap<QString, QString> & block, const quint64 & nblocks, int & state, quint64 & nlines);
		void processBlock(const QMap<QString, QString> &);
		
		const Params & params;
		
		volatile bool pleaseStop;		
	};
	
}
#endif
