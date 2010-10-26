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

	/** Checks if filename (presumably a .bin file) is openable for read and that
	    it is valid (has the proper size as specified in the meta file, etc).  
		If it's valid, returns true.  Otherwise returns false, and, if error is not 
	    NULL, optionally sets *error to an appropriate error message. */
	static bool isValidInputFile(const QString & filename, QString * error = 0);
	
    bool openForWrite(const DAQ::Params & params, const QString & filename_override = "");

	/** Returns true if binFileName was successfully opened, false otherwise. If 
	    opened, puts this instance into Input mode. */
	bool openForRead(const QString & binFileName);

    bool isOpen() const { return dataFile.isOpen() && metaFile.isOpen(); }
	bool isOpenForRead() const { return isOpen() && mode == Input; }
	bool isOpenForWrite() const { return isOpen() && mode == Output; }
    QString fileName() const { return dataFile.fileName(); }
    QString metaFileName() const { return metaFile.fileName(); }

    /// param management
    void setParam(const QString & name, const QVariant & value);
    /// param management
    const QVariant & getParam(const QString & name) const;

    /// closes the file, and saves the SHA1 hash to the metafile 
    bool closeAndFinalize();

    /** Write complete scans to the file.  File must have been opened for write 
		using openForWrite().
        Must be vector of length a multiple of  numChans() otherwise it will 
	    fail unconditionally */
    bool writeScans(const std::vector<int16> & scan);

    u64 sampleCount() const { return scanCt*(u64)nChans; }
    u64 scanCount() const { return scanCt; }
    unsigned numChans() const { return nChans; }
    double samplingRateHz() const { return sRate; }
	double fileTimeSecs() const { return double(scanCt) / double(sRate); }

    /// the average speed in bytes/sec for writes
    double writeSpeedBytesSec() const { return writeRateAvg; }
    /// the minimal write speed required in bytes/sec, based on sample rate
    double minimalWriteSpeedRequired() const { return nChans*sizeof(int16)*double(sRate); }

    /// STATIC METHODS
    static bool verifySHA1(const QString & filename); 

private:
    
	enum { Undefined, Input, Output } mode;
	
	/// member vars used for Input and Output mode
    QFile dataFile, metaFile;
    Params params;
    u64 scanCt;
    unsigned nChans;
    double sRate;
	
	/// member var used for Input mode
	
	
	/// member vars used for Output mode only
    SHA1 sha;
    double writeRateAvg; ///< in bytes/sec
    unsigned nWritesAvg, nWritesAvgMax; ///< the number of writes in the average, tops off at sRate/10
};
#endif
