#ifndef SampleBufQ_H
#define SampleBufQ_H
#include <QMutex>
#include <vector>
#include <deque>
#include <QMutexLocker>
#include "TypeDefs.h"
#include <QWaitCondition>
#include <QList>
#include <QString>

/// A zero-copy queue of sample/scan data that is thread safe.
class SampleBufQ 
{
public:
    SampleBufQ(const QString & name = "<unnamed>", unsigned dataQueueMaxSizeInBufs = 64);
    virtual ~SampleBufQ();
	
    void clear();

	const QString name;
    const unsigned dataQueueMaxSize; 

    unsigned dataQueueSize() const { QMutexLocker l(&dataQMut); return dataQ.size(); }
	
    /// put data in buffer.  calls overflowWarning() if buffer overflows
    /// swaps in src with an empty buffer if successful, calls overflowWarning() on overflow
    void enqueueBuffer(std::vector<int16> & src, u64 sampleCount);

    /// returns true if actual data was available -- in which case dest is swapped for a data buffer in the deque
    bool dequeueBuffer(std::vector<int16> & dest, u64 & sampleCountOfFirstPoint, bool wait = false, bool printError = true);

	/** returns true if queue is empty and/or if we waited and it was empty before timeout
	    returns false otherwise.  Negative timeout is infinite wait. */
	bool waitForEmpty(int ms=-1);
	
	/* -- STATIC FUNCTIONS -- */
	
	/// Iterates through all sample buf q's, returns sum of all their sizes
	static unsigned allDataQueueSizes();
	/// Iterates through all sample buf q's, returns sum of all their max sizes
	static unsigned allDataQueueMaxSizes();
	/// Returns a list of all queues above a certain threshold fill percentage
	static QList<SampleBufQ *> allQueuesAbove(double percent);

protected:
    virtual void overflowWarning(); ///< default impl prints a warning to console

    struct SampleBuf {
        std::vector<int16> data;
        u64 sampleCountOfFirstPoint;
    };
    std::deque<SampleBuf> dataQ;
    mutable QMutex dataQMut;
    mutable QWaitCondition dataQCond, dataQEmptyCond;

	static QList<SampleBufQ *> allQs;
	static QMutex allQsMut;
};

#endif
