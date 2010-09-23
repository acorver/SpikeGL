#include <QString>
#include <QFile>
#include <QMutex>
#include "TypeDefs.h"
#ifndef DataTempFile_H
#define DataTempFile_H

#include "DAQ.h"

#define TEMP_FILE_NAME_PREFIX "SpikeGL_DSTemp_"
#define TEMP_FILE_NAME_SUFFIX ".bin"

class DataTempFile
{
public:
    DataTempFile();
    ~DataTempFile();

    bool openForWrite();
    bool openForRead(QFile &file) const;

    void setNChans(unsigned int chans) { nChans = chans; }
    void close();

    bool writeScans(const std::vector<int16> & scan);
    bool readScans(QVector<int16> & outbuf, qint64 nfrom, qint64 nread, const QBitArray & channelSubset, unsigned downsample_factor = 1) const;
    void setTempFileSize(qint64 newSize) { maxSize = newSize; }

    qint64 getTempFileSize() const { return maxSize; }    
    qint64 getScanCount() const { return scanCount; }

    QString getChannelSubset() const;

private:
    QFile tempFile;
    qint64 maxSize, currSize; // in bytes
    unsigned nChans;
    qint64 scanCount;
    QString fileName;

    mutable QMutex lock;
};
#endif
