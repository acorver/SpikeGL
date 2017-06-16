#include "DataFile.h"
#include "Util.h"
#include "SpikeGL.h"
#include "MainApp.h"
#include <QFileInfo.h>
#include <memory>
#include "ConfigureDialogController.h"
#include "ChanMappingController.h"
#include <QThread>
#include "SampleBufQ.h"
#include <QMessageBox>
#include <QTextStream>
#include <QMutexLocker>

class DFWriteThread : public QThread, public SampleBufQ
{
	DataFile *d;
	volatile bool stopflg;

public:
    DFWriteThread(DataFile *df, unsigned q_size);
	~DFWriteThread();
protected:
	void run(); ///< from QThread
	
	bool write(const std::vector<int16> & scans);
};


static QString metaFileForFileName(const QString &fname)
{
    return baseName(fname) + ".meta";
}

/* static */ bool DataFile::verifySHA1(const QString & filename)
{
    SHA1 sha;
    Params p;

    if (!p.fromFile(metaFileForFileName(filename))) { 
        Error() << "verifySHA1 could not read/parse/find meta file for " << filename;
        return false;
    }
    
    if (!sha.HashFile(filename.toLatin1().constData())) {
        Error() << "verifySHA1 could not read file: " << filename;
        return false;
    }
    QString hash = sha.ReportHash().c_str();
    QString hashSaved = p["sha1"].toString().trimmed();
    if (hashSaved.length() != 40) {
        Error() << "Meta file hash is not existant or not of the correct format `" << hashSaved << "'!";
        return false;
    }
    hash = hash.trimmed();
    return hash.compare(hashSaved, Qt::CaseInsensitive) == 0;
 }
 
DataFile::DataFile()
    : mut(QMutex::Recursive), mode(Undefined), scanCt(0), nChans(0), sRate(0), writeRateAvg_for_ui(0), writeRateAvg(0.), nWritesAvg(0), nWritesAvgMax(1), dfwt(0)
{
}

DataFile::~DataFile() {
	if (dfwt) delete dfwt, dfwt = 0;
}

bool DataFile::closeAndFinalize() 
{
    QMutexLocker ml(&mut);

    if (!isOpen()) return false;
	if (mode == Input) {
		dataFile.close();
		metaFile.close();
		nChans = scanCt = sRate = 0;
		mode = Undefined;
		return true;
	} else if (mode == Output) {
		if (dfwt) delete dfwt, dfwt = 0;
		// Output mode...
		sha.Final();
        params["sha1"] = /*sha.ReportHash().c_str()*/ "0";
		params["fileTimeSecs"] = fileTimeSecs();
		params["fileSizeBytes"] = dataFile.size();
		params["createdBy"] = QString("%1").arg(VERSION_STR);
        if (badData.count()) {
            QString bdString;
            QTextStream ts(&bdString);
            int i = 0;
            for (BadData::iterator it = badData.begin(); it != badData.end(); ++it, ++i) {
                if (i) { ts << "; "; }
                ts << (*it).first << "," << (*it).second;
            }
            ts.flush();
            params["badData"] = bdString;
        }
        Debug() << fileName() << " closing after saving " << scanCount() << " scans @ "  << (writeSpeedBytesSec()/1024.0/1024.0) << " MB/s avg";
		dataFile.close();
		QString mf = metaFile.fileName();
		metaFile.close(); // close it.. we mostly reserved it in the FS.. however we did write to it if writeCommentToMetaFile() as called, otherwise we just reserved it on the FS    
        writeRateAvg_for_ui = writeRateAvg = 0.;
		nWritesAvg = nWritesAvgMax = 0;
		mode = Undefined;
		return params.toFile(mf,true /* append since we may have written comments to metafile!*/);
	} 
	return false; // not normally reached...
}

bool DataFile::writeScans(const int16 *scans, unsigned nScans)
{
    QMutexLocker ml(&mut);

    if (!isOpen()) return false;
    if (!nScans) return true; // for now, we allow empty writes!
    if (scanCt == 0) {
        // special case -- Leonardo lab requested that timestamp on data files be the timestamp of when first scan arrived
        // so, to fudge this we need to close the data file, delete it, and quickly reopen it
        // the reason we had it open in the first place was to 'reserve' that spot on the disk ;)
        QString fileName(dataFile.fileName());

        dataFile.remove(); /// remove the 0-byte file
        dataFile.setFileName(fileName);
        if (!dataFile.open(QIODevice::WriteOnly|QIODevice::Truncate)) { // reopen it to reset the timestamp
            Error() << "Failed to open data file " << fileName << " for write!";
            return false;
        }
    }

    scanCt += nScans;
    // synchronous write..
    return doFileWrite(scans, nScans);
}

bool DataFile::writeScans(const std::vector<int16> & scans, bool asynch, unsigned threaded_queueSize)
{
    QMutexLocker ml(&mut);
    if (!isOpen()) return false;
    if (!scans.size()) return true; // for now, we allow empty writes!
    if (scans.size() % nChans) {
        Error() << "writeScan: Scan needs to be of size a multiple of " << nChans << " chans long (dataFile: " << QFileInfo(dataFile.fileName()).baseName() << ")";
        return false;
    }
	if (scanCt == 0) {
		// special case -- Leonardo lab requested that timestamp on data files be the timestamp of when first scan arrived
		// so, to fudge this we need to close the data file, delete it, and quickly reopen it
		// the reason we had it open in the first place was to 'reserve' that spot on the disk ;)
		QString fileName(dataFile.fileName());
		
		dataFile.remove(); /// remove the 0-byte file
		dataFile.setFileName(fileName);
		if (!dataFile.open(QIODevice::WriteOnly|QIODevice::Truncate)) { // reopen it to reset the timestamp
			Error() << "Failed to open data file " << fileName << " for write!";
			return false;				
		}
	}
	
	scanCt += scans.size()/nChans;

    if (asynch) {
        (void)threaded_queueSize;
        Error() << "INTERNAL ERROR: DataFile `asynch'' writeScans mode is no longer supported! Fix the caller!";
        return false;
        /*if (!dfwt) {
            if (threaded_queueSize == 0) threaded_queueSize = SAMPLE_BUF_Q_SIZE;
            dfwt = new DFWriteThread(this, threaded_queueSize);
			dfwt->start();
		}
		std::vector<int16> scansCopy;
		scansCopy.reserve(scans.size());
		scansCopy.insert(scansCopy.begin(), scans.begin(), scans.end());
		dfwt->enqueueBuffer(scansCopy, 0);
        return dfwt->dataQueueSize() < dfwt->dataQueueMaxSize;*/
	} else if (dfwt) { // !asynch but we have a dfwt!! wtf?!
		Error() << "INTERNAL: Previous call to DataFile::writeScans() was asynch, now we are synch! This is unsupported! FIXME!";
		delete dfwt;
		dfwt = 0;
	}
	// else .. synch..
	return doFileWrite(scans);
}

void DataFile::writeCommentToMetaFile(const QString & cmt, bool prepend)
{
	metaFile.write(QString("%1%2%3").arg(prepend?QString("# "):QString("")).arg(cmt).arg(cmt.endsWith("\n")?QString(""):QString("\n")).toUtf8());
}

bool DataFile::doFileWrite(const std::vector<int16> & scans)
{
    if (!numChans()) return false;
    return doFileWrite(&scans[0], unsigned(scans.size()/numChans()));
}

bool DataFile::doFileWrite(const int16 *scans, unsigned nScans)
{    
    const int n2Write = nScans*numChans()*sizeof(int16);
	
	double tWrite = getTime();
	
    int nWrit = dataFile.write((const char *)scans, n2Write);

	if (nWrit != n2Write) {
		Error() << "DataFile::doFileWrite: Error returned from write call: " << nWrit;
		return false;
	}

    const double tEndWrite = getTime();

    tWrite = tEndWrite - tWrite;
    //XXX debug todo fixme
    //qDebug("Wrote %d bytes in %f ms",n2Write,tWrite*1e3);

    // Sha1 Realtime has update disabled as of 2/9/2016 -- this was causing massive slowdowns on
    // file saving of large data files.  Will revisit this later.  But noone was using the sha1
    // hash's anyway.  Right now the Sha1 Verify... popup will warn if the sha1 hash is 0,
    // and offer the user the opportunity to recompute the hash.s
    //sha.UpdateHash((const uint8_t *)&scans[0], n2Write);

	// update write speed..
	writeRateAvg = (writeRateAvg*nWritesAvg+(n2Write/tWrite))/double(nWritesAvg+1);
	if (++nWritesAvg > nWritesAvgMax) nWritesAvg = nWritesAvgMax;

    writeRateAvg_for_ui = qRound(writeRateAvg);
	
    //XXX debug todo fixme
    //qDebug("Update hash + stats took %f ms",(getTime()-tEndWrite)*1e3);

	return true;
}

// not threadsafe
bool DataFile::openForReWrite(const DataFile & other, const QString & filename, const QVector<unsigned> & chanNumSubset)
{
	if (!other.isOpenForRead()) {
		Error() << "INTERNAL ERROR: First parameter to DataFile::openForReWrite() needs to be another DataFile that is opened for reading.";
		return false;
	}
	if (isOpen()) closeAndFinalize();
	
	QString outputFile (filename);
	if (!QFileInfo(outputFile).isAbsolute())
        outputFile = mainApp()->outputDirectory() + "/" + outputFile; 
    
    Debug() << "outdir: " << mainApp()->outputDirectory() << " outfile: " << outputFile;
    
	dataFile.close();  metaFile.close();
    dataFile.setFileName(outputFile);
    metaFile.setFileName(metaFileForFileName(outputFile));
	
    if (!dataFile.open(QIODevice::WriteOnly|QIODevice::Truncate) ||
        !metaFile.open(QIODevice::WriteOnly|QIODevice::Truncate)) {
        Error() << "Failed to open either one or both of the data and meta files for " << outputFile;
        return false;
    }
	
//    badData = other.badData;
    badData.clear();
	mode = Output;
	const int nOnChans = chanNumSubset.size();
	params = other.params;
	params["outputFile"] = outputFile;
    params.remove("badData"); // rebuild this as we write!
	scanCt = 0;
	nChans = nOnChans;
	sha.Reset();
	sRate = other.sRate;
	range = other.range;
    writeRateAvg = 0.;
    nWritesAvg = 0;
    nWritesAvgMax = /*unsigned(sRate/10.)*/10;
    if (!nWritesAvgMax) nWritesAvgMax = 1;
	// compute save channel subset fudge
	const QVector<unsigned> ocid = other.channelIDs();
	chanIds.clear();
	customRanges.clear();
	chanDisplayNames.clear();
	QString crStr(""), cdnStr("");
	foreach (unsigned i, chanNumSubset) {
		if (i < unsigned(ocid.size())) {
			chanIds.push_back(ocid[i]);
			if (i < (unsigned)other.customRanges.size()) customRanges.push_back(other.customRanges[i]);
			else customRanges.push_back(range);
			if (i < (unsigned)other.chanDisplayNames.size()) chanDisplayNames.push_back(other.chanDisplayNames[i]);
			else chanDisplayNames.push_back(QString("Ch ") + QString::number(i));
			crStr.append(QString("%3%1:%2").arg(customRanges.back().min,0,'f',9).arg(customRanges.back().max,0,'f',9).arg(crStr.length() ? "," : ""));
			if (cdnStr.length()) cdnStr.append(",");
			cdnStr.append(QString(chanDisplayNames.back()).replace(",",""));
		} else 
			Error() << "INTERNAL ERROR: The chanNumSubset passet to DataFile::openForRead must be a subset of channel numbers (indices, not IDs) to use in the rewrite.";
	}
	params["saveChannelSubset"] = ConfigureDialogController::generateAIChanString(chanIds);
	params["nChans"] = nChans;
	if (params.contains("chanDisplayNames")) params["chanDisplayNames"] = cdnStr;
	if (params.contains("customRanges")) params["customRanges"] = crStr;
	pd_chanId = other.pd_chanId;
	
	return true;
}

/// threadsafe
bool DataFile::openForWrite(const DAQ::Params & dp, const QString & filename_override) 
{
    QMutexLocker ml(&mut);

	const int nOnChans = dp.demuxedBitMap.count(true);
    if (!dp.aiChannels.size() || !nOnChans) {
        Error() << "DataFile::openForWrite Error cannot open a datafile with scansize of 0!";
        return false;
    }
    if (isOpen()) closeAndFinalize();

    QString outputFile = filename_override.length() ? filename_override : dp.outputFile;

    if (!QFileInfo(outputFile).isAbsolute())
        outputFile = mainApp()->outputDirectory() + "/" + outputFile; 
    
    Debug() << "outdir: " << mainApp()->outputDirectory() << " outfile: " << outputFile;
    
	dataFile.close();  metaFile.close();
    dataFile.setFileName(outputFile);
    metaFile.setFileName(metaFileForFileName(outputFile));

    if (!dataFile.open(QIODevice::WriteOnly|QIODevice::Truncate) ||
        !metaFile.open(QIODevice::WriteOnly|QIODevice::Truncate)) {
        Error() << "Failed to open either one or both of the data and meta files for " << outputFile;
        return false;
    }
    sha.Reset();
    params = Params();
    badData.clear();
    scanCt = 0;
    nChans = nOnChans;
    sRate = dp.srate;
    writeRateAvg = 0.;
    nWritesAvg = 0;
    nWritesAvgMax = /*unsigned(sRate/10.)*/10;
    if (!nWritesAvgMax) nWritesAvgMax = 1;
    params["outputFile"] = outputFile;
	params["createdOn"] = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
	params["dev"] = dp.dev;
	if (dp.dualDevMode) {
		params["dev2"] = dp.dev2;
		params["dualDevMode"] = true;
		params["secondDevIsAuxOnly"] = dp.secondDevIsAuxOnly;
	}
    params["devProductName"] = DAQ::GetProductName(dp.dev);
    params["nChans"] = nChans;
    params["sRateHz"] = sRate;
	range = dp.range;
	customRanges.clear();
	if (dp.customRanges.size()) {
		QString s = "";
		DAQ::Range r(1e9,-1e9);
		for (int i = 0; i < dp.customRanges.size(); ++i) {
			if (dp.demuxedBitMap[i]) {
				if (s.length()) s.append(",");
				const DAQ::Range & cr(dp.customRanges[i]);
				customRanges.push_back(cr);
				if (r.min > cr.min) r.min = cr.min;
				if (r.max < cr.max) r.max = cr.max;
				s.append(QString("%1:%2").arg(cr.min,0,'f',9).arg(cr.max,0,'f',9));
			}
		}
		params["customRanges"] = s;
		params["rangeMin"] = r.min;
		params["rangeMax"] = r.max;
	} else {
		params["rangeMin"] = range.min;
		params["rangeMax"] = range.max;
	}
    params["acqMode"] = DAQ::ModeToString(dp.mode);
    params["extClock"] = dp.extClock;
    params["aiString"] = dp.aiString;
    params["fastSettleTimeMS"] = dp.fastSettleTimeMS;
    params["auxGain"] = dp.auxGain;
    params["termination"] = DAQ::TermConfigToString(dp.aiTerm);
//	params["channelMapping2"] = dp.chanMap.toString();
	params["channelMapping2Terse"] = dp.chanMap.toTerseString(dp.demuxedBitMap);
	
    if (dp.usePD) {
        params["pdChan"] = dp.pdChan;
        params["pdThresh"] = dp.pdThresh;
		params["pdChanIsVirtual"] = dp.pdChanIsVirtual;
		params["pdThreshW"] = dp.pdThreshW;
		params["pdPassThruToAO"] = dp.pdPassThruToAO;
		params["pdStopTime"] = dp.pdStopTime;
		params["silenceBeforePD"] = dp.silenceBeforePD;
    }
    if (dp.bug.enabled) {
        params["bug_errorTolerance"] = dp.bug.errTol;
        params["bug_hpfilt"] = dp.bug.hpf;
        params["bug_snfilt"] = dp.bug.snf;
        params["bug_clockEdge"] = dp.bug.clockEdge;
        params["bug_dataRate"] = dp.bug.rate;
    }
    params["acqStartEndMode"] = DAQ::AcqStartEndModeToString(dp.acqStartEndMode);
    if (dp.demuxedBitMap.count(false)) {
        params["saveChannelSubset"] = dp.subsetString;
    } else 
        params["saveChannelSubset"] = "ALL";
	mode = Output;
	
	chanDisplayNames.clear();
	if (dp.chanDisplayNames.size()) {
		QString str;
		int i = 0;
		for (QVector<QString>::const_iterator it = dp.chanDisplayNames.begin(); it < dp.chanDisplayNames.end(); ++it, ++i) {
			if (dp.demuxedBitMap[i]) {
				QString s(*it);
				s.replace(",", "");
				str = str + (str.length() ? "," : "") + s.trimmed();
				chanDisplayNames.push_back(*it);
			}
		}
		params["chanDisplayNames"] = str;
	}
	
    return true;
}

/// param management -- not threadsafe
void DataFile::setParam(const QString & name, const QVariant & value)
{
    params[name] = value;
}
/// param management -- not threadsafe
const QVariant & DataFile::getParam(const QString & name) const
{
    Params::const_iterator it = params.find(name);
    if (it != params.end()) return it.value();
    static QVariant invalid;
    return invalid;
}

/* static */ bool DataFile::isValidInputFile(const QString & filename_in, QString * error)
{	
	QString filename(filename_in);
	QFileInfo fi(filename);
	if (fi.suffix() == ".meta") { // oops! they specified .meta!  Drop the .meta and assume .bin!
		filename = fi.canonicalPath() + "/" + fi.completeBaseName() + ".bin";
		fi.setFile(filename);
	}
	if (!fi.exists()) { if (error) *error = "File does not exist."; return false; }
	fi.setFile(metaFileForFileName(filename));
	if (!fi.exists()) { if (error) *error = QString("The meta file (") + fi.fileName() + ") does not exist."; return false; }

	QFile bin(filename);
	if (!bin.open(QIODevice::ReadOnly)) {
		if (error) *error = bin.fileName() + " cannot be opened for reading.";
		return false;
	}
	QFile meta(fi.canonicalFilePath());
	if (!meta.open(QIODevice::ReadOnly)) {
		if (error) *error = QString("The meta file (") + meta.fileName() + ") cannot be opened for reading.";
		return false;
	}
	bin.close(); meta.close();
	
    Params p;
	
    if (!p.fromFile(metaFileForFileName(filename))) { 
        if (error) *error = "The meta file for " + filename + " is corrupt.";
        return false;
	}
	
	if (!p.contains("fileSizeBytes") 
		|| !p.contains("nChans")
		|| !p.contains("sRateHz")) {
		if (error) *error = "The .meta file is missing a required key.";
		return false;
	}

	if (p["fileSizeBytes"].toLongLong() != bin.size()) {
		if(error) *error = "The .bin file does not seem to match the size expected from the .meta file.";
		return false;
	}
	if (error) *error = "";
	return true;
}

/// not threadsafe
bool DataFile::openForRead(const QString & file_in) 
{
	QString file(file_in);
	if (file.endsWith(".meta")) { // oops! they specified .meta!  Drop the .meta and assume .bin!
		file = file.left(file.length() - QString(".meta").length()) + ".bin";
	}	
	QString error;
	if (!isValidInputFile(file, &error)) {
		Error() << "Cannot open data file: " << error;
		return false;
	}
	dataFile.close();  metaFile.close();
	dataFile.setFileName(file);
    metaFile.setFileName(metaFileForFileName(file));	
    if (!dataFile.open(QIODevice::ReadOnly) || !metaFile.open(QIODevice::ReadOnly)) {
        Error() << "Failed to open either one or both of the data and meta files for " << QFileInfo(file).fileName();
        return false;
    }
	params.clear();
	params.fromFile(metaFile.fileName());
	qint64 fsize = params["fileSizeBytes"].toLongLong();
	if (fsize != dataFile.size()) {
		Warning() << ".bin file size mismatches .meta file's recorded size!  .Bin file may be corrupt or truncated!";
		fsize = dataFile.size();
	}
	nChans = params["nChans"].toUInt();
	sRate = params["sRateHz"].toDouble();
	scanCt = (fsize / (qint64)sizeof(int16)) / static_cast<qint64>(nChans);
	range.min = params["rangeMin"].toDouble();
	range.max = params["rangeMax"].toDouble();
	customRanges.clear();
	if (params.contains("customRanges")) {
		QStringList sl = params["customRanges"].toString().split(",",QString::SkipEmptyParts);
		for (QStringList::iterator it = sl.begin(); it != sl.end(); ++it) {
			QStringList sl2 = (*it).split(":",QString::SkipEmptyParts);
			if (sl2.count() == 2) {
				double min = 0., max = 0.;
				bool ok = true;
				min = sl2.first().toDouble(&ok);
				if (ok) max = sl2.last().toDouble(&ok);
				if (ok) customRanges.push_back(DAQ::Range(min, max));
			}
		}
	}
	while (customRanges.size() < nChans) customRanges.push_back(range);
	chanDisplayNames.clear();
	if (params.contains("chanDisplayNames")) {
		QStringList sl = params["chanDisplayNames"].toString().split(",",QString::SkipEmptyParts);
		for (QStringList::iterator it = sl.begin(); it != sl.end(); ++it) {
			chanDisplayNames.push_back((*it).trimmed());
		}
	}
	while (chanDisplayNames.size() < nChans) chanDisplayNames.push_back(QString("Ch ") + QString::number(chanDisplayNames.size()));

	// remember channel ids
	chanIds.clear();
	bool parseError = true;
	if (params.contains("saveChannelSubset"))  {
		QString s = params["saveChannelSubset"].toString();
		ConfigureDialogController::parseAIChanString(s, chanIds, &parseError, false);
	}
	if (parseError) {
		chanIds.resize(nChans);
		for (int i = 0; i < (int)nChans; ++i) chanIds[i] = i;
	}	
	while (chanIds.size() < nChans) {
		int i = chanIds.size() > 0 ? chanIds[chanIds.size()-1]+1 : 0;
		chanIds.push_back(i);
	}
	pd_chanId = -1;
	if (params.contains("pdChan")) pd_chanId = chanIds.size() ? chanIds[chanIds.size()-1] : -1;
	badData.clear();
    if (params.contains("badData")) {
        QStringList bdl = params["badData"].toString().split("; ", QString::SkipEmptyParts);
        for (QStringList::iterator it = bdl.begin(); it != bdl.end(); ++it) {
            QStringList l = (*it).split(",", QString::SkipEmptyParts);
            if (l.count() == 2) {
                u64 scan, ct;
                bool ok1,ok2;
                scan = l.at(0).toULongLong(&ok1);
                ct = l.at(1).toULongLong(&ok2);
                if (ok1 && ok2) pushBadData(scan,ct);
            }
        }
    }
	Debug() << "Opened " << QFileInfo(file).fileName() << " " << nChans << " chans @" << sRate << " Hz, " << scanCt << " scans total.";
	mode = Input;
	return true;
}

/// not threadsafe
i64 DataFile::readScans(std::vector<int16> & scans_out, u64 pos, u64 num2read, const QBitArray & channelSubset, unsigned downSampleFactor)
{
	if (pos > scanCt) return -1;
	if (num2read + pos > scanCt) num2read = scanCt - pos;
	QBitArray chset = channelSubset;
	if (chset.size() != (i64)nChans) chset.fill(true, nChans);
	if (downSampleFactor <= 0) downSampleFactor = 1;
	unsigned nChansOn = chset.count(true);
	
	int sizeofscans;
	if ( (num2read / downSampleFactor) * downSampleFactor < num2read )
		sizeofscans = ((num2read / downSampleFactor)+1) * nChansOn;
	else
		sizeofscans = ((num2read / downSampleFactor)) * nChansOn;
	
	scans_out.resize(sizeofscans);
	
	u64 cur = pos;
	i64 nout = 0;
	std::vector<int> onChans;
	onChans.reserve(chset.size());
	for (int i = 0, n = chset.size(); i < n; ++i) 
		if (chset.testBit(i)) onChans.push_back(i);
	
    qint64 maxBufSize = nChans*sRate*1; // read about max 1sec worth of data at a time as an optimization
    qint64 desiredBufSize = num2read*nChans;    // but first try and do the entire requested read at once in our buffer if it fits within our limits..
    if (desiredBufSize > maxBufSize) desiredBufSize = maxBufSize;
    std::vector<int16> buf(desiredBufSize);
    if (int(buf.size()) < nChans) buf.resize(nChans); // minimum read is 1 scan

    while (cur < pos + num2read) {
		if (!dataFile.seek(cur * sizeof(int16) * nChans)) {
			Error() << "Error seeking in dataFile::readScans()!";
			scans_out.clear();
			return -1;
		}		
        qint64 nr = dataFile.read(reinterpret_cast<char *>(&buf[0]), sizeof(int16) * buf.size());
        if (nr < int(sizeof(int16) * nChans)) {
			Error() << "Short read in dataFile::readScans()!";
			scans_out.clear();
			return -1;
		}
		
        int nscans = int(nr/sizeof(int16))/nChans;
        if (nscans <= 0) {
            Error() << "Short read in dataFile::readScans()... nscans <= 0!";
            scans_out.clear();
            return -1;
        }

        const int16 *bufptr = &buf[0];
        const int onChansSize = int(onChans.size());
        const int skip = nChans*downSampleFactor;
        for (int sc = 0; sc < nscans && cur < pos + num2read; /* .. */) {
                if (int(nChansOn) == nChans) {
                    i64 i_out = nout * nChans;
                    for (int i = 0; i < nChans; ++i)
                        scans_out[i_out + i] = int16(bufptr[i]);
                } else {
                    // not all chans on, put subset 1 by 1 in the output vector
                    i64 i_out = nout*i64(nChansOn);
                    for (int i = 0; i < onChansSize; ++i)
                        scans_out[i_out++] = int16(bufptr[onChans[i]]);
                }
                ++nout;
            bufptr += skip;
            sc += downSampleFactor;
            cur += downSampleFactor;
        }
    }
	
	return nout;
}

double DataFile::auxGain() const 
{
	if (params.contains("auxGain")) {
		bool ok = false;
		double r = params["auxGain"].toDouble(&ok);
		if (ok) return r;
	}
	return 1.0;
}

DAQ::Mode DataFile::daqMode() const 
{
	if (params.contains("acqMode")) {
		const QString m(params["acqMode"].toString());
		return DAQ::StringToMode(m);
	}
	return DAQ::AIRegular;
}

ChanMap DataFile::chanMap() const 
{
	ChanMap chanMap;
	if (params.contains("channelMapping2Terse")) {
		chanMap = ChanMap::fromTerseString(params["channelMapping2Terse"].toString());
	} else if (params.contains("channelMapping2")) {
		chanMap = ChanMap::fromString(params["channelMapping2"].toString());
	}
	if (!chanMap.size()) {
		// saved file lacks a chan map -- sneakily pull it from the "current" chan map settings
		ChanMappingController ctl;
		ctl.currentMode = daqMode();
		ctl.setDualDevMode(isDualDevMode() && !secondDevIsAuxOnly());
		ctl.loadSettings();
		chanMap = ctl.currentMapping();
		if (nChans * 2 < chanMap.size()) { // guess we were in dual dev mode?
			ctl.setDualDevMode(true);
			chanMap = ctl.currentMapping();
		}
	}
	return chanMap;
}

bool DataFile::isDualDevMode() const 
{
	if (params.contains("dualDevMode")) 
		return params["dualDevMode"].toBool();
	return false;
}

bool DataFile::secondDevIsAuxOnly() const
{
	if (params.contains("secondDevIsAuxOnly"))
		return params["secondDevIsAuxOnly"].toBool();
	return false;
}

DFWriteThread::DFWriteThread(DataFile *df, unsigned q_size) : QThread(0), SampleBufQ("Data Write Queue", q_size), d(df), stopflg(false)
{}

DFWriteThread::~DFWriteThread()
{
	stopflg = true;
	if (isRunning()) {
		//Error() << "INTERNAL ERROR: Waiting for data file writer thread to finish!";
		//qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
	//	wait();
	//}
///*
		if (!wait(100) && isRunning()) {
			QMessageBox *mb = new QMessageBox(0);
			mb->setWindowModality(Qt::ApplicationModal);
			mb->setStandardButtons(QMessageBox::NoButton);
			mb->setText("Waiting for data file writer thread, please be patient... ");
			mb->show();
			qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
//			waitForEmpty();
//			dataQCond.wakeAll();
			if (isRunning()) wait();
			delete mb;
		}
	}
//*/
}

void DFWriteThread::run()
{
    Error() << "DFWriteThread is no longer supported, but was created in SpikeGL!  FIXME!";
    return;
/*
    unsigned bufct = 0, noDataCt = 0;
	u64 bytect = 0;
    Debug() << "DFWriteThread started for " << d->fileName() << "  with queueSize " << dataQueueMaxSize << "...";
	std::vector<int16> buf;
	u64 scount = 0;
	while (!stopflg) {
		while (dequeueBuffer(buf, scount, false, false)) {
			++bufct;
			bytect += buf.size() * sizeof(int16);
			write(buf);
            noDataCt = 0;
		}
        if (++noDataCt > 10000) msleep(1);
	}
	Debug() << "DFWriteThread stopped after writing " << bufct << " buffers (" << bytect << " bytes in " << d->scanCount() << " scans).";
*/
}

bool DFWriteThread::write(const std::vector<int16> & scans)
{
	if (!d || ! d->dataFile.isWritable() ) return false;
	return d->doFileWrite(scans);
}

/// Returns true iff we did an asynch write and we have writes that still haven't finished.  False otherwise.
bool DataFile::hasPendingWrites() const
{
	if (mode == Output && dfwt) {
		return dfwt->dataQueueSize() > 0;
	}
	return false;
}

/// Waits timeout_ms milliseconds for pending writes to complete.  Returns true if writes finishd within allotted time (or not pending writes exist), false otherwise. If timeout_ms is negative, waits indefinitely.
bool DataFile::waitForPendingWrites(int timeout_ms) const
{
	if (hasPendingWrites()) return dfwt->waitForEmpty(timeout_ms);
	return true;
}

double DataFile::pendingWriteQFillPct() const
{
	if (dfwt) return (dfwt->dataQueueSize() / double(dfwt->dataQueueMaxSize)) * 100.;
	return 0.;
}

