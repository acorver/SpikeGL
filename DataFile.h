#include <QString>
#include <QFile>
#include "TypeDefs.h"
#include "Params.h"
#ifndef DataFile_H
#define DataFile_H

#include "sha1.h"
#include "DAQ.h"

class DataFile
{
public:
    DataFile();
    ~DataFile();

    bool openForWrite(const DAQ::Params & params, const QString & filename_override = "");

    bool isOpen() const { return outFile.isOpen() && metaFile.isOpen(); }
    QString fileName() const { return outFile.fileName(); }
    QString metaFileName() const { return metaFile.fileName(); }

    /// param management
    void setParam(const QString & name, const QVariant & value);
    /// param management
    const QVariant & getParam(const QString & name) const;

    /// closes the file, and saves the SHA1 hash to the metafile
    bool closeAndFinalize();

    /** Write complete scans to the file.  
        Must be vector of length a multiple of  numChans() otherwise 
        it will fail unconditionally */
    bool writeScans(const std::vector<int16> & scan);

    u64 sampleCount() const { return scanCt*(u64)nChans; }
    u64 scanCount() const { return scanCt; }
    unsigned numChans() const { return nChans; }
    double samplingRateHz() const { return sRate; }

    /// the average speed in bytes/sec for writes
    double writeSpeedBytesSec() const { return writeRateAvg; }
    /// the minimal write speed required in bytes/sec, based on sample rate
    double minimalWriteSpeedRequired() const { return nChans*sizeof(int16)*double(sRate); }

    /// STATIC METHODS
    static bool verifySHA1(const QString & filename); 

private:
    
    QFile outFile, metaFile;
    SHA1 sha;
    Params params;
    u64 scanCt;
    unsigned nChans;
    double sRate;
    double writeRateAvg; ///< in bytes/sec
    unsigned nWritesAvg, nWritesAvgMax; ///< the number of writes in the average, tops off at sRate/10
};
#endif
