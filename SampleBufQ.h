#ifndef SampleBufQ_H
#define SampleBufQ_H
#include <QMutex>
#include <vector>
#include <deque>
#include <QMutexLocker>
#include "TypeDefs.h"
#include <QWaitCondition>

/// A zero-copy queue of sample/scan data that is thread safe.
class SampleBufQ 
{
public:
    SampleBufQ(unsigned dataQueueMaxSizeInBufs = 64);
    virtual ~SampleBufQ();

    void clear();

    const unsigned dataQueueMaxSize; 

    unsigned dataQueueSize() const { QMutexLocker l(&dataQMut); return dataQ.size(); }

    /// put data in buffer.  calls overflowWarning() if buffer overflows
    /// swaps in src with an empty buffer if successful, calls overflowWarning() on overflow
    void enqueueBuffer(std::vector<int16> & src, u64 sampleCount);

    /// returns true if actual data was available -- in which case dest is swapped for a data buffer in the deque
    bool dequeueBuffer(std::vector<int16> & dest, u64 & sampleCountOfFirstPoint, bool wait = false);

protected:
    virtual void overflowWarning(); ///< default impl prints a warning to console

    struct SampleBuf {
        std::vector<int16> data;
        u64 sampleCountOfFirstPoint;
    };
    std::deque<SampleBuf> dataQ;
    mutable QMutex dataQMut;
    mutable QWaitCondition dataQCond;
    
};

#endif
