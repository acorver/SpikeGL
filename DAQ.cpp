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

#define DAQmxErrChk(functionCall) do { if( DAQmxFailed(error=(functionCall)) ) { callStr = STR(functionCall); goto Error_Out; } } while (0)

namespace DAQ 
{
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


    QStringList GetDOChans(const QString & devname) 
    {
#ifdef HAVE_NIDAQmx
        int error;
        const char *callStr = "";
        char errBuff[2048];
        
        char buf[256000] = "";
        DAQmxErrChk(DAQmxGetDevDOLines(devname.toUtf8().constData(), buf, sizeof(buf)));
        return QString(buf).split(QRegExp("\\s*,\\s*"), QString::SkipEmptyParts);
        
    Error_Out:
        if( DAQmxFailed(error) )
            DAQmxGetExtendedErrorInfo(errBuff,2048);
        if( DAQmxFailed(error) ) {
            Error() << "DAQmx Error: " << errBuff;
            Error() << "DAQMxBase Call: " << callStr;
        }
        
        return QStringList();;
        
#else // !HAVE_NIDAQmx, emulated, 1 chan
        return QStringList(QString("%1/port0/line0").arg(devname));
#endif
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
        TaskHandle  taskHandle = 0;
        char        errBuff[2048]={'\0'};
        const char *callStr = "";
        double      startTime;
        Params & p (params);
        // Channel parameters
        QString chan = "";
        QStringList aiStringSplit = p.aiString.split(QRegExp("\\s*,\\s*"), QString::SkipEmptyParts);
        for (QStringList::const_iterator it = aiStringSplit.begin(); it != aiStringSplit.end(); ++it) {
            chan.append(QString("%1 %2/ai%3").arg(chan.length()?",":"").arg(p.dev).arg(*it).trimmed());
        }
        const int nChans = p.aiChannels.size();
        float64     min = p.range.min;
        float64     max = p.range.max;

        // Params dependent on mode and DAQ::Params, etc
        const char *clockSource = p.extClock ? "PFI2" : "OnboardClock"; ///< TODO: make extClock possibly be something other than PFI2
        float64 sampleRate = p.srate;
        const float64     timeout = DAQ_TIMEOUT;
        int NCHANS = p.nVAIChans;
        const bool muxMode =  p.mode == AI60Demux || p.mode == AI120Demux;

        if (muxMode) {
            sampleRate *= double(MUX_CHANS_PER_PHYS_CHAN);
            if (!p.extClock) {
                /// Aieeeee!  Need ext clock for demux mode!
                QString e("Aieeeee!  Need to use an EXTERNAL clock for DEMUX mode!");
                Error() << e;
                emit daqError(e);
                return;
            }
        }

        u64 bufferSize = u64(sampleRate*nChans)/TASK_READ_FREQ_HZ; ///< 1/10th sec per read
        if (bufferSize < NCHANS) bufferSize = NCHANS;
        const u64 dmaBufSize = u64(1000000); /// 1000000 sample DMA buffer per chan?
        const u64 samplesPerChan = bufferSize;
        
        // Timing parameters
        int32       pointsRead;
        const int32 pointsToRead = bufferSize;
        std::vector<int16> data, leftOver;

        DAQmxErrChk (DAQmxCreateTask("",&taskHandle));
        DAQmxErrChk (DAQmxCreateAIVoltageChan(taskHandle,chan.toUtf8().constData(),"",DAQmx_Val_Cfg_Default,min,max,DAQmx_Val_Volts,NULL));
        DAQmxErrChk (DAQmxCfgSampClkTiming(taskHandle,clockSource,sampleRate,DAQmx_Val_Rising,DAQmx_Val_ContSamps,samplesPerChan));
        DAQmxErrChk (DAQmxCfgInputBuffer(taskHandle,dmaBufSize)); //use a 1,000,000 sample DMA buffer per channel
        //DAQmxErrChk (DAQmxRegisterEveryNSamplesEvent (taskHandle, DAQmx_Val_Acquired_Into_Buffer, everyNSamples, 0, DAQPvt::everyNSamples_func, this));

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
        
            DAQmxErrChk (DAQmxReadBinaryI16(taskHandle,pointsToRead/nChans,timeout,DAQmx_Val_GroupByScanNumber,&data[oldS],pointsToRead,&pointsRead,NULL));
            u64 sampCount = totalRead;
            int32 nRead = pointsRead * nChans + oldS;
        
            if ( nRead % NCHANS ) { // not on 60channel boundary, so save extra channels and enqueue them next time around..
                nRead = (nRead / NCHANS) * NCHANS;
                leftOver.insert(leftOver.begin(), data.begin()+nRead, data.end());
                data.erase(data.begin()+nRead, data.end());
            } 
            data.resize(nRead);
            //Debug() << "Acquired " << nRead << " samples. Total " << totalRead;
            totalRead += nRead;
            
            enqueueBuffer(data, sampCount);
            //emit(gotSampleBuffer());
        
        }
        Debug() << "Acquired " << totalRead << " total samples.";

    Error_Out:
        if( DAQmxFailed(error) )
            DAQmxGetExtendedErrorInfo(errBuff,2048);
        if(taskHandle != 0) {
            DAQmxStopTask (taskHandle);
            DAQmxClearTask (taskHandle);
        }
        if( DAQmxFailed(error) ) {
            QString e;
            e.sprintf("DAQmx Error: %s\nDAQMxBase Call: %s",errBuff, callStr);
            Error() << e;
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
