#include "DataFile.h"
#include "Util.h"
#include "LeoDAQGL.h"

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
    : scanCt(0), nChans(0), sRate(0), writeRateAvg(0.), nWritesAvg(0), nWritesAvgMax(1)
{
}

DataFile::~DataFile() {}

bool DataFile::closeAndFinalize() 
{
    if (!isOpen()) return false;
    sha.Final();
    params["sha1"] = sha.ReportHash().c_str();
    params["fileTimeSecs"] = double(scanCt)/double(sRate);
    params["fileSizeBytes"] = outFile.size();
    outFile.close();
    QString mf = metaFile.fileName();
    metaFile.close(); // close it.. we never really wrote to it.. we just reserved it on the FS    
    writeRateAvg = 0.;
    nWritesAvg = nWritesAvgMax = 0;
    return params.toFile(mf);
}

bool DataFile::writeScans(const std::vector<int16> & scans)
{
    double tWrite = getTime();

    if (!isOpen()) return false;
    if (!scans.size() || (scans.size() % nChans)) {
        Error() << "writeScan  Need to send scan needs to be of size a multiple of " << nChans << " chans long";
        return false;
    }
    const int n2Write = scans.size()*sizeof(int16);

    int nWrit = outFile.write((const char *)&scans[0], n2Write);

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

bool DataFile::openForWrite(const DAQ::Params & dp, const QString & filename_override) 
{
    if (!dp.aiChannels.size()) {
        Error() << "DataFile::openForWrite Error cannot open a datafile with scansize of 0!";
        return false;
    }
    if (isOpen()) closeAndFinalize();

    const QString outputFile = filename_override.length() ? filename_override : dp.outputFile;

    outFile.setFileName(outputFile);
    metaFile.setFileName(metaFileForFileName(outputFile));

    if (!outFile.open(QIODevice::WriteOnly|QIODevice::Truncate) ||
        !metaFile.open(QIODevice::WriteOnly|QIODevice::Truncate)) {
        Error() << "Failed to open either one or both of the data and meta files for " << outputFile;
        return false;
    }
    sha.Reset();
    params = Params();
    scanCt = 0;
    nChans = dp.nVAIChans;
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
    if (dp.usePD) {
        params["pdChan"] = dp.pdChan;
        params["pdThresh"] = dp.pdThresh;
    }
    params["acqStartEndMode"] = DAQ::AcqStartEndModeToString(dp.acqStartEndMode);
    
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

