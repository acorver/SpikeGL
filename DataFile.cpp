#include "DataFile.h"
#include "Util.h"
#include "SpikeGL.h"
#include "MainApp.h"
#include <QFileInfo.h>
#include <memory>
#include "ConfigureDialogController.h"
#include "ChanMappingController.h"

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
    : mode(Undefined), scanCt(0), nChans(0), sRate(0), writeRateAvg(0.), nWritesAvg(0), nWritesAvgMax(1)
{
}

DataFile::~DataFile() {}

bool DataFile::closeAndFinalize() 
{
    if (!isOpen()) return false;
	if (mode == Input) {
		dataFile.close();
		metaFile.close();
		nChans = scanCt = sRate = 0;
		mode = Undefined;
		return true;
	} else if (mode == Output) {
		// Output mode...
		sha.Final();
		params["sha1"] = sha.ReportHash().c_str();
		params["fileTimeSecs"] = fileTimeSecs();
		params["fileSizeBytes"] = dataFile.size();
		dataFile.close();
		QString mf = metaFile.fileName();
		metaFile.close(); // close it.. we never really wrote to it.. we just reserved it on the FS    
		writeRateAvg = 0.;
		nWritesAvg = nWritesAvgMax = 0;
		mode = Undefined;
		return params.toFile(mf);
	} 
	return false; // not normally reached...
}

bool DataFile::writeScans(const std::vector<int16> & scans)
{
    double tWrite = getTime();

    if (!isOpen()) return false;
    if (!scans.size() || (scans.size() % nChans)) {
        Error() << "writeScan  Need to send scan needs to be of size a multiple of " << nChans << " chans long";
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
    const int n2Write = scans.size()*sizeof(int16);

    int nWrit = dataFile.write((const char *)&scans[0], n2Write);

    if (nWrit != n2Write) {
        Error() << "writeScan  Error returned from write call: " << nWrit;
        return false;
    }
    sha.UpdateHash((const uint8_t *)&scans[0], n2Write);
    scanCt += scans.size()/nChans;

    tWrite = getTime() - tWrite;

    // update write speed..
    writeRateAvg = (writeRateAvg*nWritesAvg+(n2Write/tWrite))/double(nWritesAvg+1);
    if (++nWritesAvg > nWritesAvgMax) nWritesAvg = nWritesAvgMax;

    return true;
}

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
	
	mode = Output;
	const int nOnChans = chanNumSubset.size();
	params = other.params;
	params["outputFile"] = outputFile;
	scanCt = 0;
	nChans = nOnChans;
	sha.Reset();
	sRate = other.sRate;
    writeRateAvg = 0.;
    nWritesAvg = 0;
    nWritesAvgMax = /*unsigned(sRate/10.)*/10;
    if (!nWritesAvgMax) nWritesAvgMax = 1;
	// compute save channel subset fudge
	const QVector<unsigned> ocid = other.channelIDs();
	chanIds.clear();
	foreach (unsigned i, chanNumSubset) {
		if (i < unsigned(ocid.size()))
			chanIds.push_back(ocid[i]);
		else 
			Error() << "INTERNAL ERROR: The chanNumSubset passet to DataFile::openForRead must be a subset of channel numbers (indices, not IDs) to use in the rewrite.";
	}
	params["saveChannelSubset"] = ConfigureDialogController::generateAIChanString(chanIds);
	params["nChans"] = nChans;
	pd_chanId = other.pd_chanId;
	
	return true;
}

bool DataFile::openForWrite(const DAQ::Params & dp, const QString & filename_override) 
{
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
    scanCt = 0;
    nChans = nOnChans;
    sRate = dp.srate;
    writeRateAvg = 0.;
    nWritesAvg = 0;
    nWritesAvgMax = /*unsigned(sRate/10.)*/10;
    if (!nWritesAvgMax) nWritesAvgMax = 1;
    params["outputFile"] = outputFile;
    params["dev"] = dp.dev;
    params["devProductName"] = DAQ::GetProductName(dp.dev);
    params["nChans"] = nChans;
    params["sRateHz"] = sRate;
    params["rangeMin"] = dp.range.min;
    params["rangeMax"] = dp.range.max;
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
    }
    params["acqStartEndMode"] = DAQ::AcqStartEndModeToString(dp.acqStartEndMode);
    if (dp.demuxedBitMap.count(false)) {
        params["saveChannelSubset"] = dp.subsetString;
    } else 
        params["saveChannelSubset"] = "ALL";
	mode = Output;
    return true;
}

/// param management
void DataFile::setParam(const QString & name, const QVariant & value)
{
    params[name] = value;
}
/// param management
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
	rangeMinMax.first = params["rangeMin"].toDouble();
	rangeMinMax.second = params["rangeMax"].toDouble();
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
		
	Debug() << "Opened " << QFileInfo(file).fileName() << " " << nChans << " chans @" << sRate << " Hz, " << scanCt << " scans total.";
	mode = Input;
	return true;
}

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
	unsigned procd = 0;
	i64 nout = 0;
	std::vector<int> onChans;
	onChans.reserve(chset.size());
	for (int i = 0, n = chset.size(); i < n; ++i) 
		if (chset.testBit(i)) onChans.push_back(i);
	
	std::vector<double> avgs(nChans,0.);
	const double factor = 1.0/downSampleFactor;
	while (cur < pos + num2read) {
		std::vector<int16> buf(nChans);
		if (!dataFile.seek(cur * sizeof(int16) * nChans)) {
			Error() << "Error seeking in dataFile::readScans()!";
			scans_out.clear();
			return -1;
		}		
		qint64 nr = dataFile.read(reinterpret_cast<char *>(&buf[0]), sizeof(int16) * nChans);
		if (nr != sizeof(int16) * nChans) {
			Error() << "Short read in dataFile::readScans()!";
			scans_out.clear();
			return -1;
		}
		
		for (int i = 0; i < nChans; ++i)
			avgs[i] += double(buf[i]) * factor;
		
		++procd;
		if (downSampleFactor <= 1 || !(procd % downSampleFactor)) { // every Nth sample, write it out
			if (int(nChansOn) == nChans) {
				i64 i_out = nout * nChans;
				for (int i = 0; i < nChans; ++i)
					scans_out[i_out + i] = int16(avgs[i]);
			} else {
				// not all chans on, put subset 1 by 1 in the output vector
				i64 i_out = nout*nChansOn;
				const int n = onChans.size();
				for (int i = 0; i < n; ++i)
					scans_out[i_out++] = int16(avgs[onChans[i]]);
			}
			for (int i = 0; i < (int)nChans; ++i) avgs[i] = 0.;
			++nout;
		}
		++cur;
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
		ctl.loadSettings();
		chanMap = ctl.mappingForMode(daqMode());
	}
	return chanMap;
}
