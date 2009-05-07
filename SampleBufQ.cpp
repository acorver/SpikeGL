#include "SampleBufQ.h"
#include "Util.h"
#include "LeoDAQGL.h"

SampleBufQ::SampleBufQ(unsigned dataQueueMaxSizeInBufs)
    : dataQueueMaxSize(dataQueueMaxSizeInBufs) {}

SampleBufQ::~SampleBufQ() {}


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

    /// returns true if actual data was available -- in which case dest is swapped for a data buffer in the deque
bool SampleBufQ::dequeueBuffer(std::vector<int16> & dest, u64 & sampCount, bool wait)
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
            dataQMut.unlock();
        } else {
            QString e = "SampleBufQ::dequeueBuffer lock timeout on buffer mutex!"; 
            Error() << e;
        }
        return ret;
}

void SampleBufQ::overflowWarning() 
{
    Warning() << "SampleBufQ overflow! Buffer queue full (capacity: " <<  dataQueueMaxSize << " buffers)!  Dropping a buffer!";
}
