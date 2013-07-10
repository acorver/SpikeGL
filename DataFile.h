#include <QString>
#include <QFile>
#include <QPair>
#include <QMutex>
#include <QList>
#include <QVector>
#include <QMutexLocker>
#include "TypeDefs.h"
#include "Params.h"
#ifndef DataFile_H
#define DataFile_H

#include "sha1.h"
#include "DAQ.h"
#include "ChanMap.h"

class DFWriteThread;

class DataFile
{
	friend class DFWriteThread;
	
public:
    DataFile();
    ~DataFile();

	/** Checks if filename (presumably a .bin file) is openable for read and that
	    it is valid (has the proper size as specified in the meta file, etc).  
		If it's valid, returns true.  Otherwise returns false, and, if error is not 
	    NULL, optionally sets *error to an appropriate error message. */
	static bool isValidInputFile(const QString & filename, QString * error = 0);
	
    bool openForWrite(const DAQ::Params & params, const QString & filename_override = "");
	
	/** Normally you won't use this method.  This is used by the FileViewerWindow export code to reopen a new file for output
	    based on a previous input file.  It computes the channel subset and other parameters correctly from the input file.
	    The passed-in chanNumSubset is a subset of channel indices (not chan id's!) to use in the export.  So if the 
	    channel id's you are reading in are 0,1,2,3,6,7,8 and you want to export the last 3 of these using openForRewrite, you would 
	    pass in [4,5,6] (not [6,7,8]) as the chanNumSubset. */
	bool openForReWrite(const DataFile & other, const QString & filename, const QVector<unsigned> & chanNumSubset);

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

    /// call this to indicate certain regions of the file are "bad", that is, they contained dropped/fake scans
    void pushBadData(u64 scan, u64 length) { badData.push_back(QPair<u64,u64>(scan,length)); }
    typedef QList<QPair<u64,u64> > BadData;
    const BadData & badDataList() const { return badData; }

    /// closes the file, and saves the SHA1 hash to the metafile 
    bool closeAndFinalize();

    /** Write complete scans to the file.  File must have been opened for write 
		using openForWrite().
        Must be vector of length a multiple of  numChans() otherwise it will 
	    fail unconditionally */
    bool writeScans(const std::vector<int16> & scan, bool asynch = false);
	
	/// Returns true iff we did an asynch write and we have writes that still haven't finished.  False otherwise.
	bool hasPendingWrites() const;
	
	/// Waits timeout_ms milliseconds for pending writes to complete.  Returns true if writes finishd within allotted time, false otherwise. If timeout_ms is negative, waits indefinitely.
	bool waitForPendingWrites(int timeout_ms) const;
	
	/// If using asynch writes, returns the pending write queue fill percent, otherwise 0.
	double pendingWriteQFillPct() const;
	
	/** Read scans from the file.  File must have been opened for read
	    using openForRead().  Returns number of scans actually read or -1 on failure.  
		NB: Note that return value is normally num2read / downSampleFactor. 
	    NB 2: Short reads are supported -- that is, if pos + num2read is past 
	    the end of file, the number of scans available is read instead.  
	    The return value will reflect this.	 */
	i64 readScans(std::vector<int16> & scans_out, u64 pos, u64 num2read, const QBitArray & channelSubset = QBitArray(), unsigned downSampleFactor = 1);
	
    u64 sampleCount() const { return scanCt*(u64)nChans; }
    u64 scanCount() const { return scanCt; }
    unsigned numChans() const { return nChans; }
    double samplingRateHz() const { return sRate; }
	double fileTimeSecs() const { return double(scanCt) / double(sRate); }
	double rangeMin() const { return rangeMinMax.first; }
	double rangeMax() const { return rangeMinMax.second; }
	/// from meta file: based on the channel subset used for this data file, returns a list of the channel id's for all the channels in the data file
	const QVector<unsigned> & channelIDs() const { return chanIds; }
	int pdChanID() const { return pd_chanId; } ///< returns negative value if not using pd channel
	/// aux gain value from .meta file or 1.0 if "auxGain" field not found in meta file
	double auxGain() const;
	bool isDualDevMode() const;
	bool secondDevIsAuxOnly() const;
	DAQ::Mode daqMode() const;
	ChanMap chanMap() const;

    /// the average speed in bytes/sec for writes
    double writeSpeedBytesSec() const { QMutexLocker l(&statsMut); return writeRateAvg; }
    /// the minimal write speed required in bytes/sec, based on sample rate
    double minimalWriteSpeedRequired() const { return nChans*sizeof(int16)*double(sRate); }

    /// STATIC METHODS
    static bool verifySHA1(const QString & filename); 

protected:
	bool doFileWrite(const std::vector<int16> & scans);
	
private:
    
	enum { Undefined, Input, Output } mode;
	
	/// member vars used for Input and Output mode
    QFile dataFile, metaFile;
    Params params;
    u64 scanCt;
    int nChans;
    double sRate;

    // list of scan_number,number_of_scans for locations of bad/faked data for buffer overrun situations
    BadData badData;

	/// member vars used for Input mode
	QPair<double,double> rangeMinMax;
	QVector<unsigned> chanIds;
	int pd_chanId;
	
	/// member vars used for Output mode only
	mutable QMutex statsMut;
    SHA1 sha;
    double writeRateAvg; ///< in bytes/sec
    unsigned nWritesAvg, nWritesAvgMax; ///< the number of writes in the average, tops off at sRate/10
	DFWriteThread *dfwt;
};
#endif
