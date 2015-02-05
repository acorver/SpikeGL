#include "SampleBufQ.h"
#include "Util.h"
#include "SpikeGL.h"

/*static*/ QList<SampleBufQ *> SampleBufQ::allQs;
/*static*/ QMutex SampleBufQ::allQsMut;

SampleBufQ::SampleBufQ(const QString & name, unsigned dataQueueMaxSizeInBufs)
    : name(name), dataQueueMaxSize(dataQueueMaxSizeInBufs) 
{
	QMutexLocker l(&allQsMut);
	allQs.push_back(this);	
}

SampleBufQ::~SampleBufQ() 
{
	QMutexLocker l(&allQsMut);
	allQs.removeAll(this);
}

void SampleBufQ::clear()
{
    QMutexLocker l(&dataQMut);
    dataQ.clear();
    //dataQCond.wakeAll();
}

/// put data in buffer.  calls overflowWarning() if buffer overflows
void SampleBufQ::enqueueBuffer(std::vector<int16> &src, u64 sampCount, bool putFakeDataOnOverrun, int fakeDataOverride, const QByteArray & metaData)
{
        QMutexLocker l(&dataQMut);
		SampleBuf buf;
        if (dataQ.size() >= dataQueueMaxSize) {

			overflowWarning();

			if (putFakeDataOnOverrun) {
				// overrun, indicate buffer is empty but should contain 'fake' data on dequeue
				buf.fakeSize = fakeDataOverride ? fakeDataOverride : src.size();
				buf.fakeMetaSize = metaData.size();
			} else {
		        dataQ.pop_front();
			}
        }
        buf.sampleCountOfFirstPoint = sampCount;
		if (!buf.fakeSize) {
			if (fakeDataOverride) {
				buf.fakeSize = fakeDataOverride;
				buf.fakeMetaSize = metaData.size();
			} else {
				src.swap(buf.data);
				buf.metaData = metaData;
			}
		}
        dataQ.push_back(buf);
		src.clear();
        dataQCond.wakeOne();
}

bool SampleBufQ::waitForEmpty(int ms)
{
	bool ok = dataQMut.tryLock(ms);
	if (ok) {
		if (dataQ.size()) 
			ok = dataQEmptyCond.wait(&dataQMut, ms < 0 ? ULONG_MAX : ms);
	}	
	if (ok) dataQMut.unlock();
	return ok;
}

    /// returns true if actual data was available -- in which case dest is swapped for a data buffer in the deque
bool SampleBufQ::dequeueBuffer(std::vector<int16> & dest, u64 & sampCount, bool wait, bool err_prt, int *fakeDataSz, bool expandFakeData, QByteArray *metaData)
{
        bool ret = false, ok = false;
        dest.clear();
		if (fakeDataSz) *fakeDataSz = -1;
        if (wait) {
            ok = dataQMut.tryLock(LOCK_TIMEOUT_MS);
            if (ok) {
                if (!dataQ.size())
                    ok = dataQCond.wait(&dataQMut);
            }
        } else {
            ok = dataQMut.tryLock(LOCK_TIMEOUT_MS);
        }
        if (ok) {
            if (dataQ.size()) {
                SampleBuf & buf = dataQ.front();
                sampCount = buf.sampleCountOfFirstPoint;
				if (!buf.fakeSize) {
					// real buffer, put data
	                dest.swap(buf.data);
					if (metaData) *metaData = buf.metaData;
				} else {
					// fake buffer, put fake 0x7fff data!
					dest.clear();
					if (metaData) metaData->clear();
                    if (expandFakeData) {
					    dest.resize(buf.fakeSize,0x7fff);
						if (metaData) metaData->fill(0,buf.fakeMetaSize);
					}
					if (fakeDataSz) *fakeDataSz = buf.fakeSize;
				}
                dataQ.pop_front();
                ret = true;
            }
			if (!dataQ.size()) dataQEmptyCond.wakeAll();
            dataQMut.unlock();
        } else {
			if (err_prt) {
				QString e = "SampleBufQ::dequeueBuffer lock timeout on buffer mutex!"; 
				Error() << e;
			}
        }
        return ret;
}

void SampleBufQ::overflowWarning() 
{
    Warning() << name << " overflow! Buffer queue full (capacity: " <<  dataQueueMaxSize << " buffers)!  Dropping a buffer!";
}


/// Iterates through all sample buf q's, returns sum of all their sizes
/*static*/ unsigned SampleBufQ::allDataQueueSizes()
{
	unsigned sum = 0;
	QMutexLocker l(&allQsMut);
	for (QList<SampleBufQ *>::iterator it = allQs.begin(); it != allQs.end(); ++it) {
		sum += (*it)->dataQueueSize();
	}
	return sum;
}
/// Iterates through all sample buf q's, returns sum of all their max sizes
/*static*/ unsigned SampleBufQ::allDataQueueMaxSizes()
{
	unsigned sum = 0;
	QMutexLocker l(&allQsMut);
	for (QList<SampleBufQ *>::iterator it = allQs.begin(); it != allQs.end(); ++it) {
		sum += (*it)->dataQueueMaxSize;
	}
	return sum;	
}
/// Returns a list of all queues above a certain threshold fill percentage
/*static*/ QList<SampleBufQ *> SampleBufQ::allQueuesAbove(double pct)
{
	QList<SampleBufQ *> ret;
	QMutexLocker l(&allQsMut);
	for (QList<SampleBufQ *>::iterator it = allQs.begin(); it != allQs.end(); ++it) {
		if ( (double((*it)->dataQueueSize())*100.0) / double((*it)->dataQueueMaxSize) >= pct )
			ret.push_back(*it);
	}
	return ret;
}
