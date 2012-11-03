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
void SampleBufQ::enqueueBuffer(std::vector<int16> &src, u64 sampCount)
{
        QMutexLocker l(&dataQMut);
        if (dataQ.size() >= dataQueueMaxSize) {
            overflowWarning();
            dataQ.pop_front();        
        }
        SampleBuf buf;
        buf.sampleCountOfFirstPoint = sampCount;
        dataQ.push_back(buf);
        src.swap(dataQ.back().data);
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
bool SampleBufQ::dequeueBuffer(std::vector<int16> & dest, u64 & sampCount, bool wait, bool err_prt)
{
        bool ret = false, ok = false;
        dest.clear();
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
                dest.swap(buf.data);
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
