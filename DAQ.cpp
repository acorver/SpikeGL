#include "LeoDAQGL.h"
#include "DAQ.h"
#ifdef HAVE_NIDAQmx
#  include "NI/NIDAQmx.h"
#else
#  define FAKEDAQ
#ifndef Q_OS_WIN
#  warning Not a real NI platform.  All acquisition related functions are emulated!
#endif
#endif

#include <string.h>
#include <QString>
#include <QFile>
#include <QMessageBox>
#include <QApplication>
#include <QRegExp>
#include <QVarLengthArray>

#define DAQmxErrChk(functionCall) do { if( DAQmxFailed(error=(functionCall)) ) { callStr = STR(functionCall); goto Error_Out; } } while (0)

namespace DAQ 
{
    static bool noDaqErrPrint = false;

    /// if empty map returned, no devices with AI!
    DeviceRangeMap ProbeAllAIRanges() 
    {
        DeviceRangeMap ret;
        Range r;
#ifdef HAVE_NIDAQmx
        double myDoubleArray[512];
        for (int devnum = 1; devnum <= 16; ++devnum) {
            memset(myDoubleArray, 0, sizeof(myDoubleArray));
            QString dev( QString("Dev%1").arg(devnum) );
            if (!DAQmxFailed(DAQmxGetDevAIVoltageRngs(dev.toUtf8().constData(), myDoubleArray, 512))) {
                for (int i=0; i<512; i=i+2) {
                    r.min = myDoubleArray[i];
                    r.max = myDoubleArray[i+1];
                    if (r.min == r.max) break;
                    ret.insert(dev, r);
                }
            }
        }
#else // !WINDOWS, emulate
        r.min = -2.5;
        r.max = 2.5;
        ret.insert("Dev1", r);
        r.min = -5.;
        r.max = 5.;
        ret.insert("Dev1", r);
#endif
        return ret;
    }
    /// if empty map returned, no devices with AO!
    DeviceRangeMap ProbeAllAORanges()
    {
        DeviceRangeMap ret;
        Range r;
#ifdef HAVE_NIDAQmx
        double myDoubleArray[512];
        for (int devnum = 1; devnum <= 16; ++devnum) {
            memset(myDoubleArray, 0, sizeof(myDoubleArray));
            QString dev( QString("Dev%1").arg(devnum) );
            if (!DAQmxFailed(DAQmxGetDevAIVoltageRngs(dev.toUtf8().constData(), myDoubleArray, 512))) {
                for (int i=0; i<512; i=i+2) {
                    r.min = myDoubleArray[i];
                    r.max = myDoubleArray[i+1];
                    if (r.min == r.max) break;
                    ret.insert(dev, r);
                }
            }
        }
#else // !WINDOWS, emulate
        r.min = -2.5;
        r.max = 2.5;
        ret.insert("Dev1", r);
        r.min = -5.;
        r.max = 5.;
        ret.insert("Dev1", r);
#endif
        return ret;
    }

    DeviceChanMap ProbeAllAIChannels() {
        bool savedPrt = noDaqErrPrint;
        noDaqErrPrint = true;
        DeviceChanMap ret;
        for (int devnum = 1; devnum <= 16; ++devnum) {
            QString dev( QString("Dev%1").arg(devnum) );
            QStringList l = GetAIChans(dev);
            if (!l.empty()) {
                ret[dev] = l;
            }
        }
        noDaqErrPrint = savedPrt;
        return ret;
    }

    DeviceChanMap ProbeAllAOChannels() {
        bool savedPrt = noDaqErrPrint;
        noDaqErrPrint = true;
        DeviceChanMap ret;
        for (int devnum = 1; devnum <= 16; ++devnum) {
            QString dev( QString("Dev%1").arg(devnum) );
            QStringList l = GetAOChans(dev);
            if (!l.empty()) {
                ret[dev] = l;
            }
        }
        noDaqErrPrint = savedPrt;
        return ret;
    }
    

#if HAVE_NIDAQmx
    typedef int32 (__CFUNC *QueryFunc_t)(const char [], char *, uInt32);
    
    static QStringList GetPhysChans(const QString &devname, QueryFunc_t queryFunc, const QString & fn = "") 
    {
        int error;
        const char *callStr = "";
        char errBuff[2048];        
        char buf[65536] = "";
        QString funcName = fn;

        if (!funcName.length()) {
            funcName = "??";
        }

        DAQmxErrChk(queryFunc(devname.toUtf8().constData(), buf, sizeof(buf)));
        return QString(buf).split(QRegExp("\\s*,\\s*"), QString::SkipEmptyParts);
        
    Error_Out:
        if( DAQmxFailed(error) )
            DAQmxGetExtendedErrorInfo(errBuff,2048);
        if( DAQmxFailed(error) ) {
            if (!noDaqErrPrint) {
                Error() << "DAQmx Error: " << errBuff;
                Error() << "DAQMxBase Call: " << funcName << "(" << devname << ",buf," << sizeof(buf) << ")";
            }
        }
        
        return QStringList();         
    }
#endif

    QStringList GetDOChans(const QString & devname) 
    {
#ifdef HAVE_NIDAQmx
        return GetPhysChans(devname, DAQmxGetDevDOLines, "DAQmxGetDevDOLines");
#else // !HAVE_NIDAQmx, emulated, 1 chan
        return QStringList(QString("%1/port0/line0").arg(devname));
#endif
    }

    QStringList GetAIChans(const QString & devname)
    {
#ifdef HAVE_NIDAQmx
        return GetPhysChans(devname, DAQmxGetDevAIPhysicalChans, "DAQmxGetDevAIPhysicalChans");
#else // !HAVE_NIDAQmx, emulated, 60 chans
        QStringList ret;
        for (int i = 0; i < 60; ++i) {
            ret.push_back(QString("%1/ai%2").arg(devname).arg(i));
        }
        return ret;
#endif
    }

    QStringList GetAOChans(const QString & devname)
    {
#ifdef HAVE_NIDAQmx
        return GetPhysChans(devname, DAQmxGetDevAOPhysicalChans, "DAQmxGetDevAOPhysicalChans");
#else // !HAVE_NIDAQmx, emulated, 2 chans
        QStringList ret;
        for (int i = 0; i < 2; ++i) {
            ret.push_back(QString("%1/ao%2").arg(devname).arg(i));
        }
        return ret;
#endif
    }

    /// returns the number of physical channels in the AI subdevice for this device, or 0 if AI not supported on this device
    unsigned GetNAIChans(const QString & devname)
    {
        return GetAIChans(devname).count();
    }
    /// returns the number of physical channels in the AO subdevice for this device, or 0 if AO not supported on this device
    unsigned GetNAOChans(const QString & devname)
    {
        return GetAOChans(devname).count();
    }


    /// returns true iff the device supports AI simultaneous sampling
    bool     SupportsAISimultaneousSampling(const QString & devname)
    {
#ifdef HAVE_NIDAQmx
        bool32 ret = false;
        if (DAQmxFailed(DAQmxGetDevAISimultaneousSamplingSupported(devname.toUtf8().constData(), &ret))) {
            Error() << "Failed to query whether dev " << devname << " AI supports simultaneous sampling.";
        }
        return ret;
#else // !HAVE_NIDAQmx, emulated
        (void)devname;
        return true;
#endif
    }

    double   MaximumSampleRate(const QString & dev, int nChans) 
    {
        double ret = 1e6;
        (void)dev; (void)nChans;
#ifdef HAVE_NIDAQmx
        float64 val;
        int32 e;
        if (nChans <= 0) nChans = 1;
        if (nChans == 1)
            e = DAQmxGetDevAIMaxSingleChanRate(dev.toUtf8().constData(), &val);
        else
            e = DAQmxGetDevAIMaxMultiChanRate(dev.toUtf8().constData(), &val);
        if (DAQmxFailed(e)) {
            Error() << "Failed to query maximum sample rate for dev " << dev << ".";
        } else {
            ret = val;
            if (nChans > 1 && !SupportsAISimultaneousSampling(dev)) {
                ret = ret / nChans;
            }
        }
#endif
        return ret;        
    }

    double   MinimumSampleRate(const QString & dev)
    {
        double ret = 10.;
        (void)dev;
#ifdef HAVE_NIDAQmx
        float64 val;
        if (DAQmxFailed(DAQmxGetDevAIMinRate(dev.toUtf8().constData(), &val))) {
            Error() << "Failed to query minimum sample rate for dev " << dev << ".";
            
        } else {
            ret = val;
        }
#endif
        return ret;
    }

    QString  GetProductName(const QString &dev)
    {
#ifdef HAVE_NIDAQmx
        char buf[65536] = "Unknown";
        if (DAQmxFailed(DAQmxGetDevProductType(dev.toUtf8().constData(), buf, sizeof(buf)))) {
            Error() << "Failed to query product name for dev " << dev << ".";
        } 
        // else..
        return buf;
#else
        (void)dev;
        return "FakeDAQ";
#endif
    }


#define DEFAULT_DEV "Dev1"
#define DEFAULT_DO 0

#ifndef FAKEDAQ

    struct DAQPvt {
        static int32 everyNSamples_func (TaskHandle taskHandle, int32 everyNsamplesEventType, uint32 nSamples, void *callbackData); 
    };

#endif

    Task::Task(const Params & acqParams, QObject *p) 
        : QThread(p), pleaseStop(false), params(acqParams)
    {
        totalRead = 0;
    }

    Task::~Task()
    {
        stop();
    }


    void Task::stop() 
    {
        if (isRunning() && !pleaseStop) {
            pleaseStop = true;
            wait();
            pleaseStop = false;
        }
    }

    void Task::run()
    {
        daqThr();
    }

    void Task::overflowWarning() 
    {
        Warning() << "DAQ Task sample buffer overflow!  Queue has " << dataQueueMaxSize << " buffers in it!  Dropping a buffer!";
        emit(bufferOverrun());
#ifdef FAKEDAQ
        static int overflowct = 0;
    
        if (++overflowct == 5) {
            emit(daqError("Overflow limit exceeded"));
        }
#endif
    }

#ifdef FAKEDAQ
}// end namespace DAQ

#include <stdlib.h>

namespace DAQ 
{
    void Task::daqThr()
    {
        static QString fname("fakedaqdata.bin");
        char *e;
        if ((e=getenv("FAKEDAQ"))) {
            fname = e;
        } else {
            Warning() << "FAKEDAQ env var not found, using " << fname << " as filename instead";
        }
        QFile f(fname);
        if (!f.open(QIODevice::ReadOnly)) {
            QString err = QString("Could not open %1!").arg(fname);
            Error() << err;
            emit daqError(err);        
            return;
        }
        std::vector<int16> data;
        const double onePd = int(params.srate/double(TASK_READ_FREQ_HZ));
        while (!pleaseStop) {
            data.resize(unsigned(params.nVAIChans*onePd));
            qint64 nread = f.read((char *)&data[0], data.size()*sizeof(int16));
            if (nread != data.size()*sizeof(int16)) {
                f.seek(0);
            } else if (nread > 0) {
                nread /= sizeof(int16);
                data.resize(nread);
                enqueueBuffer(data, totalRead);
      
                totalRead += nread;
            }
            usleep(int((1e6/params.srate)*onePd));
        }
    }

    void Task::setDO(bool onoff)
    {
        Warning() << "setDO(" << (onoff ? "on" : "off") << ") called (unimplemented in FAKEDAQ mode)";
    }


#else // !FAKEDAQ

    void Task::daqThr()
    {
        // Task parameters
        int32       error = 0;
        TaskHandle  taskHandle = 0, aoTaskHandle = 0;
        char        errBuff[2048]={'\0'};
        const char *callStr = "";
        double      startTime;
        const Params & p (params);
        // Channel parameters
        QString chan = "", aoChan = "";
      

        {
            const QVector<QString> aiChanStrings ((ProbeAllAIChannels()[p.dev]).toVector());
            //build chanspec string for aiChanStrings..
            for (QVector<unsigned>::const_iterator it = p.aiChannels.begin();
                 it != p.aiChannels.end(); ++it) {
                chan.append(QString("%1%2").arg(chan.length() ? ", " : "").arg(aiChanStrings[*it]));
            }
            
            if (p.aoPassthru) {
                const QVector<QString> aoChanStrings ((ProbeAllAOChannels()[p.aoDev]).toVector());
                for (QVector<unsigned>::const_iterator it = p.aoChannels.begin();
                     it != p.aoChannels.end(); 
                     ++it) {
                    aoChan.append(QString("%1%2").arg(aoChan.length() ? ", " : "").arg(aoChanStrings[*it]));
                }
            }
        }
        
        const int nChans = p.aiChannels.size();
        const float64     min = p.range.min;
        const float64     max = p.range.max;
        const int aoNChans = p.aoChannels.size();
        const float64     aoMin = p.aoRange.min;
        const float64     aoMax = p.aoRange.max;
        

        // Params dependent on mode and DAQ::Params, etc
        const char *clockSource = p.extClock ? "PFI2" : "OnboardClock"; ///< TODO: make extClock possibly be something other than PFI2
        const char *aoClockSource = "OnboardClock";
        float64 sampleRate = p.srate;
        const float64 aoSampleRate = p.aoSrate;
        const float64     timeout = DAQ_TIMEOUT;
        const float64     aoTimeout = DAQ_TIMEOUT*2.;
        const int NCHANS = p.nVAIChans;
        const bool muxMode =  p.mode == AI60Demux || p.mode == AI120Demux;
        int nscans_per_mux_scan = 1;
        unsigned aoWriteCt = 0;

        if (muxMode) {
            sampleRate *= double(MUX_CHANS_PER_PHYS_CHAN);
            nscans_per_mux_scan = MUX_CHANS_PER_PHYS_CHAN;
            if (!p.extClock) {
                /// Aieeeee!  Need ext clock for demux mode!
                QString e("Aieeeee!  Need to use an EXTERNAL clock for DEMUX mode!");
                Error() << e;
                emit daqError(e);
                return;
            }
        }

        QVarLengthArray<int> 
            aoAITab(aoNChans ? aoNChans : 1); ///< map of AO chan id to virtual AI chan id
        for (int i = 0; i < aoNChans; ++i) {
            aoAITab[i] = p.aoPassthruMap[i];
        }

        u64 bufferSize = u64(sampleRate*nChans)/TASK_READ_FREQ_HZ; ///< 1/10th sec per read
        if (bufferSize < NCHANS) bufferSize = NCHANS;
        const u64 dmaBufSize = u64(1000000); /// 1000000 sample DMA buffer per chan?
        const u64 samplesPerChan = bufferSize/nChans;
        const int32 aoSamplesPerChan = samplesPerChan*2;
        const u64 aoBufferSize = u64(aoSamplesPerChan) * aoNChans;

        // Timing parameters
        int32       pointsRead;
        const int32 pointsToRead = bufferSize;
        std::vector<int16> data, leftOver, aoData;

        DAQmxErrChk (DAQmxCreateTask("",&taskHandle));
        DAQmxErrChk (DAQmxCreateAIVoltageChan(taskHandle,chan.toUtf8().constData(),"",DAQmx_Val_Cfg_Default,min,max,DAQmx_Val_Volts,NULL));
        DAQmxErrChk (DAQmxCfgSampClkTiming(taskHandle,clockSource,sampleRate,DAQmx_Val_Rising,DAQmx_Val_ContSamps,bufferSize));
        DAQmxErrChk (DAQmxCfgInputBuffer(taskHandle,dmaBufSize)); //use a 1,000,000 sample DMA buffer per channel
        //DAQmxErrChk (DAQmxRegisterEveryNSamplesEvent (taskHandle, DAQmx_Val_Acquired_Into_Buffer, everyNSamples, 0, DAQPvt::everyNSamples_func, this));

        if (p.aoPassthru && p.aoChannels.size()) {
            DAQmxErrChk (DAQmxCreateTask("",&aoTaskHandle));
            DAQmxErrChk (DAQmxCreateAOVoltageChan(aoTaskHandle,aoChan.toUtf8().constData(),"",aoMin,aoMax,DAQmx_Val_Volts,NULL));
            DAQmxErrChk (DAQmxCfgSampClkTiming(aoTaskHandle,aoClockSource,aoSampleRate,DAQmx_Val_Rising,DAQmx_Val_ContSamps,aoBufferSize));
        }


        if (muxMode)
            setDO(false); // set DO line low to reset external MUX

        DAQmxErrChk (DAQmxStartTask(taskHandle));

        if (muxMode)
            setDO(true); // now set DO line high to start external MUX and clock on PFI2

        startTime = getTime();
        while( !pleaseStop ) {
            data.clear(); // should already be cleared, but enforce...
            if (leftOver.size()) data.swap(leftOver);            
            unsigned long oldS = data.size();
            data.reserve(pointsToRead+oldS);
            data.resize(pointsToRead+oldS);
        
            DAQmxErrChk (DAQmxReadBinaryI16(taskHandle,samplesPerChan,timeout,DAQmx_Val_GroupByScanNumber,&data[oldS],pointsToRead,&pointsRead,NULL));
            u64 sampCount = totalRead;
            int32 nRead = pointsRead * nChans + oldS;                  
            const int nCH = nChans*nscans_per_mux_scan;
            if ( nRead % nCH ) { // not on 60 (or 75 if have interwoven PD and in PD mode) channel boundary, so save extra channels and enqueue them next time around..
                nRead = (nRead / nCH) *  nCH;
                leftOver.insert(leftOver.begin(), data.begin()+nRead, data.end());
                data.erase(data.begin()+nRead, data.end());
            }
            // at this point we have scans of size 60 (or 75) channels
            // in the 75-channel case we need to throw away 14 of those channels since they are interwoven PD-channels!
            data.resize(nRead);
            if (!nRead) {
                Warning() << "Read less than a full scan from DAQ hardware!  FIXME on line:" << __LINE__ << " in " << __FILE__ << "!";
                continue; 
            }

            //Debug() << "Acquired " << nRead << " samples. Total " << totalRead;
            if (p.usePD && muxMode) {
            /*
              NB: at this point data contains scans of the form:

              | 0 | 1 | 2 | 3 | PD | 4 | 5 | 6 | 7 | PD |... | 58 | 59 | PD |
              ---------------------------------------------------------------

              Notice how the PD channel is interwoven into the
              0:3 (or 0:7 if in 120 channel mode) AI channels.

              We need to remove it!

              We want to turn that into 1 (or more) demuxed VIRTUAL scans
              of the form:

              | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |... | 58 | 59 | PD |
              -----------------------------------------------------

              Where we remove every `nChans' PD except for when we have 
              NCHANS-1 samples, then we add the PD channel.

              Capisce?

              NB: nChans = Number of physical channels per physical scan
                  NCHANS = Number of virtual channels per virtual scan

            */
                std::vector<int16> tmp;
                const int datasz = data.size();
                tmp.reserve(datasz);
               
                for (int i = nChans-1; i < datasz; i += nChans) {
                    tmp.insert(tmp.end(), &data[(i-nChans)+1], &data[i]); // copy non-PD channels for this MUXed sub-scan
                    if (!(tmp.size() % (NCHANS-1))) // if we are on a virtual scan-boundary, then ...
                        tmp.push_back(data[i]); // .. we keep the PD channel
                    else
                        --sampCount, --nRead;  // otherwise we discarded it, so tally its disappearance
                }
                data.swap(tmp);
            }
            totalRead += nRead;
            
            // now, do optional AO output TODO: see about doing this in another thread to save on latency here?
            if (aoTaskHandle) {               
                double t0 = getTime(); // XXX HACK FIXME DEBUG TODO
                const int dsize = data.size();
                aoData.reserve(MAX(aoBufferSize*2,dsize));
                for (int i = 0; i < dsize; i += NCHANS) {
                    for (int aoch = 0; aoch < aoNChans; ++aoch) {
                        aoData.push_back(data[i+aoAITab[aoch]]);
                    }
                }
                if (aoData.size() >= aoBufferSize) { 
                    const int32 nScansToWrite = aoSamplesPerChan;
                    int32 nScansWritten = 0;
                    DAQmxErrChk(DAQmxWriteBinaryI16(aoTaskHandle, nScansToWrite, 1, aoTimeout, DAQmx_Val_GroupByScanNumber, &aoData[0], &nScansWritten, NULL));
                    if (nScansWritten != nScansToWrite) {
                        Error() << "nScansWritten (" << nScansWritten << ") != nScansToWrite (" << nScansToWrite << ")";
                    }                    
                    ++aoWriteCt;
                    aoData.erase(aoData.begin(), aoData.begin()+nScansWritten);
                    Debug() << "AOWrite " << aoWriteCt << " of " << nScansWritten << " (bufsize=" << aoBufferSize << ") took: " << (getTime()-t0) << " secs";
                }

            }
            
            enqueueBuffer(data, sampCount);

        
        }
        Debug() << "Acquired " << totalRead << " total samples.";

    Error_Out:
        if ( DAQmxFailed(error) )
            DAQmxGetExtendedErrorInfo(errBuff,2048);
        if ( taskHandle != 0) {
            DAQmxStopTask (taskHandle);
            DAQmxClearTask (taskHandle);
        }
        if ( aoTaskHandle != 0) {
            DAQmxStopTask (aoTaskHandle);
            DAQmxClearTask (aoTaskHandle);
        }
        if( DAQmxFailed(error) ) {
            QString e;
            e.sprintf("DAQmx Error: %s\nDAQMxBase Call: %s",errBuff, callStr);
            if (!noDaqErrPrint) {
                Error() << e;
            }
            emit daqError(e);
        }
        
    }


    void Task::setDO(bool onoff)
    {
        const char *callStr = "";

        // Task parameters
        int      error = 0;
        TaskHandle  taskHandle = 0;
        char        errBuff[2048];
        QString tmp;

        // Channel parameters
        const QString & chan (doCtlChan());
        
        // Write parameters
        uint32      w_data [1];

        // Create Digital Output (DO) Task and Channel
        DAQmxErrChk (DAQmxCreateTask ("", &taskHandle));
        DAQmxErrChk (DAQmxCreateDOChan(taskHandle,chan.toUtf8().constData(),"",DAQmx_Val_ChanPerLine));

        // Start Task (configure port)
        //DAQmxErrChk (DAQmxStartTask (taskHandle));

        //  Only 1 sample per channel supported for static DIO
        //  Autostart ON

        if (!onoff) 
            w_data[0] = 0x0;
        else 
            w_data[0] = 0x1;

        tmp.sprintf("Writing to DO: %s data: 0x%X", chan.toUtf8().constData(),(unsigned int)w_data[0]);
        Debug() << tmp;

        DAQmxErrChk (DAQmxWriteDigitalScalarU32(taskHandle,1,DAQ_TIMEOUT,w_data[0],NULL));


    Error_Out:

        if (DAQmxFailed (error))
            DAQmxGetExtendedErrorInfo (errBuff, 2048);

        if (taskHandle != 0)
            {
                DAQmxStopTask (taskHandle);
                DAQmxClearTask (taskHandle);
            }

        if (error) {
            QString e;
            e.sprintf("DAQmx Error %d: %s", error, errBuff);
            if (!noDaqErrPrint) 
                Error() << e;
            emit daqError(e);
        }
    }

    int32 DAQPvt::everyNSamples_func (TaskHandle taskHandle, int32 everyNsamplesEventType, uint32 nSamples, void *callbackData)
    {
        Task *daq = (Task *)callbackData;
        (void)daq;
        // todo: read data here
        (void)taskHandle; (void)everyNsamplesEventType; (void) nSamples;
        return 0;
    }

#endif


    /// some helper funcs from LeoDAQGL.h
    static const QString acqModes[] = { "AI60Demux", "AIRegular", "AI120Demux", QString::null };
    
    const QString & ModeToString(Mode m)
    {
        return acqModes[(int)m];
    }
    
    Mode StringToMode(const QString &str)
    {
        int i;
        for (i = 0; i < (int)AIUnknown; ++i) {
            if (str.trimmed().compare(acqModes[i], Qt::CaseInsensitive) == 0) {
                break;
            }
        }
        return (Mode)i;
    }


} // end namespace DAQ
