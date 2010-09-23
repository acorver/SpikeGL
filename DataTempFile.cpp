#include "DataTempFile.h"
#include "Util.h"
#include "SpikeGL.h"
#include "MainApp.h"
#include "ConfigureDialogController.h"

DataTempFile::DataTempFile()
    :   maxSize(1048576000),
		currSize(0),
        nChans(1),
        scanCount(0)
{
    Util::removeDataTempFiles();
    fileName = QDir::tempPath() +
               "/SpikeGL_DSTemp_" +
               QString::number(QCoreApplication::applicationPid()) +
               ".bin";

    tempFile.setFileName(fileName);
}

DataTempFile::~DataTempFile() 
{
    Util::removeDataTempFiles();
}

bool DataTempFile::writeScans(const std::vector<int16> & scans)
{
    if (!tempFile.isOpen() && !openForWrite())
        return false;
	
    qint64 pos = tempFile.pos();

    std::vector<int16>::size_type bytes2Write = scans.size() * sizeof(int16);
    const quint64 freeSpace = Util::availableDiskSpace();
    // check disk filling up and adjust the temporary file's maximum size
    // leave there 10MB of free space
    if (bytes2Write + 10485760 > freeSpace && maxSize > tempFile.size())
    {
        lock.lock();

        maxSize = tempFile.size();

        lock.unlock();
    }

    const int scanSz = nChans * sizeof(int16);
	const int nScans = scans.size() / nChans;
    const int nFitsToEOF = (maxSize - pos) / scanSz;
    qint64 nWritten = 0, nWritten0 = 0;

    if (nFitsToEOF < nScans && nFitsToEOF)
    {
        nWritten = tempFile.write((const char*)&scans[0], nFitsToEOF*scanSz) / scanSz;

        if (nWritten != nFitsToEOF)
            Warning() << "Writing to temporary file failed (expected to write " << nFitsToEOF << " scans, wrote " << nWritten << " scans)";

		nWritten0 = nFitsToEOF; // pretend it was ok even on error
        bytes2Write -= nFitsToEOF*scanSz;
        tempFile.seek(pos=0);
    }

    nWritten = tempFile.write((const char *)&scans[nWritten0*nChans], bytes2Write) / scanSz;
	pos += bytes2Write;
    if (nWritten*scanSz != bytes2Write)
    {
        Warning() << "Writing to temporary file failed (expected to write " << (bytes2Write/scanSz) << " scans, wrote " << nWritten << " scans)";		
		tempFile.seek(pos);
    }

    lock.lock();

    scanCount += nWritten + nWritten0;
	currSize = tempFile.size();

    lock.unlock();

    return true;
}

bool DataTempFile::readScans(QVector<int16> & out, qint64 nfrom, qint64 nread, const QBitArray &channelSubset, unsigned downsample) const
{
    QFile readFile;
	
    if (!openForRead(readFile))
        return false;

    lock.lock();

	const qint64 my_scanCount = scanCount, my_currSize = currSize;
	
	lock.unlock();
	
    const qint64 scanSz = nChans * sizeof(int16);
    const qint64 maxNScans = my_currSize / scanSz;
    const qint64 maxPos = scanSz * maxNScans;
	
    qint64 readCount = nread >= 0 ? nread : 1;
	
	if (readCount > maxNScans) readCount = maxNScans;
	if (readCount > 20000000) readCount = 20000000; // max 20 million scans
	if (nfrom + readCount > my_scanCount) readCount = my_scanCount - nfrom;
	
	if (nfrom < 0 || nfrom+readCount > my_scanCount) {
		Error() << "Specified invalid scan range for nfrom and nread parameters to DataTempFile::readScans()";
		return false;
	}
	
	
    qint64 pos = (nfrom % maxNScans) * scanSz;
    if (!readFile.seek(pos))
    {
		Error() << "Reading seek to " << pos << " failed in DataTempFile::readScans()";
        readFile.close();
        return false;
    }

	if (downsample <= 0) downsample = 1;

	const int nChansOn = channelSubset.count(true), subsetSize = channelSubset.size();
	out.clear();
    out.reserve((readCount/downsample) * nChansOn);

    qint64 readSoFar = 0;

	QVector<int16> scan(nChans,0);

	QVector<int> chansOn; 
	chansOn.reserve(nChansOn);
	for (int i = 0; i < subsetSize; ++i)
		if (channelSubset.testBit(i))
			chansOn.append(i);
	
    while (readSoFar < readCount)
    {
		if (pos + scanSz > maxPos) 
			readFile.seek(pos=0);
		
		
        qint64 read = readFile.read((char*)&scan[0], scanSz);
				
        if (scanSz == read)
        {
			if (((int)nChans) == nChansOn)
				// happens to have all chans on, so just append the entire scan
				out += scan;
			else { 
				// do a channel subset
				for (int i = 0; i < nChansOn; ++i)
					out.append(scan[chansOn[i]]);
			}
			pos += read;
        }
        else
        {
            Warning() << "Reading from temporary file failed (expected " << scanSz << " read " << read << ")";
			pos = (pos + scanSz) % maxPos;
            readFile.seek(pos);
        }

		// skip around in the file -- this is how our downsampling works
		if (downsample > 1) {
			const int skip = downsample-1;
			pos = (pos + skip*scanSz) % maxPos;
			readFile.seek(pos);
			readSoFar += skip;
		}
		
        ++readSoFar;
    }

    return true;
}

bool DataTempFile::openForWrite() 
{	
	if (tempFile.isOpen()) return true;
	lock.lock();
	scanCount = currSize = 0;
	lock.unlock();
    if (!tempFile.open(QIODevice::WriteOnly|QIODevice::Truncate)) {
        Error() << "Failed to open the temporary file " << tempFile.fileName() << " for write";
        return false;
    }
	Debug() << "Opened Matlab data API temp file for write: " << tempFile.fileName();
    return true;
}

bool DataTempFile::openForRead(QFile & file) const
{
	file.setFileName(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        Warning() << "Failed to open the temporary file " << fileName << " for read";
        return false;
    }

    return true;
}

QString DataTempFile::getChannelSubset() const
{
    QString ret;
    QTextStream ts(&ret, QIODevice::WriteOnly);
    QBitArray bitArr = mainApp()->configureDialogController()->acceptedParams.demuxedBitMap;
    for (int i = 0, n = bitArr.size(); i < n; ++i)
    {
        if (bitArr.testBit(i))
            ts << i << " ";
    }
	ts << "\n";
    return ret;
}


void DataTempFile::close()
{
	tempFile.close();
    Util::removeDataTempFiles();
	lock.lock();
	scanCount = currSize = 0;
	lock.unlock();
	Debug() << "Closed and removed Matlab API temp file.";
}


