#include <QString>
#include <QFile>
#include <QMutex>
#include "TypeDefs.h"
#ifndef TempDataFile_H
#define TempDataFile_H

#include "DAQ.h"

#define TEMP_FILE_NAME_PREFIX "SpikeGL_DSTemp_"
#define TEMP_FILE_NAME_SUFFIX ".bin"

class TempDataFile
{
public:
    TempDataFile();
    ~TempDataFile();

    bool openForWrite();
    bool openForRead(QFile &file) const;

    void setNChans(unsigned int chans) { nChans = chans; }
    void close();

    bool writeScans(const int16 * scan, unsigned nsamps);
    bool readScans(QVector<int16> & outbuf, i64 nfrom, i64 nread, const QBitArray & channelSubset, unsigned downsample_factor = 1) const;
    void setTempFileSize(qint64 newSize) { maxSize = newSize; }

    qint64 getTempFileSize() const { return maxSize; }    
    qint64 getScanCount() const { return scanCount; }

    QString getChannelSubset() const;

private:
    QFile tempFile;
    i64 maxSize, currSize; // in bytes
    unsigned nChans;
    i64 scanCount;
    QString fileName;

    mutable QMutex lock;
};
#endif
