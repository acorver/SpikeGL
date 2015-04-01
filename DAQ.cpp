#include "SpikeGL.h"
#include "DAQ.h"
#ifdef HAVE_NIDAQmx
#  include "NI/NIDAQmx.h"
#  include "AOWriteThread.h"
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
#include <QThread>
#include <QPair>
#include <QSet>
#include <QMutexLocker>
#include <QProcess>
#include <QProcessEnvironment>
#include <math.h>
#include "SampleBufQ.h"
#include "MainApp.h"
#include "FrameGrabber/FG_SpikeGL/FG_SpikeGL/XtCmd.h"

#define DAQmxErrChk(functionCall) do { if( DAQmxFailed(error=(functionCall)) ) { callStr = STR(functionCall); goto Error_Out; } } while (0)
#define DAQmxErrChkNoJump(functionCall) ( DAQmxFailed(error=(functionCall)) && (callStr = STR(functionCall)) )

bool excessiveDebug = false; /* from SpikeGL.h */

namespace DAQ 
{
    static bool noDaqErrPrint = false;

    const unsigned ModeNumChansPerIntan[N_Modes] = {
        15, 0, 15, 16, 16, 32, 16, 16, 32
    };
    
    const unsigned ModeNumIntans[N_Modes] = {
        4, 0, 8, 2, 8, 8, 4, 6, 4
    };
    
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
        r.min = -2.5;
        r.max = 2.5;
        ret.insert("Dev2", r);
        r.min = -5.;
        r.max = 5.;
        ret.insert("Dev2", r);
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
            if (!DAQmxFailed(DAQmxGetDevAOVoltageRngs(dev.toUtf8().constData(), myDoubleArray, 512))) {
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
        r.min = -2.5;
        r.max = 2.5;
        ret.insert("Dev2", r);
        r.min = -5.;
        r.max = 5.;
        ret.insert("Dev2", r);
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
        if (devname == "Dev1") {
            for (int i = 0; i < 60; ++i) {
                ret.push_back(QString("%1/ai%2").arg(devname).arg(i));
            }
        }
        if (devname == "Dev2") { // new "massive-channel" dev 2, for testing..
            for (int i = 0; i < 4096; ++i) {
                ret.push_back(QString("%1/ai%2").arg(devname).arg(i));
            }
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
        if (devname == "Dev1" || devname == "Dev2") {
            for (int i = 0; i < 2; ++i) {
                ret.push_back(QString("%1/ao%2").arg(devname).arg(i));
            }
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
		if (dev.compare("USB_Bug3", Qt::CaseInsensitive)==0) return "Intan USB Telemetry Receiver (Bug3)";
		if (dev.compare("Framegrabber", Qt::CaseInsensitive)==0) return "XtiumCL PX4 + Camerialink to Intan";
#ifdef HAVE_NIDAQmx
        char buf[65536] = "Unknown";
        if (DAQmxFailed(DAQmxGetDevProductType(dev.toUtf8().constData(), buf, sizeof(buf)))) {
            Error() << "Failed to query product name for dev " << dev << ".";
        } 
        // else..
        return buf;
#else
        (void)dev;
        return dev == "Dev2" ? "FakeDAQMassiveTest" : "FakeDAQ";
#endif
    }
    
    NITask::NITask(const Params & acqParams, QObject *p) 
        : Task(p,"DAQ Task", SAMPLE_BUF_Q_SIZE), pleaseStop(false), params(acqParams), 
          fast_settle(0), muxMode(false), aoWriteThr(0),
#ifdef HAVE_NIDAQmx
          taskHandle(0), taskHandle2(0),
#endif
          clockSource(0), error(0), callStr(0)
    {
        errBuff[0] = 0;
        setDO(false); // assert DO is low when stopped...
    }
	
	NITask::~NITask() { stop(); }


    void NITask::stop() 
    {
        if (isRunning() && !pleaseStop) {
            pleaseStop = true;
            wait();
            pleaseStop = false;
        }
        setDO(false); // assert DO low when stopped...
    }

    void NITask::overflowWarning() 
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

    /* static */
    int NITask::computeTaskReadFreq(double srate_in) {
        int srate = ceil(srate_in); (void)srate;
        return DEF_TASK_READ_FREQ_HZ;
        /*
        // try and figure out how often to run the task read function.  defaults to 10Hz
        // but if that isn't groovy with the sample rate, try a freq that is more groovy
        int task_freq = DEF_TASK_READ_FREQ_HZ;
        while (task_freq >= 3 && (srate % task_freq)) 
            --task_freq;
        if (task_freq < 3) {
            for (task_freq = DEF_TASK_READ_FREQ_HZ; (srate % task_freq) && task_freq <= DEF_TASK_READ_FREQ_HZ*2; 
                 ++task_freq)
                ;
            if (srate % task_freq)
                task_freq = 1;// give up and use 1Hz!
        }    
        Debug() << "Using task read freq: " << task_freq << "Hz.";        
        return task_freq;
         */
    }
    
#ifdef FAKEDAQ
}// end namespace DAQ

#include <stdlib.h>

namespace DAQ 
{
    void NITask::daqThr()
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
        const double onePd = int(params.srate/double(computeTaskReadFreq(params.srate)));
        while (!pleaseStop) {
			double ts = getTime();
            data.resize(unsigned(params.nVAIChans*onePd));
            qint64 nread = f.read((char *)&data[0], data.size()*sizeof(int16));
            if (nread != data.size()*sizeof(int16)) {
                f.seek(0);
            } else if (nread > 0) {
                nread /= sizeof(int16);
                data.resize(nread);
                if (!totalRead) emit(gotFirstScan());
                enqueueBuffer(data, totalRead);
      
                totalReadMut.lock();
                totalRead += nread;
                totalReadMut.unlock();
            }
			int sleeptime = int((1e6/params.srate)*onePd) - (getTime()-ts)*1e6;
			if (sleeptime > 0) 
				usleep(sleeptime);
			else
				Warning() << "FakeDAQ NITask::daqThr(). sleeptime (" << sleeptime << ") is less than 0!";
        }
    }

    void NITask::setDO(bool onoff)
    {
        Warning() << "setDO(" << (onoff ? "on" : "off") << ") called (unimplemented in FAKEDAQ mode)";
    }


    void NITask::requestFastSettle() 
    {
        Warning() << "requestFastSettle() unimplemented for FAKEDAQ mode!";
        emit(fastSettleCompleted());
    }

#else // !FAKEDAQ

    void AOWriteThread::run()
    {
        if (old2Delete) {
            delete old2Delete;
            old2Delete = 0;
        }
        const Params & p(params);
        TaskHandle taskHandle(0);
        int32       error = 0;
        char        errBuff[2048]={'\0'};
        const char *callStr = "";
        unsigned bufferSizeCS = p.aoBufferSizeCS;
        if (p.aoBufferSizeCS < p.aiBufferSizeCS) {
            Warning() << "AOWrite thread AO buf=" << (p.aoBufferSizeCS*10.) << " is less than AI buf=" << (p.aiBufferSizeCS*10.) << ", this is unsupported.  Forcing AO buffer size to " << (p.aiBufferSizeCS*10.);
            bufferSizeCS = p.aiBufferSizeCS;
        }
        int32 aoSamplesPerChan( ceil(ceil(p.aoSrate) * ((bufferSizeCS/100.0)+0.005/*<--ugly fudge to make sure AO buffer size is always 5ms bigger than AI! Argh!*/)) );
        while (aoSamplesPerChan % 2)
            ++aoSamplesPerChan; // force aoSamplesPerChan to be multiple of 2? I forget why!!!!
        const int32 aoChansSize = p.aoChannels.size();
        unsigned aoBufferSize(aoSamplesPerChan * aoChansSize);
        const float64     aoMin = p.aoRange.min;
        const float64     aoMax = p.aoRange.max;
        const float64     aoTimeout = DAQ_TIMEOUT*2.;
        unsigned aoWriteCt = 0;
        pleaseStop = false;
        //std::vector<int16> leftOver;
        // XXX diags..
        int nWritesAvg = 0, nWritesAvgMax = 10;
        double writeSpeedAvg = 0.;
        double tLastData = -1.0, inpDataRate=0., outDataRate=0.;
        std::vector<int16> sampsOrig, samps;
        Util::Resampler resampler(double(p.aoSrate) / double(p.srate) /* HACK FIXME * 2.0*/, aoChansSize);
        int nodatact = 0;
        int daqerrct = 0;

        while (daqerrct < 3 && !pleaseStop) {
            bool break2 = false;
            DAQmxErrChk (DAQmxCreateTask("",&taskHandle));
            DAQmxErrChk (DAQmxCreateAOVoltageChan(taskHandle,aoChanString.toUtf8().constData(),"",aoMin,aoMax,DAQmx_Val_Volts,NULL));
            DAQmxErrChk (DAQmxCfgSampClkTiming(taskHandle,p.aoClock.toUtf8().constData(),p.aoSrate,DAQmx_Val_Rising,DAQmx_Val_ContSamps,/*aoBufferSize*/aoSamplesPerChan/*0*/));
            DAQmxErrChk (DAQmxCfgOutputBuffer(taskHandle,aoSamplesPerChan));
            DAQmxSetWriteRegenMode(taskHandle, DAQmx_Val_DoNotAllowRegen);
			DAQmxSetImplicitUnderflowBehavior(taskHandle, DAQmx_Val_PauseUntilDataAvailable);

            Debug() << "AOWrite thread started.";

            while (!pleaseStop && !break2) {
                u64 sampCount;
                if (!dequeueBuffer(sampsOrig, sampCount) || !sampsOrig.size()) {
                    if (++nodatact > 10) {
                        nodatact = 0;
						if (aoWriteCt) Warning() << "AOWrite thread failed to dequeue any sample data 10 times in a row. Clocks drifting?";
                        // failed to dequeue data, insert silence to keep ao writer going otherwise we get underruns!
 /*                       sampsOrig.clear();
                        samps.clear();
                        samps.resize(p.aoSrate * 0.002 * aoChansSize, lastSamp); // insert 2ms of silence?
*/
                    } else {
                        msleep(2);
                        continue;
                    }
                } else { // normal operation..
                    double now = getTime();
					nodatact = 0;
                    QString err = resampler.resample(sampsOrig, samps);
					if (tLastData > 0.0) { 
                        inpDataRate = (double(sampsOrig.size())/aoChansSize)/(now-tLastData);
						outDataRate = (double(samps.size())/aoChansSize)/(now-tLastData);
					}
                    tLastData = now;
                    if (err.length()) {
                        Error() << "AOWrite thread resampler error: " << err;
                        continue;
                    }
                }
                //if (leftOver.size()) samps.insert(samps.begin(), leftOver.begin(), leftOver.end());
                //leftOver.clear();
                unsigned sampsSize = samps.size(), rem = sampsSize % aoChansSize;
                if (rem) {
                    Warning() << "AOWrite thread got scan data that has invalid size.. throwing away last " << rem << " chans (writect " << aoWriteCt << ")";
                    //sampsSize -= rem;
                    //leftOver.reserve(rem);
                    //leftOver.insert(leftOver.begin(), samps.begin()+sampsSize, samps.end());
                }
                unsigned sampIdx = 0;
                while (sampIdx < sampsSize && !pleaseStop) {
                    int32 nScansToWrite = (sampsSize-sampIdx)/aoChansSize;
                    int32 nScansWritten = 0;                

					if (nScansToWrite > aoSamplesPerChan) {
						Warning() << "AOWrite thread got " << nScansToWrite << " input scans but AO Buffer size is only " << aoSamplesPerChan << " scans.. increase AO buffer size in UI!";
					}
                    /*if (nScansToWrite > aoSamplesPerChan)
                    nScansToWrite = aoSamplesPerChan;
                    else if (!aoWriteCt && nScansToWrite < aoSamplesPerChan) {                    leftOver.insert(leftOver.end(), samps.begin()+sampIdx, samps.end());
                    break;
                    }*/

                    double t0 = getTime();
                    if ( DAQmxErrChkNoJump(DAQmxWriteBinaryI16(taskHandle, nScansToWrite, 1, aoTimeout, DAQmx_Val_GroupByScanNumber, &samps[sampIdx], &nScansWritten, NULL)) )
                    {
                        break2 = true;
                        if (++daqerrct < 3) {
                            DAQmxGetExtendedErrorInfo(errBuff,2048);
                            Debug() << "AOWrite thread error on write: \"" << errBuff << "\". AOWrite retrying since errct < 3.";
                            DAQmxStopTask(taskHandle);
                            DAQmxClearTask(taskHandle);
                            taskHandle = 0;
                        } else {
                            Debug() << "AOWrite error on write and not retrying since errct >= 3!";
                        }
                        break;
                    } else if (daqerrct > 0) --daqerrct; 

                    // test code to test AI->AO delay in software, in a very rough way.. remove if you want.
                    if (excessiveDebug) {
                        u64 scanCt = (sampsOrig.size() + sampCount)/aoChansSize,
                        gScanCt = mainApp()->currentDAQScan();
                        Debug() << "AO got scan:" << scanCt << " (global scan: " << gScanCt << "). delay=~" << int32( (gScanCt-scanCt) / double(p.srate) * 1000.) << "ms";
                    }

                    const double tWrite(getTime()-t0);

                    if (nScansWritten >= 0 && nScansWritten < nScansToWrite) {
                        rem = (nScansToWrite - nScansWritten) * aoChansSize;
                        Debug() << "Partial write: nScansWritten=" << nScansWritten << " nScansToWrite=" << nScansToWrite << ", queueing for next write..";
                        //leftOver.insert(leftOver.end(), samps.begin()+sampIdx+nScansWritten, samps.end());
                        break;
                    } else if (nScansWritten != nScansToWrite) {
                        Error() << "nScansWritten (" << nScansWritten << ") != nScansToWrite (" << nScansToWrite << ")";
                        break;
                    }

                    if (++nWritesAvg > nWritesAvgMax) nWritesAvg = nWritesAvgMax;
                    writeSpeedAvg -= writeSpeedAvg/nWritesAvg;
                    double spd;
                    writeSpeedAvg += (spd = double(nScansWritten)/tWrite ) / double(nWritesAvg);

                    if (tWrite > 0.250) {
                        Debug() << "AOWrite #" << aoWriteCt << " " << nScansWritten << " scans " << aoChansSize << " chans" << " (bufsize=" << aoBufferSize << ") took: " << tWrite << " secs";
                    }
                    if (excessiveDebug) Debug() << "AOWrite speed " << spd << " Hz avg speed " << writeSpeedAvg << " Hz" << " (" << nScansWritten << " scans) buffer fill " << ((dataQueueSize()/double(dataQueueMaxSize))*100.0) << "% inprate->outrate:" << inpDataRate << "->" << outDataRate << "(?) Hz";

                    ++aoWriteCt;
                    sampIdx += nScansWritten*aoChansSize;
                }
            }
        }
    Error_Out:
        if ( DAQmxFailed(error) )
            DAQmxGetExtendedErrorInfo(errBuff,2048);
        if ( taskHandle != 0) {
            DAQmxStopTask(taskHandle);
            DAQmxClearTask(taskHandle);
            taskHandle = 0;
        }

        if( DAQmxFailed(error) ) {
            QString e;
            e.sprintf("DAQmx Error: %s\nDAQMxBase Call: %s",errBuff, callStr);
            if (!noDaqErrPrint) {
                Error() << e;
            }
            emit daqError(e);
        }   

        Debug() << "AOWrite thread ended after " << aoWriteCt << " chunks written.";
    }

    /* static */
    void NITask::recomputeAOAITab(QVector<QPair<int, int> > & aoAITab, QString & aoChan, const Params & p)
    {
        aoAITab.clear();
        aoChan.clear();
        if (p.aoPassthru) {
            const QVector<QString> aoChanStrings ((ProbeAllAOChannels()[p.aoDev]).toVector());
            const int aoNChans = aoChanStrings.size();
            QSet<int> seenAO;
            aoAITab.reserve(aoNChans > 0 ? aoNChans : 0); ///< map of AO chan id to virtual AI chan id
            for (int i = 0; i < aoNChans; ++i) {
                if (p.aoPassthruMap.contains(i) && !seenAO.contains(i)) { 
                    aoAITab.push_back(QPair<int, int>(i,p.aoPassthruMap[i]));
                    seenAO.insert(i);
                    //build chanspec string for AI
                    aoChan.append(QString("%1%2").arg(aoChan.length() ? ", " : "").arg(aoChanStrings[i]));
                }
            }
        }
    }
    
    static inline int mapNewChanIdToPreJuly2011ChanId(int c, DAQ::Mode m, bool dualDevMode) {
        const int intan = c/DAQ::ModeNumChansPerIntan[m], chan = c % DAQ::ModeNumChansPerIntan[m];
        return chan*(DAQ::ModeNumIntans[m] * (dualDevMode ? 2 : 1)) + intan;
    }
    
    bool NITask::createAITasks() 
    {
        const DAQ::Params & p(params);

        if (    DAQmxErrChkNoJump (DAQmxCreateTask("",&taskHandle)) 
             || DAQmxErrChkNoJump (DAQmxCreateAIVoltageChan(taskHandle,chan.toUtf8().constData(),"",(int)p.aiTerm,min,max,DAQmx_Val_Volts,NULL))
             || DAQmxErrChkNoJump (DAQmxCfgSampClkTiming(taskHandle,clockSource,sampleRate,DAQmx_Val_Rising,DAQmx_Val_ContSamps,bufferSize)) 
             || DAQmxErrChkNoJump (DAQmxTaskControl(taskHandle, DAQmx_Val_Task_Commit)) )
            return false; 
        //DAQmxErrChk (DAQmxCfgInputBuffer(taskHandle,dmaBufSize));  //use a 1,000,000 sample DMA buffer per channel
        //DAQmxErrChk (DAQmxRegisterEveryNSamplesEvent (taskHandle, DAQmx_Val_Acquired_Into_Buffer, everyNSamples, 0, DAQPvt::everyNSamples_func, this)); 
        if (p.dualDevMode) {
            const char * clockSource2 = clockSource;//*/"PFI2";
//#ifdef DEBUG
//            clockSource2 = clockSource;
//#endif
            if (    DAQmxErrChkNoJump (DAQmxCreateTask((QString("")+QString::number(qrand())).toUtf8(),&taskHandle2))
                 || DAQmxErrChkNoJump (DAQmxCreateAIVoltageChan(taskHandle2,chan2.toUtf8().constData(),"",(int)p.aiTerm,min,max,DAQmx_Val_Volts,NULL)) 
                 || DAQmxErrChkNoJump (DAQmxCfgSampClkTiming(taskHandle2,clockSource2,sampleRate,DAQmx_Val_Rising,DAQmx_Val_ContSamps,bufferSize2)) 
                 || DAQmxErrChkNoJump (DAQmxTaskControl(taskHandle2, DAQmx_Val_Task_Commit)) )
                 return false;
        }
        return true;
    }

    bool NITask::startAITasks() 
    {
        const DAQ::Params & p(params);
        if ( DAQmxErrChkNoJump(DAQmxStartTask(taskHandle)) )
            return false; 
        if (p.dualDevMode) { 
            if ( DAQmxErrChkNoJump(DAQmxStartTask(taskHandle2)) )
                return false; 
        }
        return true;
    }

    void NITask::destroyAITasks()
    {
        if ( taskHandle != 0) {
            DAQmxStopTask (taskHandle);
            DAQmxClearTask (taskHandle);
            taskHandle = 0;
        }
        if ( taskHandle2 != 0) {
            DAQmxStopTask (taskHandle2);
            DAQmxClearTask (taskHandle2);
            taskHandle2 = 0;
        }
    }

    // returns 0 if all ok, -1 if unrecoverable error, 1 if had "buffer overflow error" and tried did acq restart..
    int NITask::doAIRead(TaskHandle th, u64 samplesPerChan, std::vector<int16> & data, unsigned long oldS, int32 pointsToRead, int32 & pointsRead)
    {
        const DAQ::Params & p(params);
        if (DAQmxErrChkNoJump (DAQmxReadBinaryI16(th,samplesPerChan,timeout,DAQmx_Val_GroupByScanNumber,&data[oldS],pointsToRead,&pointsRead,NULL))) {
            Debug() << "Got error number on AI read: " << error;
            if (p.autoRetryOnAIOverrun && acceptableRetryErrors.contains(error)) {
                if (nReadRetries > 2) {
                    Error() << "Auto-Retry of AI read failed due to too many consecutive overrun errors!";
                    return -1;
                }
                Debug() << "Error ok? Ignoring/Retrying read.. ";
                double t0 = Util::getTime();
                static const double fixedRestartTime = 0.050;
                destroyAITasks();
                // XXX TODO FIXME -- figure out if we need to settle here for some time to restart the task
                if (muxMode) { setDO(false); /*QThread::msleep(100);*/ }
                if ( !createAITasks() || !startAITasks() )
                    return -1;
                const double tleft = fixedRestartTime - (Util::getTime()-t0);
                if (tleft > 0.0) QThread::usleep(tleft*1e6);
                if (muxMode) setDO(true);
                Debug() << "Restart elapsed time: " << ((Util::getTime()-t0)*1000.0) << "ms";
                ++nReadRetries;
                fudgeDataDueToReadRetry();
                return 1;
            } else
                return -1;
        }
        nReadRetries = 0;
        return 0;
    }

    void NITask::fudgeDataDueToReadRetry()
    {
        const DAQ::Params & p (params);
        const double tNow = Util::getTime();
        const double tFudge = tNow - lastEnq;
        u64 samps2Fudge = static_cast<u64>(p.srate * tFudge) * static_cast<u64>(p.nVAIChans);
        Debug() << "Fudging " << (tFudge*1e3) << "ms worth of data (" << samps2Fudge << " samples)..";
        std::vector<int16> dummy;
        enqueueBuffer(dummy, totalRead, true, samps2Fudge);
        if (aoWriteThr) {
            u64 aosamps2fudge = static_cast<u64>(p.srate * tFudge) * static_cast<u64>(aoAITab.size());
            Debug() << "Fudging " << (tFudge*1e3) << "ms worth of AO data as well (" << aosamps2fudge << " samples)..";
            aoWriteThr->enqueueBuffer(dummy, aoSampCount, true, aosamps2fudge);
            aoSampCount += aosamps2fudge;
        }
        totalRead += samps2Fudge;
        lastEnq = tNow;
    }

    void NITask::daqThr()
    {
        // Task parameters
        error = 0;
        taskHandle = 0, taskHandle2 = 0;
        errBuff[0]='\0';
        callStr = "";
        double startTime, lastReadTime;
        const Params & p (params);

        // used for auto-retry code .. the following status codes are accepted for auto-retry, if p.autoRetryOnAIOverrun is true
        acceptableRetryErrors.clear(); acceptableRetryErrors << -200279;
        nReadRetries = 0;

        // Channel spec string for NI driver
        chan = "", chan2 = "";
        QString aoChan = "";
          
        {
            const QVector<QString> aiChanStrings ((ProbeAllAIChannels()[p.dev]).toVector());
            //build chanspec string for aiChanStrings..
            for (QVector<unsigned>::const_iterator it = p.aiChannels.begin(); it != p.aiChannels.end(); ++it) 
            {
                chan.append(QString("%1%2").arg(chan.length() ? ", " : "").arg(aiChanStrings[*it]));
            }
            
        }
        if (p.dualDevMode) {
            const QVector<QString> aiChanStrings2 ((ProbeAllAIChannels()[p.dev2]).toVector());
            //build chanspec string for aiChanStrings..
            for (QVector<unsigned>::const_iterator it = p.aiChannels2.begin(); it != p.aiChannels2.end(); ++it) 
            {
                chan2.append(QString("%1%2").arg(chan2.length() ? ", " : "").arg(aiChanStrings2[*it]));
            }
            
        }
        
        const int nChans = p.aiChannels.size(), nChans2 = p.dualDevMode ? p.aiChannels2.size() : 0;
        min = p.range.min;
        max = p.range.max;
        const int nExtraChans1 = p.nExtraChans1, nExtraChans2 = p.dualDevMode ? p.nExtraChans2 : 0;
        

        // Params dependent on mode and DAQ::Params, etc
        clockSource = p.extClock ? "PFI2" : "OnboardClock"; ///< TODO: make extClock possibly be something other than PFI2
        sampleRate = p.srate;
        timeout = DAQ_TIMEOUT;
        const int NCHANS1 = p.nVAIChans1, NCHANS2 = p.dualDevMode ? p.nVAIChans2 : 0; 
        muxMode =  (p.mode != AIRegular);
        int nscans_per_mux_scan = 1;
        aoWriteThr = 0;

        if (muxMode) {
            const int mux_chans_per_phys = ModeNumChansPerIntan[p.mode];
            sampleRate *= double(mux_chans_per_phys);
            nscans_per_mux_scan = mux_chans_per_phys;
            /*if (!p.extClock) {
                /// Aieeeee!  Need ext clock for demux mode!
                QString e("Aieeeee!  Need to use an EXTERNAL clock for DEMUX mode!");
                Error() << e;
                emit daqError(e);
                return;
            }
             */
        }

        recomputeAOAITab(aoAITab, aoChan, p);
        
        const int task_read_freq_hz = computeTaskReadFreq(p.srate);
        
        int fudged_srate = ceil(sampleRate);
        while ((fudged_srate/task_read_freq_hz) % 2) // samples per chan needs to be a multiple of 2
            ++fudged_srate;
        bufferSize = u64(fudged_srate*nChans);
        bufferSize2 = u64(fudged_srate*nChans2);
        if (p.lowLatency)
            bufferSize /= (task_read_freq_hz); ///< 1/10th sec per read
        else 
            bufferSize *= double(p.aiBufferSizeCS) / 100.0; ///< otherwise just use user spec..
        if (p.dualDevMode) {
            if (p.lowLatency)
                bufferSize2 /= (task_read_freq_hz); ///< 1/10th sec per read
            else 
                bufferSize2 *= double(p.aiBufferSizeCS) / 100.0; ///< otherwise just use user spec..            
        }
        
        if (bufferSize < NCHANS1) bufferSize = NCHANS1;
        if (bufferSize2 < NCHANS2) bufferSize2 = NCHANS2;
/*        if (bufferSize * task_read_freq_hz != u64(fudged_srate*nChans)) // make sure buffersize is on scan boundary?
            bufferSize += task_read_freq_hz - u64(fudged_srate*nChans)%task_read_freq_hz; */
        if (bufferSize % nChans) // make sure buffer is on a scan boundary!
            bufferSize += nChans - (bufferSize%nChans);
        if (p.dualDevMode && bufferSize2 % nChans2) // make sure buffer is on a scan boundary!
            bufferSize2 += nChans2 - (bufferSize2%nChans2);
        
        //const u64 dmaBufSize = p.lowLatency ? u64(100000) : u64(1000000); /// 1000000 sample DMA buffer per chan?
        const u64 samplesPerChan = bufferSize/nChans, samplesPerChan2 = (nChans2 ? bufferSize2/nChans2 : 0);

        // Timing parameters
        int32       pointsRead=0, pointsRead2=0;
        const int32 pointsToRead = bufferSize, pointsToRead2 = bufferSize2;
        std::vector<int16> data, data2, leftOver, leftOver2, aoData;       

        QMap<unsigned, unsigned> saved_aoPassthruMap = p.aoPassthruMap;
        QString saved_aoDev = p.aoDev;
        Range saved_aoRange = p.aoRange;
        QString saved_aoClock = p.aoClock;
        double saved_aoSrate = p.aoSrate;
        unsigned saved_aoBufferSizeCS = p.aoBufferSizeCS; 


        if (muxMode) {
            setDO(false);// set DO line low to reset external MUX
            msleep(1000); // keep it low for 1 second
        }

        if ( !createAITasks() ) goto Error_Out;

        if (p.aoPassthru && aoAITab.size()) {
            aoWriteThr = new AOWriteThread(0, aoChan, p);
            Connect(aoWriteThr, SIGNAL(daqError(const QString &)), this, SIGNAL(daqError(const QString &)));
            aoWriteThr->start();                
        }

        if ( !startAITasks() ) goto Error_Out;

        if (muxMode) {
            setDO(true); // now set DO line high to start external MUX and clock on PFI2
        }

        lastEnq = lastReadTime = startTime = getTime();
        aoSampCount = 0;
        double hzAvg = 0.;
        int nHz = 0, nHzMax = 20;

        while( !pleaseStop ) {
            data.clear(); // should already be cleared, but enforce...
            data2.clear();
            if (leftOver.size()) data.swap(leftOver);            
            if (leftOver2.size()) data2.swap(leftOver2);
            unsigned long oldS = data.size(), oldS2 = data2.size();
            data.reserve(pointsToRead+oldS);
            data.resize(pointsToRead+oldS);

            double tr0 = getTime();

            // XXX DEBUG HACK.. test DAQ retry mechanism
            /*static int ctr = 0;
            if (++ctr >= 10) {
                QThread::msleep(1000);
                ctr = 0;
            }*/

            //DAQmxErrChk (DAQmxReadBinaryI16(taskHandle,samplesPerChan,timeout,DAQmx_Val_GroupByScanNumber,&data[oldS],pointsToRead,&pointsRead,NULL));
            int retVal = doAIRead(taskHandle, samplesPerChan, data, oldS, pointsToRead, pointsRead);
            if (retVal < 0) goto Error_Out; // fatal
            else if (retVal > 0) continue; // retry
            // else retval == 0, all ok

            double hz = pointsRead/(getTime()-tr0);
            hzAvg = ((hzAvg*nHz) + hz) / (nHz+1);
            if (++nHz >= nHzMax) nHz = nHzMax-1;
            if (excessiveDebug) Debug() << "read rate: " << hz << " Hz, avg " << hzAvg << " Hz";

            if (p.dualDevMode) {
                data2.reserve(pointsToRead2+oldS2);
                data2.resize(pointsToRead2+oldS2);                
                //DAQmxErrChk (DAQmxReadBinaryI16(taskHandle2,samplesPerChan2,timeout,DAQmx_Val_GroupByScanNumber,&data2[oldS2],pointsToRead2,&pointsRead2,NULL));
                retVal = doAIRead(taskHandle2, samplesPerChan2, data2, oldS2, pointsToRead2, pointsRead2);
                if (retVal < 0) goto Error_Out; // fatal
                else if (retVal > 0) continue; // retry
                // else retval == 0, all ok

                // TODO FIXME XXX -- *use* this data.. for now we are just testing
                //Debug() << "Read " << pointsRead2 << " samples from second dev.\n";
            }
            lastReadTime = getTime();
            u64 sampCount = totalRead;
            if (!sampCount) emit(gotFirstScan());
            int32 nRead = pointsRead * nChans + oldS, nRead2 = pointsRead2 * nChans2 + oldS2;                  
            int nDemuxScans = nRead/nChans/nscans_per_mux_scan, nDemuxScans2 = 1;
            if ( nDemuxScans*nscans_per_mux_scan*nChans != nRead ) {// not on 60 (or 75 if have interwoven PD and in PD mode) channel boundary, so save extra channels and enqueue them next time around..
                nRead = nDemuxScans*nscans_per_mux_scan*nChans;
                leftOver.insert(leftOver.end(), data.begin()+nRead, data.end());
                data.erase(data.begin()+nRead, data.end());
            }
            if (p.dualDevMode && nChans2) {
                nDemuxScans2 = nRead2 / nChans2 / nscans_per_mux_scan;
                if (nDemuxScans2 * nscans_per_mux_scan * nChans2 != nRead2) {
                    nRead2 = nDemuxScans2*nscans_per_mux_scan*nChans2;
                    leftOver2.insert(leftOver2.end(), data2.begin()+nRead2, data2.end());
                    data2.erase(data2.begin()+nRead2, data2.end());                    
                }
            } else
                nRead2 = 0;
            
            if (p.dualDevMode && nDemuxScans != nDemuxScans2) { // ensure same exact number of scans
                if (nDemuxScans > nDemuxScans2) {
                    const int diff = nDemuxScans-nDemuxScans2;
                    nRead = (nDemuxScans-diff)*nscans_per_mux_scan*nChans;
                    leftOver.insert(leftOver.end(), data.begin()+nRead, data.end());
                    data.erase(data.begin()+nRead, data.end());                    
                    nDemuxScans -= diff;                    
                } else if (nDemuxScans2 > nDemuxScans) {
                    const int diff = nDemuxScans2-nDemuxScans;
                    nRead2 = (nDemuxScans2-diff)*nscans_per_mux_scan*nChans2;
                    leftOver2.insert(leftOver2.end(), data2.begin()+nRead2, data2.end());
                    data2.erase(data2.begin()+nRead2, data2.end());                    
                    nDemuxScans2 -= diff;
                }
            }
            
            // at this point we have scans of size 60 (or 75) channels (or 32 in JFRCIntan32)
            // in the 75-channel case we need to throw away 14 of those channels since they are interwoven PD-channels!
            data.resize(nRead);
            data2.resize(nRead2);
            if (!nRead) {
                Warning() << "Read less than a full scan from DAQ hardware!  FIXME on line:" << __LINE__ << " in " << __FILE__ << "!";
                continue; 
            }

            //Debug() << "Acquired " << nRead << " samples. Total " << totalRead;
            if (muxMode && (nExtraChans1 || nExtraChans2)) {
                /*
                  NB: at this point data contains scans of the form:

                
                  | 0 | 1 | 2 | 3 | Extra 1 | Extra 2 | PD | ...
                  | 4 | 5 | 6 | 7 | Extra 1 | Extra 2 | PD | ... 
                  | 56| 57| 58| 59| Extra 1 | Extra 2 | PD |
                  ---------------------------------------------------------------

                  Notice how the Extra channels are interwoven into the
                  0:3 (or 0:7 if in 120 channel mode) AI channels.

                  We need to remove them!

                  We want to turn that into 1 (or more) demuxed VIRTUAL scans
                  of either form:
                 
                  Pre-July 2011 ordering, which was INTAN_Channel major:

                  | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | ... | 58 | 59 | Extra 1 | Extra 2| PD |
                  -------------------------------------------------------------------------

                  Or, the current ordering, which is INTAN major:
                 
                  | 0 | 4 | 8 | 12 | 16 | 20 | 24 | 28 | ... | 55 | 59 | Extra 1 | Extra 2| PD |
                  ------------------------------------------------------------------------------
                 
                  Where we remove every `nChans' extraChans except for when 
                  we have (NCHANS-extraChans) samples, then we add the extraChans, 
                  effectively downsampling the extra channels from 444kHz to 
                  29.630 kHz.

                  Capisce?

                  NB: nChans = Number of physical channels per physical scan
                  NCHANS = Number of virtual channels per virtual scan

                */
                std::vector<int16> tmp, tmp2;
                const int datasz = data.size(), datasz2 = data2.size();
                tmp.reserve(datasz);
                tmp2.reserve(datasz2);
                const int nMx = nChans-nExtraChans1, nMx2 = nChans2-nExtraChans2;
                if (nMx > 0) {
                    for (int i = nMx; nExtraChans1 && i <= datasz; i += nChans) {
                        std::vector<int16>::const_iterator begi = data.begin()+(i-nMx), endi = data.begin()+i;
                        tmp.insert(tmp.end(), begi, endi); // copy non-aux channels for this MUXed sub-scan
                        if ((!((tmp.size()+nExtraChans1) % NCHANS1)) && i+nExtraChans1 <= datasz) {// if we are on a virtual scan-boundary, then ...
                            begi = data.begin()+i;  endi = begi+nExtraChans1;
                            tmp.insert(tmp.end(), begi, endi); // insert the scan into the destination data
                        }
                    }
                } else { // not normally reached currently, but just in case we are MUX mode and 1st dev is *ALL* AUX, we take every Nth scan
                    for (int i = 0; nExtraChans1 && i+nChans <= datasz; i += nChans * nscans_per_mux_scan) {
                        tmp.insert(tmp.end(), data.begin()+i, data.begin()+i+nChans);
                    }
                }
                if (nMx2 > 0) { // if we _aren't_ in "secondDevIsAuxOnly" mode
                    for (int i = nMx2; p.dualDevMode && nExtraChans2 && i <= datasz2; i += nChans2) {
                        std::vector<int16>::const_iterator begi = data2.begin()+(i-nMx2), endi = data2.begin()+i;
                        tmp2.insert(tmp2.end(), begi, endi); // copy non-aux channels for this MUXed sub-scan
                        if ((!((tmp2.size()+nExtraChans2) % NCHANS2)) && i+nExtraChans2 <= datasz2) {// if we are on a virtual scan-boundary, then ...
                            begi = data2.begin()+i;  endi = begi+nExtraChans2;
                            tmp2.insert(tmp2.end(), begi, endi); // insert the scan into the destination data
                        }
                    }
                } else { // we are in "secondDevIsAuxOnly" mode, so take every Nth scan to match first dev sampling rate..
                    for (int i = 0; nExtraChans2 && i+nChans2 <= datasz2; i += nChans2 * nscans_per_mux_scan) {
                        tmp2.insert(tmp2.end(), data2.begin()+i, data2.begin()+i+nChans2);
                    }
                }
                if (tmp.size()) { data.swap(tmp); nRead = data.size(); }
                if (tmp2.size()) { data2.swap(tmp2); nRead2 = data2.size(); }
                if ((data.size()+data2.size()) % (NCHANS1+NCHANS2)) {
                    // data didn't end on scan-boundary -- we have leftover scans!
                    Error() << "INTERNAL ERROR SCAN DIDN'T END ON A SCAN BOUNDARY FIXME!!! in " << __FILE__ << ":" << __LINE__;                 
                }
            }
            
            if (p.dualDevMode) {
                std::vector<int16> out;
                mergeDualDevData(out, data, data2, NCHANS1, NCHANS2, nExtraChans1, nExtraChans2);
                data.swap(out);
            }

            totalReadMut.lock();
            totalRead += static_cast<u64>(nRead) + static_cast<u64>(nRead2);
            totalReadMut.unlock();

            // note that from this point forward, the 'data' buffer is the only valid buffer
            // and it contains the MERGED data from both devices if in dual dev mode.
            
            // now, do optional AO output .. done in another thread to save on latency...
            if (aoWriteThr) {  
                {   // detect AO passthru changes and re-setup the AO task if ao passthru spec changed
                    QMutexLocker l(&p.mutex);
                    if (saved_aoPassthruMap != p.aoPassthruMap
                        || saved_aoRange != p.aoRange
                        || saved_aoDev != p.aoDev
                        || saved_aoClock != p.aoClock
                        || saved_aoSrate != p.aoSrate
                        || saved_aoBufferSizeCS != p.aoBufferSizeCS) {
                        // delete aoWriteThr, aoWriteThr = 0;   <--- SLOW SLOW SLOW EXPENSIVE OPERATION!  So we don't do it here, instead we defer deletion to the new AOWriteThread that replaces this one!
                        aoData.clear();
                        recomputeAOAITab(aoAITab, aoChan, p);
                        saved_aoPassthruMap = p.aoPassthruMap;
                        saved_aoRange = p.aoRange;
                        saved_aoDev = p.aoDev;
                        saved_aoClock = p.aoClock;
                        saved_aoSrate = p.aoSrate;
                        saved_aoBufferSizeCS = p.aoBufferSizeCS;
                        /* Note: deleting the old AOWrite thread is potentially slow due to expensive/slow wait for DAQmxWriteBinary to finish when joining threads
                                 so.. we tell the newly created aowrite thread to delete the old thread for us!  Clever! :) */
                        aoWriteThr = new AOWriteThread(0, aoChan, p, aoWriteThr /* old thread to delete when start() is called... */);
                        Connect(aoWriteThr, SIGNAL(daqError(const QString &)), this, SIGNAL(daqError(const QString &)));
                        aoWriteThr->start();
                    }
                }
                const int dsize = data.size();
                aoData.reserve(aoData.size()+dsize);
                for (int i = 0; i < dsize; i += NCHANS1+NCHANS2) { // for each scan..
                    for (QVector<QPair<int,int> >::const_iterator it = aoAITab.begin(); it != aoAITab.end(); ++it) { // take ao channels
						const int FIRSTAUX = muxMode ? (NCHANS1+NCHANS2) - (nExtraChans1+nExtraChans2) : 0;
                        const int aiChIdx = ( (p.doPreJuly2011IntanDemux || !muxMode || ((*it).second) >= FIRSTAUX) ? ((*it).second) : mapNewChanIdToPreJuly2011ChanId((*it).second, p.mode, p.dualDevMode && !p.secondDevIsAuxOnly) );
                        const int dix = i+aiChIdx;
                        if (dix < dsize)
                            aoData.push_back(data[dix]);
                        else {
                            static int errct = 0;
                            aoData.push_back(0);
                            if (errct++ < 5)
                                Error() << "INTERNAL ERROR: This shouldn't happen.  AO passthru code is buggy. FIX ME!!";
                        }
                    }
                }
                u64 sz = aoData.size();
                aoWriteThr->enqueueBuffer(aoData, aoSampCount);
                aoSampCount += sz;
            }
            
            breakupDataIntoChunksAndEnqueue(data, sampCount, p.autoRetryOnAIOverrun);
            lastEnq = lastReadTime;

            // fast settle...
            if (muxMode && fast_settle && !leftOver.size() && !leftOver2.size()) {
                double t0 = getTime();
                /// now possibly do a 'fast settle' request by stopping the task, setting the DO line low, then restarting the task after fast_settle ms have elapsed, and setting the line high
                Debug() << "Fast settle of " << fast_settle << " ms begin";
                DAQmxErrChk(DAQmxStopTask(taskHandle));
                if (p.dualDevMode) DAQmxErrChk(DAQmxStopTask(taskHandle2));
                if (aoWriteThr) {
                    delete aoWriteThr, aoWriteThr = 0; 
                    aoWriteThr = new AOWriteThread(0, aoChan, p);
                    Connect(aoWriteThr, SIGNAL(daqError(const QString &)), this, SIGNAL(daqError(const QString &)));
                    aoWriteThr->start();
                }
                setDO(false);
                msleep(fast_settle);
                fast_settle = 0;
                DAQmxErrChk(DAQmxStartTask(taskHandle));                
                if (p.dualDevMode) DAQmxErrChk(DAQmxStartTask(taskHandle2));    
                // no need to restart AO as it is autostart..
                setDO(true);
                Debug() << "Fast settle completed in " << (getTime()-t0) << " secs";
                emit(fastSettleCompleted());
            }

        }

        Debug() << "Acquired " << totalRead << " total samples.";

    Error_Out:    
        if ( DAQmxFailed(error) )
            DAQmxGetExtendedErrorInfo(errBuff,2048);
        destroyAITasks();
        if (aoWriteThr != 0) {
            delete aoWriteThr, aoWriteThr = 0;
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
    
    /*static*/
    inline void NITask::mergeDualDevData(std::vector<int16> & out,
                                const std::vector<int16> & data, const std::vector<int16> & data2, 
                                int NCHANS1, int NCHANS2, 
                                int nExtraChans1, int nExtraChans2)
    {
        const int nMx = NCHANS1-nExtraChans1, nMx2 = NCHANS2-nExtraChans2, s1 = data.size(), s2 = data2.size();
        out.clear();
        out.reserve(s1+s2);
        int i,j;
        for (i = 0, j = 0; i < s1 && j < s2; i+=NCHANS1, j+=NCHANS2) {
            if (nMx > 0) out.insert(out.end(), data.begin()+i, data.begin()+i+nMx);
            if (nMx2 > 0) out.insert(out.end(), data2.begin()+j, data2.begin()+j+nMx2);
            out.insert(out.end(), data.begin()+i+nMx, data.begin()+i+NCHANS1);
            out.insert(out.end(), data2.begin()+j+nMx2, data2.begin()+j+NCHANS2);
        }
        if (i < s1 || j < s2) 
            Error() << "INTERNAL ERROR IN FUNCTION `mergeDualDevData()'!  The two device buffers data and data2 have differing numbers of scans! FIXME!  Aieeeee!!\n";
    }    

    void NITask::breakupDataIntoChunksAndEnqueue(std::vector<int16> & data, u64 sampCount, bool putFakeDataOnOverrun)
    {
        int chunkSize = int(params.srate / computeTaskReadFreq(params.srate)) * params.nVAIChans, nchunk = 0, ct = 0;
        if (chunkSize < (int)params.nVAIChans) chunkSize = params.nVAIChans;
        static const int limit = (1024*786)/sizeof(int16); /* 768KB max chunk size? */
        if (chunkSize > limit) { 
            chunkSize = limit - (limit % params.nVAIChans);
        }
        const int nSamps = data.size();
        if (chunkSize >= nSamps) {
            enqueueBuffer(data, sampCount, putFakeDataOnOverrun);
            return;
        }
        std::vector<int16> chunk;
        for (int i = 0; i < nSamps; i += nchunk, ++ct) {
            chunk.reserve(chunkSize);
            nchunk = nSamps - i;
            if (nchunk > chunkSize) nchunk = chunkSize;
            chunk.insert(chunk.begin(), data.begin()+i, data.begin()+i+nchunk);
            enqueueBuffer(chunk, sampCount+u64(i), putFakeDataOnOverrun);
            chunk.clear();
        }
        //Debug() << "broke up data of size " << data.size() << " into " << ct << " chunks of size " << chunkSize;
        data.clear();
    }

    void NITask::setDO(bool onoff)
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

    void NITask::requestFastSettle() 
    {
        if (!muxMode) {
            Warning() << "Fast settle requested -- but not running in MUX mode!  FIXME!";
            return;
        }
        unsigned ms = params.fastSettleTimeMS;
        if (!fast_settle) {
            if (ms > 10000) { ///< hard limit on ms
                Warning() << "Requested fast settled of " << ms << " ms, limiting to 10000 ms";
                ms = 10000;
            }
            fast_settle = ms;
        } else {
            Warning() << "Dupe fast settle requested -- fast settle already running!";
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

#endif // ! FAKEDAQ


    /// some helper funcs from SpikeGL.h
    static const QString acqModes[] = { "AI60Demux", "AIRegular", "AI120Demux", "JFRCIntan32", "AI128Demux", "AI256Demux", "AI64Demux", "AI96Demux", "AI128_32_Demux", QString::null };
    
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

    TermConfig StringToTermConfig(const QString & txt) 
    {
        if (!txt.compare("RSE", Qt::CaseInsensitive))
            return RSE;
        else if (!txt.compare("NRSE", Qt::CaseInsensitive))
            return NRSE;
        else if (!txt.compare("Differential", Qt::CaseInsensitive) 
                 || !txt.compare("Diff", Qt::CaseInsensitive) )
            return Diff;
        else if (!txt.compare("PseudoDifferential", Qt::CaseInsensitive) 
                 || !txt.compare("PseudoDiff", Qt::CaseInsensitive) )
            return PseudoDiff;
        return Default;       
    }

    QString TermConfigToString(TermConfig t)
    {
        switch(t) {
        case RSE: return "RSE";
        case NRSE: return "NRSE";
        case Diff: return "Differential";
        case PseudoDiff: return "PseudoDifferential";
        default: break;
        }
        return "Default";
    }


    static const QString acqStartEndModes[] = { "Immediate", "PDStartEnd", "PDStart", "Timed", "StimGLStartEnd", "StimGLStart", "AITriggered", "Bug3TTLTriggered", QString::null };
    
    const QString & AcqStartEndModeToString(AcqStartEndMode m) {
        if (m >= 0 && m < N_AcqStartEndModes) 
            return acqStartEndModes[(int)m];
        static QString unk ("Unknown");
        return unk;
    }

	Task::Task(QObject *parent, const QString & name, unsigned max)
		: QThread(parent), SampleBufQ(name, max), totalRead(0ULL)
	{
	
	}
	
	Task::~Task() { }
	
	u64 Task::lastReadScan() const
    {
        u64 ret (0);
        totalReadMut.lock();
        ret = totalRead / numChans();
        totalReadMut.unlock();
        return ret;
    }
	
	SubprocessTask::SubprocessTask(const DAQ::Params & p, QObject *parent,
									   const QString & shortName,
								   const QString & exeName)
	: Task(parent, shortName + " DAQ task", SAMPLE_BUF_Q_SIZE), params(p),
	  shortName(shortName), dirName(shortName + "_SpikeGLv" + QString(VERSION_STR).right(8)), exeName(exeName)
	{
        QString tp(QDir::tempPath());
        if (!tp.endsWith("/")) tp.append("/");
        exeDir = tp + dirName + "/";
        Debug() << shortName << " exedir = " << exeDir;
    }
	
	SubprocessTask::~SubprocessTask() { if (isRunning()) { stop(); wait(); } }
	
	void SubprocessTask::stop() { pleaseStop = true; }
		
    QString SubprocessTask::exePath() const { return exeDir + exeName; }
	
	bool SubprocessTask::setupExeDir(QString *errOut) const
	{
		if (errOut) *errOut = "";
        QDir().mkpath(exeDir);
        QDir d(exeDir);
        if (!d.exists()) { if (errOut) *errOut = QString("Could not create ") + exeDir; return false; }
		QStringList files = filesList();
		for (QStringList::const_iterator it = files.begin(); it != files.end(); ++it) {
			const QString bn ((*it).split("/").last());
            const QString dest (exeDir + bn);
			if (QFile::exists(dest) && !QFile::remove(dest)) {
				if (errOut) *errOut = dest + " exists and cannot be removed.";
				return false;
			}
			if (!QFile::copy(*it, dest)) {
				if (errOut) *errOut = dest + " file create/write failed.";
				return false;
			}
			QFile f(dest);
			if (dest.endsWith(".exe") || dest.endsWith(".dll")) 
                f.setPermissions(f.permissions()|QFile::ReadOwner|QFile::ExeOwner|QFile::WriteOwner|QFile::WriteOther|QFile::WriteGroup|QFile::ReadOther|QFile::ExeOther|QFile::ReadGroup|QFile::ExeGroup);
			else
                f.setPermissions(f.permissions()|QFile::ReadOwner|QFile::ReadOther|QFile::ReadGroup|QFile::WriteOwner|QFile::WriteOther|QFile::WriteGroup);
		}
		return true;
	}
	
	void SubprocessTask::slaveProcStateChanged(QProcess::ProcessState ps)
	{
		switch(ps) {
			case QProcess::NotRunning:
				Debug() << shortName << " slave process got state change -> NotRunning";
				if (isRunning() && !pleaseStop) {
					emit daqError(shortName + " slave process died unexpectedly!");
					pleaseStop = true;
					Debug() << shortName << " task thread still running but slave process died -- signaling BugTask::daqThr() to end as a result...";
				} 
				break;
			case QProcess::Starting:
				Debug() << shortName << " slave process got state change -> Starting";
				break;
			case QProcess::Running:
				Debug() << shortName << " slave process got state change -> Running";
				break;
		}
	}
	
	void SubprocessTask::daqThr() 
	{		
		if (!platformSupported()) {
			emit daqError(QString("This platform does not support the ") + shortName + " acquisition mode!");
			pleaseStop = true;
			return;			
		}
		
		pleaseStop = false;
		QString err("");
		if (!setupExeDir(&err)) {
			emit daqError(err);
			return;
		}
        QProcess p(0); // Qt throws a warning if we create the process with "this" as its QObject parent because 'this' object lives in another thread. Weird.  No harm tho: note that even though the qprocess has no parent, it's ok, it will die anyway :)
		//		p.moveToThread(this);
		static bool RegisteredMetaType = false;
		if (!RegisteredMetaType) {
			int id = qRegisterMetaType<QProcess::ProcessState>("QProcess::ProcessState");
			(void) id;
			RegisteredMetaType = true;
		}
		Connect(&p, SIGNAL(stateChanged(QProcess::ProcessState)), this, SLOT(slaveProcStateChanged(QProcess::ProcessState)));		
        p.setProcessChannelMode(usesMergedChannels() ? QProcess::MergedChannels : QProcess::SeparateChannels);
        p.setWorkingDirectory(exeDir);
		QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
		setupEnv(env);
		p.setProcessEnvironment(env);
		if (!interpreter().length()) {
			p.start(exePath());
		} else  {
			p.start(interpreter(), QStringList()<<exePath());
		} 
		if (p.state() == QProcess::NotRunning) {
			emit daqError(shortName + " slave process startup failed!");
			p.kill();
			return;
		}
		if (!p.waitForStarted(30000)) {
			emit daqError(shortName + " slave process: startup timed out!");
			p.kill();
			return;
		}
		Debug() << shortName << " slave process started ok";
        emit(justStarted());

		int tout_ct = 0;
		QByteArray data;
		
        int notRunningCt = 0;
		
		data.reserve(4*1024*1024);
        while (!pleaseStop /*&& p.state() != QProcess::NotRunning*/) {
			if (p.state() == QProcess::Running) { 
				qint64 n_avail = p.bytesAvailable();
                if (n_avail == 0 && !p.waitForReadyRead(1000)) {
					if (++tout_ct > readTimeoutMaxSecs()) {
						emit(daqError(shortName + " slave process: timed out while waiting for data!"));
						p.kill();
						return;
					} else {
						Warning() << shortName << " slave process: read timed out, ct=" << tout_ct;
                        readStdErr(p);
                        processCmds(p);
						continue;
					}
				} else if (n_avail < 0) {
                    emit(daqError(shortName + " slave process: eof on stdout stream!"));
					p.kill();
					return;
				}
				if (--tout_ct < 0) tout_ct = 0;
				QByteArray buf = p.readAll();
				if (!buf.size()) {
					Warning() << shortName << " slave process: read 0 bytes!";
                    readStdErr(p);
                    processCmds(p);
					continue;
				}
				data.append(buf);
				unsigned consumed = gotInput(data, buf.size(), p);
				data.remove(0,consumed);
				if (excessiveDebug && data.size()) {
					Debug() << shortName << " slave process: partial data left over " << data.size() << " bytes";
				}
				
                readStdErr(p);
				processCmds(p);
				
            } else {
                msleep(10); // sleep 10ms and try again..
                if (++notRunningCt > 100) {
                    emit daqError(shortName + " slave process died unexpectedly!");
                    p.kill();
                    return;
                }
            }
			
		}
		if (p.state() == QProcess::Running) {
			sendExitCommand(p);
			Debug() << "Sent 'exit' cmd to " << shortName << " slave process...";
			p.waitForFinished(500); /// wait 500 msec for finish
		}
		if (p.state() != QProcess::NotRunning) {
			p.kill();
			Debug() << shortName << " slave process refuses to exit gracefully.. forcibly killed!";
		}
	}
	
    void SubprocessTask::readStdErr(QProcess &p) {
        if (!usesMergedChannels()) {
            p.setReadChannel(QProcess::StandardError);
            QByteArray tmpData = p.readAll();
            p.setReadChannel(QProcess::StandardOutput);
            if (tmpData.size()) gotStdErr(tmpData);
        }
    }

    void SubprocessTask::gotStdErr(const QByteArray & data) {
        QString s(data);
        s = s.trimmed();
        Debug() << shortName << " stderr: " << s;
    }

	void SubprocessTask::pushCmd(const QByteArray &c) {
		cmdQMut.lock();
		cmdQ.push_back(c);
		cmdQMut.unlock();
	}
	
	void SubprocessTask::processCmds(QProcess & p)
	{
		QList<QByteArray> cmds;
		cmdQMut.lock();
		cmds = cmdQ;
		cmdQ.clear();
		cmdQMut.unlock();
		for (QList<QByteArray>::iterator it = cmds.begin(); it != cmds.end(); ++it) {
			if (outputCmdsAreBinary()) {
				Debug() << shortName << ": sending command of size " << (*it).size() << " bytes";				
			} else {
                Debug() << shortName << ": sending command `" << QString(*it).trimmed() << "'";
			}
			p.write(*it);
		}
	}
	

    /*static*/ const double BugTask::SamplingRate = 16.0 / 0.0006144;
    /*static*/ const double BugTask::ADCStepNeural = 2.5;                // units = uV
    /*static*/ const double BugTask::ADCStepEMG = 0.025;                 // units = mV
    /*static*/ const double BugTask::ADCStepAux = 0.0052;                // units = V
    /*static*/ const double BugTask::ADCStep = 1.02 * 1.225 / 1024.0;    // units = V
    /*static*/ const double BugTask::MaxBER = -1.0;
    /*static*/ const double BugTask::MinBER = -5.0;

	BugTask::BugTask(const DAQ::Params & p, QObject *parent)
	: SubprocessTask(p, parent, "Bug3", "bug3_spikegl.exe")
	{
		state = 0; nblocks = 0; nlines = 0;
		debugTTLStart = 0;
	}

    BugTask::~BugTask() {  if (isRunning()) { stop(); wait(); } }

    unsigned BugTask::numChans() const { return params.nVAIChans; }
    unsigned BugTask::samplingRate() const { return params.srate; }
	
	QStringList BugTask::filesList() const
	{
		QStringList files; 
		files.push_back(QString(":/Bug3/Bug3_for_SpikeGL/Bug3_for_SpikeGL/bin/Release/") + exeName);
		files.push_back(":/Bug3/Bug3_for_SpikeGL/Bug3_for_SpikeGL/bug3a_receiver_1.bit");
		files.push_back(":/Bug3/Bug3_for_SpikeGL/Bug3_for_SpikeGL/libFrontPanel-csharp.dll");
		files.push_back(":/Bug3/Bug3_for_SpikeGL/Bug3_for_SpikeGL/libFrontPanel-pinv.dll");
		return files;
	}
	
	void BugTask::setupEnv(QProcessEnvironment & env) const
	{
		env.insert("BUG3_SPIKEGL_MODE", "yes");
		switch (params.bug.rate) {
			case 0:
				env.insert("BUG3_DATA_RATE", "LOW");
				break;
			case 1:
				env.insert("BUG3_DATA_RATE", "MEDIUM");
				break;
			default:
				env.insert("BUG3_DATA_RATE", "HIGH");
				break;
		}
		env.insert("BUG3_CHANGES_ON_CLOCK", QString::number(params.bug.clockEdge));
		env.insert("BUG3_ERR_TOLERANCE", QString::number(params.bug.errTol));
		if (params.bug.hpf > 0) env.insert("BUG3_HIGHPASS_FILTER_CUTOFF", QString::number(params.bug.hpf));
		if (params.bug.snf) env.insert("BUG3_60HZ_NOTCH_FILTER", "yes");		
	}
		
	void BugTask::sendExitCommand(QProcess & p) const 
	{
		p.write("exit\r\n");				
	}
	
	int BugTask::usbDataBlockSizeSamps() const
	{
		int frameSize = 174;
		switch (params.bug.rate) {
			case 0: frameSize = 48; break;
			case 1: frameSize = 96; break;
			default: break;
		}
		return frameSize * FramesPerBlock;
	}
	
	unsigned BugTask::gotInput(const QByteArray & data, unsigned lastReadNBytes, QProcess & p) 
	{
		(void) p;
		unsigned consumed = 0;
		for (int i = (data.size()-lastReadNBytes), l = 0; i < data.size(); ++i) {
			if (data[i] == '\n') {
				QString line = data.mid(l, i-l+1);
				l = consumed = i+1;
				processLine(line, block, nblocks, state, nlines); 
				if (state > nstates) {
					processBlock(block, nblocks++);
					block.clear();
					state = 0;
				}
			}
		}
		return consumed;
	}
			
	void BugTask::processLine(const QString & lineUntrimmed, QMap<QString, QString> & block, const quint64 & nblocks, int & state, quint64 & nlines)
	{
		QString line = lineUntrimmed.trimmed();
#if 0
		if (excessiveDebug) {
			QString linecpy = line;
			if (line.length() > 75) linecpy = line.left(65) + "..." + line.right(10);
			if (linecpy.length() > 3) { Debug() << "Bug3: " << linecpy; }
		}
#endif
		if (line.startsWith("---> Console")) { 
			++nlines;
			state=1; 
			if (excessiveDebug) Debug() << "Bug3: New block, current nblocks=" << nblocks; 
		} else if (state) {
			++nlines;
			// we are in parsing mode..
			QStringList nv = line.split(QRegExp("[}{]"), QString::SkipEmptyParts);
			if (nv.count() == 2) {
				//Debug() << "Bug3: Got name=" << nv.first() << " v=" << nv.last().left(5) << "..." << nv.last().right(5);
				block[nv.first()] = nv.last();
				++state;
			}
		} else if (!state && line.startsWith("USRMSG:")) {
			emit(daqWarning(line.mid(7).trimmed()));
		}		
	}
	
	void BugTask::processBlock(const QMap<QString, QString> & blk, quint64 blockNum)
	{
		BlockMetaData meta;
		meta.blockNum = blockNum;
		const int nchans (numChans());
		std::vector<int16> samps;
		samps.resize(nchans * NeuralSamplesPerFrame * FramesPerBlock); // todo: fix to come from params?
		
		// todo implement..
		for (QMap<QString,QString>::const_iterator it = blk.begin(); it != blk.end(); ++it) {
			const QString & k = it.key(), & v(it.value());
			if (k.startsWith("NEU_")) {
				QStringList knv = k.split("_");
				int neur_chan = 0;
				bool ok = false;
				if (knv.count() == 2) {
					neur_chan = knv.last().toInt(&ok);
					if (neur_chan < 0 || neur_chan >= TotalNeuralChans) { neur_chan = 0; ok = false; }
				} 
				if (!ok) 
					Warning() << "Bug3: Internal problem -- parse error on NEU_ key.";

				QStringList nums = v.split(",");
                //if (excessiveDebug) Debug() << "Bug3: got " << nums.count() << " neural samples for NEU chan " << neur_chan << "..";
				bool warned = false;
				for (int frame = 0; frame < FramesPerBlock; ++frame) {
					for (int neurix = 0; neurix < NeuralSamplesPerFrame; ++neurix) {
						QString num = nums.empty() ? "0" : nums.front();
						if (nums.empty()) { 
							if (!warned) Warning() << "Bug3: Internal problem -- ran out of neural samples in frame `" << v << "'";
							warned = true;
						} else nums.pop_front();
						ok = false;
						int samp = num.toUShort(&ok);
						if (!ok) Error() << "Bug3: Internal error -- parse error while reading neuronal sample `" << num << "'";
						// todo.. fix this.  wtf? can't think straight now...
						samp = samp-ADCOffset; // incoming data is such that 0v = (int16)1023, this is because incoming samples are 11-bit. normalize so that 0v = 0
						samp = samp * int16(32); // promote to 16-bit
						samps[ frame*(NeuralSamplesPerFrame*nchans) + neurix*nchans + neur_chan ] = samp;
					}
				}
			} else if (k.startsWith("EMG_")) {
				QStringList knv = k.split("_");
				int emg_chan = 0;
				bool ok = false;
				if (knv.count() == 2) {
					emg_chan = knv.last().toInt(&ok);
					if (emg_chan < 0 || emg_chan >= TotalEMGChans) { emg_chan = 0; ok = false; }
				} 
				if (!ok) Warning() << "Bug3: Internal problem -- parse error on EMG_ key.";

				QStringList nums = v.split(",");
                //if (excessiveDebug) Debug() << "Bug3: got " << nums.count() << " EMG samples..";
				bool warned = false;
				for (int frame = 0; frame < FramesPerBlock; ++frame) {
					QString num = nums.empty() ? "0" : nums.front();
					if (nums.empty()) { 
						if (!warned) Warning() << "Bug3: Internal problem -- ran out of EMG samples in frame `" << v << "'";
						warned = true;
					}
					else nums.pop_front();
					bool ok = false;
					int samp = num.toUShort(&ok);
					if (!ok) Error() << "Bug3: Internal error -- parse error while reading emg sample `" << num << "'";
					samp = samp-ADCOffset; // normalize since incoming data is weird 11 bit
					samp = samp * int16(32); // promote to 16-bit
					for (int emgix = 0; emgix < NeuralSamplesPerFrame; ++emgix) { // need to produce 16 samples for each 1 emg sample read in order to match neuronal rate!						
						samps[ frame*(NeuralSamplesPerFrame*nchans) + emgix*nchans + (TotalNeuralChans+emg_chan) ] = samp;
					}

				}
			} else if (k.startsWith("AUX_")) {
				QStringList knv = k.split("_");
				int aux_chan = 0;
				bool ok = false;
				if (knv.count() == 2) {
					aux_chan = knv.last().toInt(&ok);
					if (aux_chan < 0 || aux_chan >= TotalAuxChans) { aux_chan = 0; ok = false; }
				} 
				if (!ok) Warning() << "Bug3: Internal problem -- parse error on AUX_ key.";
				bool warned = false;
				QStringList nums = v.split(",");
                //if (excessiveDebug) Debug() << "Bug3: got " << nums.count() << " AUX samples..";
				double avgVunreg = 0.0;
				for (int frame = 0; frame < FramesPerBlock; ++frame) {
					QString num = nums.empty() ? "0" : nums.front();
					if (nums.empty()) { 
						if (!warned) Warning() << "Bug3: Internal problem -- ran out of AUX samples in frame `" << v << "'";
						warned = true;
					}
					else nums.pop_front();
					bool ok = false;
					int samp = num.toUShort(&ok);
					if (!ok) Error() << "Bug3: Internal error -- parse error while reading emg sample `" << num << "'";
					samp = samp-ADCOffset; // normalize since incoming data is 11-bit
					if (aux_chan == 1)  avgVunreg += ADCStep * double(samp);
					samp = samp * int16(32); // promote to 16-bit					
					for (int auxix = 0; auxix < NeuralSamplesPerFrame; ++auxix) { // need to produce 16 samples for each 1 emg sample read in order to match neuronal rate!	
						samps[ frame*(NeuralSamplesPerFrame*nchans) + auxix*nchans + (TotalNeuralChans+TotalEMGChans+aux_chan) ] = samp;
					}
				}
				if (aux_chan == 1) {
					avgVunreg = avgVunreg / (double)FramesPerBlock;
					if (avgVunreg < 0.0) avgVunreg = 0.0;
					if (avgVunreg > 6.0) avgVunreg = 6.0;
					meta.avgVunreg = avgVunreg;
				}
			} else if (k.startsWith("TTL_")) {
				// don't grab all ttl chans here, since we only really support a subset of them
				QStringList knv = k.split("_");
				int ttl_chan = 0;
				bool ok = false;
				if (knv.count() == 2) {
					ttl_chan = knv.last().toInt(&ok);
					if (ttl_chan < 0 || ttl_chan >= TotalTTLChans) { ttl_chan = 0; ok = false; }
				} 
				if (!ok) Warning() << "Bug3: Internal problem -- parse error on TTL_ key.";
				if (params.bug.whichTTLs & (0x1<<ttl_chan)) {
                    int ttl_chan_translated = ttl_chan;
                    for (int i = 0; i < TotalTTLChans && i < ttl_chan; ++i)
                        // make the TTL line number -> our_channelid
                        // (because TTL10 from bug3 may not always be our TTL10 if say the ttl chans we have on are like 0,1,3,5,10,
                        // then TTL 10 is really 4 for us!
                        if (!(params.bug.whichTTLs & (0x1<<i)))
                            --ttl_chan_translated;
					bool warned = false;
					QStringList nums = v.split(",");
                    //if (excessiveDebug) Debug() << "Bug3: got " << nums.count() << " TTL samples..";
					for (int frame = 0; frame < FramesPerBlock; ++frame) {
						QString num = nums.empty() ? "0" : nums.front();
						if (nums.empty()) { 
							if (!warned) Warning() << "Bug3: Internal problem -- ran out of TTL samples in frame `" << v << "'";
							warned = true;
						}
						else nums.pop_front();
						bool ok = false;
						int samp = num.toUShort(&ok);
						if (!ok) Error() << "Bug3: Internal error -- parse error while reading emg sample `" << num << "'";
#if 0 // BUG3_TTL_TESTING
						// TODO HACK BUG FIXME TESTING XXX
						///*
						if (ttl_chan_translated == 0) {
							qint64 scan = qint64(blockNum)*qint64(SpikeGLScansPerBlock) + qint64(frame*NeuralSamplesPerFrame);
							const qint64 sr = params.srate;
							static const qint64 ttlWSamps = 640*2;
							static const qint64 everyNSecs = 2;
							
							if (scan>sr*everyNSecs) {
								if (scan>(debugTTLStart+sr*everyNSecs)) 
									debugTTLStart = scan;
							
								if (scan<debugTTLStart+ttlWSamps)
									samp = 1;
								else samp = 0;
							}
						}
						//*/
#endif
						if (samp) samp = 32767; // normalize high to maxV
						for (int ix = 0; ix < NeuralSamplesPerFrame; ++ix) { // need to produce 16 samples for each 1 emg sample read in order to match neuronal rate!						
                            samps[ frame*(NeuralSamplesPerFrame*nchans) + ix*nchans + (TotalNeuralChans+TotalEMGChans+TotalAuxChans+ttl_chan_translated) ] = samp;
						}						
					}				
				}
			} else if (k.startsWith("CHIPID")) {
				QStringList nums = v.split(",");
				bool warned = false;
				//if (excessiveDebug) Debug() << "Bug3: got " << nums.count() << " chipIDs..";
				for (int frame = 0; frame < FramesPerBlock; ++frame) {
					QString num = nums.empty() ? "0" : nums.front();
					if (nums.empty()) { 
						if (!warned) Warning() << "Bug3: Internal problem -- ran out of chipID data in frame `" << v << "'";
						warned = true;
					}
					else nums.pop_front();
					bool ok = false;
					quint16 chipid = num.toUShort(&ok);
					if (!ok) Error() << "Bug3: Internal error -- parse error while reading chipID `" << num << "'";
					meta.chipID[frame] = chipid;
				}
			} else if (k.startsWith("CHIP_FC")) {
				QStringList nums = v.split(",");
				bool warned = false;
				//if (excessiveDebug) Debug() << "Bug3: got " << nums.count() << " chip_fcs..";
				for (int frame = 0; frame < FramesPerBlock; ++frame) {
					QString num = nums.empty() ? "0" : nums.front();
					if (nums.empty()) { 
						if (!warned) Warning() << "Bug3: Internal problem -- ran out of chip_fc data in frame `" << v << "'";
						warned = true;
					}
					else nums.pop_front();
					bool ok = false;
					int chipfc = num.toInt(&ok);
					if (!ok) Error() << "Bug3: Internal error -- parse error while reading chip_fc `" << num << "'";
					meta.chipFrameCounter[frame] = chipfc;
				}
			} else if (k.startsWith("FRAME_MARKER_COR")) {
				QStringList nums = v.split(",");
				bool warned = false;
				//if (excessiveDebug) Debug() << "Bug3: got " << nums.count() << " FMCorr..";
				for (int frame = 0; frame < FramesPerBlock; ++frame) {
					QString num = nums.empty() ? "0" : nums.front();
					if (nums.empty()) { 
						if (!warned) Warning() << "Bug3: Internal problem -- ran out of framemarker_corr data in frame `" << v << "'";
						warned = true;
					}
					else nums.pop_front();
					bool ok = false;
					quint16 fmc  = num.toUShort(&ok);
					if (!ok) Error() << "Bug3: Internal error -- parse error while reading fmcorr `" << num << "'";
					meta.frameMarkerCorrelation[frame] = fmc;
				}
			} else if (k.startsWith("BOARD_FC")) {
				QStringList nums = v.split(",");
				bool warned = false;
				//if (excessiveDebug) Debug() << "Bug3: got " << nums.count() << " board_fc..";
				for (int frame = 0; frame < FramesPerBlock; ++frame) {
					QString num = nums.empty() ? "0" : nums.front();
					if (nums.empty()) { 
						if (!warned) Warning() << "Bug3: Internal problem -- ran out of board_fc data in frame `" << v << "'";
						warned = true;
					}
					else nums.pop_front();
					bool ok = false;
					int bfc  = num.toInt(&ok);
					if (!ok) Error() << "Bug3: Internal error -- parse error while reading board_fc `" << num << "'";
					meta.boardFrameCounter[frame] = bfc;
				}
			} else if (k.startsWith("BOARD_FRAME_TIMER")) {
				QStringList nums = v.split(",");
				bool warned = false;
				//if (excessiveDebug) Debug() << "Bug3: got " << nums.count() << " board_frame_timer..";
				for (int frame = 0; frame < FramesPerBlock; ++frame) {
					QString num = nums.empty() ? "0" : nums.front();
					if (nums.empty()) { 
						if (!warned) Warning() << "Bug3: Internal problem -- ran out of board_ftimer data in frame `" << v << "'";
						warned = true;
					}
					else nums.pop_front();
					bool ok = false;
					int bft  = num.toInt(&ok);
					if (!ok) Error() << "Bug3: Internal error -- parse error while reading board_ftimer `" << num << "'";
					meta.boardFrameTimer[frame] = bft;
				}
			} else if (k.startsWith("BER")) {
				bool ok = false;
				double n = v.toDouble(&ok);
				if (!ok) Warning() << "Bug3: Internal problem -- error parsing BER `" << v << "'";
				meta.BER = n;
			} else if (k.startsWith("WER")) {
				bool ok = false;
				double n = v.toDouble(&ok);
				if (!ok) Warning() << "Bug3: Internal problem -- error parsing WER `" << v << "'";
				meta.WER = n;				
			} else if (k.startsWith("MISSING_FC")) {
				bool ok = false;
				int n = v.toInt(&ok);
				if (!ok) Warning() << "Bug3: Internal problem -- error parsing MISSING_FC `" << v << "'";
				// TESTING HACK XXX TODO FIXME!
//				if (Util::random() < .3 && totalRead < 10000000ULL) n = int(Util::random(0.,16.)) % 6;
				meta.missingFrameCount = n;								
			} else if (k.startsWith("FALSE_FC")) {
				bool ok = false;
				int n = v.toInt(&ok);
				if (!ok) Warning() << "Bug3: Internal problem -- error parsing FALSE_FC `" << v << "'";
				// TESTING HACK XXX TODO FIXME!
//				if (Util::random() < .3 && totalRead < 10000000ULL) n = int(Util::random(0.,16.)) % 6;
				meta.falseFrameCount = n;								
			}
		}
		totalReadMut.lock();
		quint64 oldTotalRead = totalRead;
		totalRead += (quint64)samps.size(); 
		totalReadMut.unlock();
		//Debug() << "Enq: " << samps.size() << " samps, firstSamp: " << oldTotalRead;
		enqueueBuffer(samps, oldTotalRead, false, 0, QByteArray(reinterpret_cast<char *>(&meta),sizeof(meta)));
		if (!oldTotalRead) emit(gotFirstScan());
	}
		
	BugTask::BlockMetaData::BlockMetaData() { ::memset(this, 0, sizeof(*this)); /*zero out all data.. yay!*/ }
	BugTask::BlockMetaData::BlockMetaData(const BlockMetaData &o) { *this = o; }
	BugTask::BlockMetaData & BugTask::BlockMetaData::operator=(const BlockMetaData & o) { ::memcpy(this, &o, sizeof(o)); return *this; }
	
	void BugTask::setNotchFilter(bool enabled)
	{
		pushCmd(QString("SNF=%1\r\n").arg(enabled ? "1" : "0").toUtf8());
	}
	void BugTask::setHPFilter(int val) { ///<   <=0 == off, >0 = freq in Hz to high-pass filter
		if (val < 0) val = 0;
		pushCmd(QString("HPF=%1\r\n").arg(val).toUtf8());
	}
	
	/* static */ QString BugTask::getChannelName(unsigned num)
	{
		if (int(num) < TotalNeuralChans)
			return QString("NEU%1").arg(num);
		num -= TotalNeuralChans;
		if (int(num) < TotalEMGChans)
			return QString("EMG%1").arg(num);
		num -= TotalEMGChans;
		if (int(num) < TotalAuxChans)
			return QString("AUX%1").arg(num);
		num -= TotalAuxChans;
		if (int(num) < TotalTTLChans)
			return QString("TTL%1").arg(num);
		return QString("UNK%1").arg(num);
	}
	
	/* static */ bool BugTask::isNeuralChan(unsigned num) { return int(num) < TotalNeuralChans; }
	/* static */ bool BugTask::isEMGChan(unsigned num) { return int(num) >= TotalNeuralChans && int(num) < TotalNeuralChans+TotalEMGChans; }
	/* static */ bool BugTask::isAuxChan(unsigned num) { return int(num) >= TotalNeuralChans+TotalEMGChans && int(num) < TotalNeuralChans+TotalEMGChans+TotalAuxChans; }
	/* static */ bool BugTask::isTTLChan(unsigned num) { return int(num) >= TotalNeuralChans+TotalEMGChans+TotalAuxChans && int(num) < TotalNeuralChans+TotalEMGChans+TotalAuxChans+TotalTTLChans; }
	
	/*-------------------- Framegrabber Task --------------------------------*/
	
    /* static */ const double FGTask::SamplingRate = /*100;*/ 20000.0; // 20kHz sampling rate!
	unsigned FGTask::numChans() const { return params.nVAIChans; }
    unsigned FGTask::samplingRate() const { return params.srate; }
	
    FGTask::FGTask(const Params & ap, QObject *parent, bool isDummy)
    : SubprocessTask(ap, parent, "Framegrabber", "FG_SpikeGL.exe"/*"MFCApplication2.exe"*/)
	{
        dialogW = 0; dialog = 0;
        didImgSizeWarn = sentFGCmd = false;
        if (!isDummy) {
            dialogW = new QDialog(0,Qt::CustomizeWindowHint|Qt::Dialog|Qt::WindowTitleHint);
            dialog = new Ui::FG_Controls;
            dialog->setupUi(dialogW);
            Connect(dialog->calibAdcBut, SIGNAL(clicked()), this, SLOT(calibClicked()));
            Connect(dialog->setupRegsBut, SIGNAL(clicked()), this, SLOT(setupRegsClicked()));
            Connect(dialog->contAdcBut, SIGNAL(clicked()), this, SLOT(contAdcClicked()));
            Connect(dialog->grabFramesBut, SIGNAL(clicked()), this, SLOT(grabFramesClicked()));
            Connect(this, SIGNAL(gotMsg(QString,QColor)), this, SLOT(appendTE(QString,QColor)));
            Connect(this, SIGNAL(gotClkSignals(int)), this, SLOT(updateClkSignals(int)));
            Connect(this, SIGNAL(gotImg()), this, SLOT(updateImgXferCt()));
            Connect(this, SIGNAL(justStarted()), this, SLOT(setSaperaDevice()));
            Connect(this, SIGNAL(justStarted()), this, SLOT(openComPort()));
            dialogW->show();
            mainApp()->windowMenuAdd(dialogW);
        }
    }
    FGTask::~FGTask() {
        if (isRunning()) { stop(); wait(); }
        if (dialogW) mainApp()->windowMenuRemove(dialogW);
        if (dialog) delete dialog; dialog = 0;
        if (dialogW) delete dialogW; dialogW = 0;
    }

	QStringList FGTask::filesList() const 
	{
		QStringList files;
        files.push_back(QString(":/FG/FrameGrabber/FG_SpikeGL/x64/Release/") + exeName);
		files.push_back(":/FG/FrameGrabber/J_2000+_Electrode_8tap_8bit.ccf");
		return files;
	}
		
    void FGTask::sendExitCommand(QProcess & p) const
    {
        XtCmd xt;
        xt.init();
        xt.cmd = XtCmd_Exit;
        p.write((const char *)&xt, sizeof(xt));
    }

    void FGTask::openComPort()
    {
        const DAQ::Params & p(params);
        int parms[6];
        parms[0] = p.fg.com;
        parms[1] = p.fg.baud;
        parms[2] = p.fg.bits;
        parms[3] = p.fg.parity;
        parms[4] = p.fg.stop;
        parms[5] = 0;
        XtCmdOpenPort x;
        x.init(parms);
        pushCmd(x);
    }

    void FGTask::setSaperaDevice()
    {
        const DAQ::Params & p(params);
        XtCmdServerResource x;
        x.init("","",p.fg.sidx,p.fg.ridx,0,true);
        pushCmd(x);
    }

	unsigned FGTask::gotInput(const QByteArray & data, unsigned lastReadNBytes, QProcess & p) 
	{
		(void) lastReadNBytes;
		unsigned consumed = 0;
		
		std::vector<const XtCmd *> cmds;
		cmds.reserve(16384);
		const XtCmd *xt = 0;
		int cons = 0, nscans = 0;
		unsigned char *pdata = (unsigned char *)data.data();
		while ((xt = XtCmd::parseBuf(pdata+consumed, data.size()-consumed, cons))) {
			consumed += cons;
			cmds.push_back(xt);
			if (xt->cmd == XtCmd_Img) ++nscans;            
		}
		std::vector<int16> scans;
		scans.reserve(params.nVAIChans * nscans);
		for (std::vector<const XtCmd *>::iterator it = cmds.begin(); it != cmds.end(); ++it) {
			xt = *it;
			if (xt->cmd == XtCmd_Img) {
				const XtCmdImg *xi = (const XtCmdImg *)xt;
				//if (excessiveDebug) Debug() << "Got image of size " << xi->w << "x" << xi->h;
                const int imgSz = (xi->w/2)*xi->h;
                if (imgSz < (int)params.nVAIChans) {
                    emit(daqError("Received image frame from slave process but it's smaller than the number of channels we are expecting!"));
					p.kill();
					return 0;
                } else if (!didImgSizeWarn && imgSz > (int)params.nVAIChans) {
                    Warning() << "Received image of size:" << (xi->w/2) << "x" << xi->h << ", but expected an image of 72x32";
                    didImgSizeWarn = true;
                }
				scans.insert(scans.end(),(int16*)xi->img,((int16 *)xi->img)+params.nVAIChans);
                emit(gotImg()); // todo.. make this not get called for each frame since it may impact performance significantly
            } else if (xt->cmd == XtCmd_ConsoleMessage) {
				XtCmdConsoleMsg *xm = (XtCmdConsoleMsg *)xt; 
                QString msg(xm->msg);
				msg = msg.trimmed();
                QColor c = QColor(Qt::black);
				switch (xm->msgType) {
					case XtCmdConsoleMsg::Error:
                        c = QColor(Qt::red);
						Error() << shortName << ": " << msg; 
						emit(daqError(shortName + " slave process: " + msg));
						break;
					case XtCmdConsoleMsg::Warning:
                        c = QColor(Qt::magenta);
                        Warning() << shortName << ": " << msg; break;
					case XtCmdConsoleMsg::Debug:
                        c = QColor(Qt::blue);
                        Debug() << shortName << ": " << msg; break;
					default:
                        Log() << shortName << ": " << msg;
                        break;
				}
                emit gotMsg(msg,c);
            } else if (xt->cmd == XtCmd_ClkSignals) {
                emit(gotClkSignals(xt->param));
            } else if (xt->cmd == XtCmd_ServerResource) {
                XtCmdServerResource *xs = (XtCmdServerResource *)xt;
                Hardware h;
                h.serverName = xs->serverName;
                h.resourceName = xs->resourceName;
                h.serverIndex = xs->serverIndex;
                h.resourceIndex = xs->resourceIndex;
                h.serverType = xs->serverType;
                h.accessible = xs->accessible;
                probedHardware.push_back(h);
            } else {
				// todo.. handle other cmds coming in?
			}
		}
		if (scans.size()) {
			totalReadMut.lock();
			quint64 oldTotalRead = totalRead;
			totalRead += (quint64)scans.size();
			totalReadMut.unlock();
			enqueueBuffer(scans, oldTotalRead, true);
			if (!oldTotalRead) emit(gotFirstScan());
		}

		return consumed;
	}
	
    void FGTask::updateClkSignals(int param) {
        XtCmdClkSignals dummy; dummy.init(0,0,0,0,0); dummy.param = param;
        dialog->pxClk1Lbl->setText(dummy.isPxClk1() ? "<font color=green>+</font>" : "<font color=red>-</font>");
        dialog->pxClk2Lbl->setText(dummy.isPxClk2() ? "<font color=green>+</font>" : "<font color=red>-</font>");
        dialog->pxClk3Lbl->setText(dummy.isPxClk3() ? "<font color=green>+</font>" : "<font color=red>-</font>");
        dialog->hsyncLbl->setText(dummy.isHSync() ? "<font color=green>+</font>" : "<font color=red>-</font>");
        dialog->vsyncLbl->setText(dummy.isVSync() ? "<font color=green>+</font>" : "<font color=red>-</font>");
        unsigned long ctr = dialog->clkCtLbl->text().toULong();
        dialog->clkCtLbl->setText(QString::number(ctr+1UL));
    }

    void FGTask::updateImgXferCt()
    {
		// todo.. make this not get called for each frame since it may impact performance significantly
        unsigned long ctr = dialog->imgXferCtLbl->text().toULong();
        dialog->imgXferCtLbl->setText(QString::number(ctr+1UL));
    }

	/* static */
	QString FGTask::getChannelName(unsigned num)
	{ 
		return QString("AD %1").arg(num); 
	}
	
    void FGTask::pushCmd(const XtCmd * c)
    {
        if (!c) return;
        QByteArray b((char *)c, sizeof(*c) - sizeof(int) + c->len);
        SubprocessTask::pushCmd(b);
    }

    void FGTask::appendTE(const QString &s, const QColor &color)
    {
        QColor origColor = dialog->textEdit->textColor();
        dialog->textEdit->moveCursor(QTextCursor::End);
        dialog->textEdit->setTextColor(color);
        dialog->textEdit->append(s);
        dialog->textEdit->moveCursor(QTextCursor::End);
        dialog->textEdit->ensureCursorVisible();
        dialog->textEdit->setTextColor(origColor);
    }

    void FGTask::calibClicked() {
        appendTE("Calib clicked.", QColor(Qt::gray));
    }

    void FGTask::setupRegsClicked() {
        appendTE("Setup regs clicked.", QColor(Qt::gray));
    }

    void FGTask::contAdcClicked() {
        appendTE("Cont ADC clicked.", QColor(Qt::gray));
        XtCmdFPGAProto p;
        p.init(6,0,0);
        pushCmd(p);
    }

    void FGTask::grabFramesClicked() {
        appendTE("Grab frames clicked.", QColor(Qt::gray));
        XtCmd x;
        // grab frames.. does stuff with Sapera API in the slave process
        x.init();
        x.cmd = XtCmd_GrabFrames;
        pushCmd(x);
    }

    /* static */
    QList<FGTask::Hardware> FGTask::probedHardware;

    /* static */
    void FGTask::probeHardware() {
        double t0 = getTime();
        DAQ::Params dummy;
        FGTask task(dummy, 0, true);
        if (!task.platformSupported()) return;

        int wflags = Qt::Window|Qt::FramelessWindowHint|Qt::MSWindowsFixedSizeDialogHint|Qt::CustomizeWindowHint|Qt::WindowTitleHint|Qt::WindowStaysOnTopHint;
        QMessageBox mb(QMessageBox::Information, "Probing Hardware", "Probing framegrabber hardware, please wait...", QMessageBox::Abort, (QWidget *)(mainApp()->console()), (Qt::WindowFlags)wflags);
        QList<QAbstractButton *> buts = mb.buttons();
        foreach (QAbstractButton *b, buts) {
            mb.removeButton(b);
        }
        mb.setWindowFlags((Qt::WindowFlags)wflags);
        mb.show();
        mb.repaint();

        QString err;
        if (!task.setupExeDir(&err)) {
            Error() << "Failed to probe Sapera for active hardware: " << err;
            return;
        }
        QProcess p(0);
        p.setProcessChannelMode(task.usesMergedChannels() ? QProcess::MergedChannels : QProcess::SeparateChannels);
        p.setWorkingDirectory(task.exeDir);
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        task.setupEnv(env);
        p.setProcessEnvironment(env);
        p.start(task.exePath());
        if (p.state() == QProcess::NotRunning) {
            Error() << "Failed to probe Sapera for active hardware: slave process startup failed!";
            p.kill();
            return;
        }
        if (!p.waitForStarted(30000)) {
            Error() << "Failed to probe Sapera for active hardware: slave process startup timed out!";
            p.kill();
            return;
        }
        QByteArray ba;
        ba.resize(sizeof(XtCmdServerResource));
        XtCmdServerResource *x = (XtCmdServerResource *)ba.data();
        x->init("","",-1,-1,0,0);
        p.write(ba);
        int timeout = 10000;
        if (p.state() == QProcess::Running && p.waitForReadyRead(timeout)) {
            ba.clear();
            probedHardware.clear();
            do {
                ba += p.readAll();
                unsigned consumed = task.gotInput(ba, 0, p);
                ba.remove(0,consumed);
                if (!probedHardware.empty()) timeout = 500;
            } while (p.state() == QProcess::Running && p.waitForReadyRead(timeout));
            p.kill();
        } else {
            Error() << "Failed to probe Sapera for active hardware: slave process not running or waitForReadyRead() timed out!";
            p.kill();
            return;
        }
        task.sendExitCommand(p);
        p.waitForFinished(250);
        if (p.state() != QProcess::NotRunning) p.kill();

        Debug() << "Probe done, found " << probedHardware.size() << " valid AcqDevices in " << (getTime()-t0) << " secs.";
    }
	
} // end namespace DAQ

//-- #pragma mark Windows Hacks

///-- below is a bunch of ugly hacks for windows only to not have this .EXE depend on NI .DLLs!  

#if defined(Q_OS_WIN) && defined(HAVE_NIDAQmx)
#include <windows.h>

namespace DAQ {
    static HMODULE module = 0;
    
    bool Available(void) {
        static bool tried = false;
        //bool hadNoModule = !module;
        if (!module && !tried && !(module = LoadLibraryA("nicaiu.dll")) ) {
            //Log() << "Could not find nicaiu.dll, DAQ features disabled!";
            tried = true;
            return false;
        } else if (tried) return false;
        //if (hadNoModule)
        //    Log() << "Found and dynamically loaded NI Driver DLL: nicaiu.dll, DAQ features enabled";
        return true;
    }
    
    template <typename T> void tryLoadFunc(T * & func, const char *funcname) {
        if (!func && Available()) {
            func = reinterpret_cast<T *> (GetProcAddress( module, funcname ));
            if (!func) {
                //Warning() << "Could not find the function " << funcname << " in nicaiu.dll, NI functionality may fail!";
                return;                
            }
        }
    }
}

extern "C" {    
    //*** Set/Get functions for DAQmx_Dev_DO_Lines ***
    int32 __CFUNC DAQmxGetDevDOLines(const char device[], char *data, uInt32 bufferSize) {
        static int32 (__CFUNC *func)(const char *, char *, uInt32) = 0;
        DAQ::tryLoadFunc(func, "DAQmxGetDevDOLines");
        //Debug() << "DAQmxGetDevDOLines called";
        if (func) return func(device, data, bufferSize);
        return DAQmxErrorRequiredDependencyNotFound;
    }
    
    int32 __CFUNC DAQmxWriteDigitalScalarU32   (TaskHandle taskHandle, bool32 autoStart, float64 timeout, uInt32 value, bool32 *reserved) {
        static int32 (__CFUNC *func) (TaskHandle, bool32, float64, uInt32, bool32 *) = 0;
        DAQ::tryLoadFunc(func, "DAQmxWriteDigitalScalarU32");
        //Debug() << "DAQmxWriteDigitalScalarU32 called";
        if (func) return func(taskHandle, autoStart, timeout, value, reserved);
        return DAQmxErrorRequiredDependencyNotFound;
    }
    
    int32 __CFUNC  DAQmxStartTask (TaskHandle taskHandle) {
        static int32 (__CFUNC  *func) (TaskHandle) = 0;
        DAQ::tryLoadFunc(func, "DAQmxStartTask");
        Debug() << "DAQmxStartTask called";
        if (func) return func(taskHandle);
        return DAQmxErrorRequiredDependencyNotFound;
    }    
    
    int32 __CFUNC  DAQmxStopTask (TaskHandle taskHandle) {
        static int32 (__CFUNC  *func) (TaskHandle) = 0;
        DAQ::tryLoadFunc(func, "DAQmxStopTask");
        Debug() << "DAQmxStopTask called";
        if (func) return func(taskHandle);
        return DAQmxErrorRequiredDependencyNotFound;
    }
    
    int32 __CFUNC  DAQmxClearTask (TaskHandle taskHandle) {
        static int32 (__CFUNC  *func) (TaskHandle) = 0;
        DAQ::tryLoadFunc(func, "DAQmxClearTask");
        //Debug() << "DAQmxClearTask called";
        if (func) return func(taskHandle);
        return DAQmxErrorRequiredDependencyNotFound;
    }
    
    int32 __CFUNC DAQmxCreateDOChan (TaskHandle taskHandle, const char lines[], const char nameToAssignToLines[], int32 lineGrouping) {
        static int32 (__CFUNC *func)(TaskHandle, const char *, const char *, int32 lineGrouping) = 0;
        DAQ::tryLoadFunc(func, "DAQmxCreateDOChan");
        //Debug() << "DAQmxCreateDOChan called";
        if (func) return func(taskHandle,lines,nameToAssignToLines,lineGrouping);
        return DAQmxErrorRequiredDependencyNotFound;
    }
    
    int32 __CFUNC     DAQmxGetExtendedErrorInfo (char errorString[], uInt32 bufferSize) {
        static int32 (__CFUNC *func) (char *, uInt32) = 0;
        DAQ::tryLoadFunc(func, "DAQmxGetExtendedErrorInfo");
        //Debug() << "DAQmxGetExtendedErrorInfo called";
        if (func) return func(errorString,bufferSize);
        strncpy(errorString, "DLL Missing", bufferSize);
        return DAQmxSuccess;        
    }
    
    int32 __CFUNC     DAQmxCreateTask          (const char taskName[], TaskHandle *taskHandle) {
        static int32 (__CFUNC *func) (const char *, TaskHandle *) = 0;
        static const char * const fname = "DAQmxCreateTask";
        DAQ::tryLoadFunc(func, fname);
        //Debug() << fname << " called";
        if (func) return func(taskName,taskHandle);
        return DAQmxErrorRequiredDependencyNotFound;                
    }
    
    int32 __CFUNC DAQmxGetDevAIMaxMultiChanRate(const char device[], float64 *data) {
        static int32 (__CFUNC *func)(const char *, float64 *) = 0;
        static const char * const fname = "DAQmxGetDevAIMaxMultiChanRate";
        DAQ::tryLoadFunc(func, fname);
        //Debug() << fname << " called";
        if (func) return func(device,data);
        return DAQmxErrorRequiredDependencyNotFound;                
    }
    
    int32 __CFUNC DAQmxGetDevAISimultaneousSamplingSupported(const char device[], bool32 *data) {
        static int32 (__CFUNC *func)(const char *, bool32 *)      = 0;
        const char *fname = "DAQmxGetDevAISimultaneousSamplingSupported";
        DAQ::tryLoadFunc(func, fname);
        //Debug() << fname << " called";
        if (func) return func(device,data);
        return DAQmxErrorRequiredDependencyNotFound;                        
    }
    
    int32 __CFUNC DAQmxGetDevAIMaxSingleChanRate(const char device[], float64 *data) {
        static int32 (__CFUNC *func)(const char *, float64 *)      = 0;
        const char *fname = "DAQmxGetDevAIMaxSingleChanRate";
        DAQ::tryLoadFunc(func, fname);
        //Debug() << fname << " called";
        if (func) return func(device,data);
        return DAQmxErrorRequiredDependencyNotFound;                        
    }
    
    int32 __CFUNC DAQmxGetDevAIPhysicalChans(const char device[], char *data, uInt32 bufferSize) {
        static int32 (__CFUNC *func)(const char *, char *, uInt32)      = 0;
        const char *fname = "DAQmxGetDevAIPhysicalChans";
        DAQ::tryLoadFunc(func, fname);
        //Debug() << fname << " called";
        if (func) return func(device,data,bufferSize);
        return DAQmxErrorRequiredDependencyNotFound;                                
    }
    
    int32 __CFUNC DAQmxGetDevAOVoltageRngs(const char device[], float64 *data, uInt32 arraySizeInSamples) {
        static int32 (__CFUNC *func)(const char *, float64 *, uInt32)      = 0;
        const char *fname = "DAQmxGetDevAOVoltageRngs";
        DAQ::tryLoadFunc(func, fname);
        //Debug() << fname << " called";
        if (func) return func(device,data,arraySizeInSamples);
        return DAQmxErrorRequiredDependencyNotFound;                                        
    }
    
    int32 __CFUNC DAQmxGetDevAIVoltageRngs(const char device[], float64 *data, uInt32 arraySizeInSamples) {
        static int32 (__CFUNC *func)(const char *, float64 *, uInt32)      = 0;
        const char *fname = "DAQmxGetDevAIVoltageRngs";
        DAQ::tryLoadFunc(func, fname);
        //Debug() << fname << " called";
        if (func) return func(device,data,arraySizeInSamples);
        return DAQmxErrorRequiredDependencyNotFound;                                                
    }
    
    int32 __CFUNC DAQmxGetDevAIMinRate(const char device[], float64 *data) {
        static int32 (__CFUNC *func)(const char *, float64 *)      = 0;
        const char *fname = "DAQmxGetDevAIMinRate";
        DAQ::tryLoadFunc(func, fname);
        //Debug() << fname << " called";
        if (func) return func(device,data);
        return DAQmxErrorRequiredDependencyNotFound;                                                        
    }
    
    int32 __CFUNC DAQmxGetDevAOPhysicalChans(const char device[], char *data, uInt32 bufferSize) {
        static int32 (__CFUNC *func)(const char *, char  *, uInt32)      = 0;
        const char *fname = "DAQmxGetDevAOPhysicalChans";
        DAQ::tryLoadFunc(func, fname);
        //Debug() << fname << " called";
        if (func) return func(device,data,bufferSize);
        return DAQmxErrorRequiredDependencyNotFound;                                                                
    }
    
    int32 __CFUNC DAQmxGetDevProductType(const char device[], char *data, uInt32 bufferSize) {
        static int32 (__CFUNC *func)(const char *, char  *, uInt32)      = 0;
        const char *fname = "DAQmxGetDevProductType";
        DAQ::tryLoadFunc(func, fname);
        //Debug() << fname << " called";
        if (func) return func(device,data,bufferSize);
        return DAQmxErrorRequiredDependencyNotFound;                                                                        
    }
    
    int32 __CFUNC DAQmxCfgInputBuffer(TaskHandle taskHandle, uInt32 numSampsPerChan) {
        static int32 (__CFUNC *func)(TaskHandle, uInt32)      = 0;
        const char *fname = "DAQmxCfgInputBuffer";
        DAQ::tryLoadFunc(func, fname);
        //Debug() << fname << " called";
        if (func) return func(taskHandle,numSampsPerChan);
        return DAQmxErrorRequiredDependencyNotFound;        
    }
    
    int32 __CFUNC DAQmxCfgOutputBuffer(TaskHandle taskHandle, uInt32 numSampsPerChan) {
    static int32 (__CFUNC *func)(TaskHandle, uInt32)      = 0;
    const char *fname = "DAQmxCfgOutputBuffer";
    DAQ::tryLoadFunc(func, fname);
    //Debug() << fname << " called";
    if (func) return func(taskHandle,numSampsPerChan);
        return DAQmxErrorRequiredDependencyNotFound;        
    }
    
    int32 __CFUNC DAQmxCfgSampClkTiming(TaskHandle taskHandle, const char source[], float64 rate, int32 activeEdge, int32 sampleMode, uInt64 sampsPerChan) {
        static int32 (__CFUNC *func)(TaskHandle, const char *, float64, int32, int32, uInt64)      = 0;
        const char *fname = "DAQmxCfgSampClkTiming";
        DAQ::tryLoadFunc(func, fname);
        //Debug() << fname << " called";
        if (func) return func(taskHandle,source,rate,activeEdge,sampleMode,sampsPerChan);
        return DAQmxErrorRequiredDependencyNotFound;                
    }

    int32 __CFUNC DAQmxCreateAIVoltageChan(TaskHandle taskHandle, const char physicalChannel[], const char nameToAssignToChannel[], int32 terminalConfig, float64 minVal, float64 maxVal, int32 units, const char customScaleName[]) {
        static int32 (__CFUNC *func)(TaskHandle, const char *, const char *, int32, float64, float64, int32, const char *)      = 0;
        const char *fname = "DAQmxCreateAIVoltageChan";
        DAQ::tryLoadFunc(func, fname);
        //Debug() << fname << " called";
        if (func) return func(taskHandle,physicalChannel,nameToAssignToChannel,terminalConfig,minVal,maxVal,units,customScaleName);
        return DAQmxErrorRequiredDependencyNotFound;                
    }        

    int32 __CFUNC DAQmxCreateAOVoltageChan(TaskHandle taskHandle, const char physicalChannel[], const char nameToAssignToChannel[], float64 minVal, float64 maxVal, int32 units, const char customScaleName[]) {
        static int32 (__CFUNC *func)(TaskHandle, const char *, const char *, float64, float64, int32, const char *)      = 0;
        const char *fname = "DAQmxCreateAOVoltageChan";
        DAQ::tryLoadFunc(func, fname);
        //Debug() << fname << " called";
        if (func) return func(taskHandle,physicalChannel,nameToAssignToChannel,minVal,maxVal,units,customScaleName);
        return DAQmxErrorRequiredDependencyNotFound;                
    }        

    int32 __CFUNC DAQmxReadBinaryI16(TaskHandle taskHandle, int32 numSampsPerChan, float64 timeout, bool32 fillMode, int16 readArray[], uInt32 arraySizeInSamps, int32 *sampsPerChanRead, bool32 *reserved) {
        static int32 (__CFUNC *func)(TaskHandle, int32, float64, bool32, int16 *, uInt32, int32 *, bool32 *)      = 0;
        const char *fname = "DAQmxReadBinaryI16";
        DAQ::tryLoadFunc(func, fname);
        //Debug() << fname << " called";
        if (func) return func(taskHandle,numSampsPerChan,timeout,fillMode,readArray,arraySizeInSamps,sampsPerChanRead,reserved);
        return DAQmxErrorRequiredDependencyNotFound;                
    }

    int32 __CFUNC DAQmxWriteBinaryI16(TaskHandle taskHandle, int32 numSampsPerChan, bool32 autoStart, float64 timeout, bool32 dataLayout, const int16 writeArray[], int32 *sampsPerChanWritten, bool32 *reserved) {
        static int32 (__CFUNC *func)(TaskHandle, int32, bool32, float64, bool32, const int16 *, int32 *, bool32 *)      = 0;
        const char *fname = "DAQmxWriteBinaryI16";
        DAQ::tryLoadFunc(func, fname);
        //Debug() << fname << " called";
        if (func) return func(taskHandle,numSampsPerChan,autoStart,timeout,dataLayout,writeArray,sampsPerChanWritten,reserved);
        return DAQmxErrorRequiredDependencyNotFound;                
    }
    
    int32 __CFUNC DAQmxRegisterEveryNSamplesEvent(TaskHandle taskHandle, int32 everyNsamplesEventType, uInt32 nSamples, uInt32 options, DAQmxEveryNSamplesEventCallbackPtr callbackFunction, void *callbackData) {
        static int32 (__CFUNC *func)(TaskHandle, int32, uInt32, uInt32, DAQmxEveryNSamplesEventCallbackPtr, void *)      = 0;
        const char *fname = "DAQmxRegisterEveryNSamplesEvent";
        DAQ::tryLoadFunc(func, fname);
        //Debug() << fname << " called";
        if (func) return func(taskHandle,everyNsamplesEventType,nSamples,options,callbackFunction,callbackData);
        return DAQmxErrorRequiredDependencyNotFound;        
    }


    /* Implementation Checklist for DLL decoupling:
             √ DAQmxCfgInputBuffer
             √ DAQmxCfgOutputBuffer
             √ DAQmxCfgSampClkTiming
             √ DAQmxClearTask
             √ DAQmxCreateAIVoltageChan
             √ DAQmxCreateAOVoltageChan
             √ DAQmxCreateDOChan
             √ DAQmxCreateTask
             √ DAQmxErrChk
             √ DAQmxErrorRequiredDependencyNotFound
             √ DAQmxFailed
             √ DAQmxGetDevAIMaxMultiChanRate
             √ DAQmxGetDevAIMaxSingleChanRate
             √ DAQmxGetDevAIMinRate
             √ DAQmxGetDevAIPhysicalChans
             √ DAQmxGetDevAISimultaneousSamplingSupported
             √ DAQmxGetDevAIVoltageRngs
             √ DAQmxGetDevAOPhysicalChans
             √ DAQmxGetDevAOVoltageRngs
             √ DAQmxGetDevDOLines
             √ DAQmxGetDevProductType
             √ DAQmxGetExtendedErrorInfo
             √ DAQmxReadBinaryI16
             √ DAQmxRegisterEveryNSamplesEvent
             √ DAQmxStartTask
             √ DAQmxStopTask
             √ DAQmxSuccess
             √ DAQmxWriteBinaryI16
             √ DAQmxWriteDigitalScalarU32
        */
    
}
#else
namespace DAQ {
    bool Available() { return true; /* emulated, but available! */ }
}
#endif
