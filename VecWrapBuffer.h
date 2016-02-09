#ifndef VecWrapBuffer_H
#define VecWrapBuffer_H

#include "WrapBuffer.h"
#include "Vec.h"


template<typename VECT> 
class VecWrapBuffer : protected WrapBuffer
{
public:
    VecWrapBuffer(unsigned nVec2sForSize = 0)
	: WrapBuffer(nVec2sForSize*sizeof(VECT)) {}
	
    void reserve(unsigned newSize) { return WrapBuffer::reserve(newSize*sizeof(VECT)); }
    unsigned capacity() const { return WrapBuffer::capacity()/sizeof(VECT); } 
    void clear() { WrapBuffer::clear(); }
    unsigned size() const { return WrapBuffer::size()/sizeof(VECT); } 
    unsigned unusedCapacity() const { return WrapBuffer::unusedCapacity()/sizeof(VECT); }
    unsigned putData(const VECT *data, unsigned num) {
        return WrapBuffer::putData((const void *)data, num*sizeof(VECT));
    }
	
    bool isBufferWrapped() const { return WrapBuffer::isBufferWrapped(); }
	
    /// returns a pointer to the first piece of the data in the ringbuffer, along with its length.  if buffer is empty ptr will be valid but length will be 0
    void dataPtr1(VECT * & ptr, unsigned & lenVecs) const
    {
        void *p; unsigned l;
        WrapBuffer::dataPtr1(p,l);
        ptr = (VECT *)p;
        lenVecs = l/sizeof(VECT);        
    }
    /// returns a pointer to the second piece of the data or NULL pointer if buffer is not wrapped (1 piece)
    void dataPtr2(VECT * & ptr, unsigned & lenVecs) const
    {
        void *p; unsigned l;
        WrapBuffer::dataPtr2(p,l);
        ptr = (VECT *)p;
        lenVecs = l/sizeof(VECT);        
    }
	
    VECT & first() const {
        VECT *p; unsigned l;
        dataPtr1(p,l);
        return *p;
    }
	
    VECT & last() const {
        VECT *p; unsigned l;
        dataPtr2(p,l);
        if (!p || !l) {
            dataPtr1(p,l);
        }
        return p[l-1];
    }
	
	VECT & at(int i) const {
		if (i < 0) i = 0;
		if (i > (int)size()) i = size() - 1;
		VECT *p1, *p2; unsigned l1,l2;
		dataPtr1(p1, l1);
		if (i < (int)l1) return p1[i];
		dataPtr2(p2, l2);
		return p2[i-l1];
	}
	
	VECT & operator[](int i) { return this->at(i); }	
};

/// a class that is a wrapper for WrapBuffer that deals with data items as Vec2s rather than bytes (thus sizes, etc, are in terms of number of Vec2's)
typedef VecWrapBuffer<Vec2> Vec2WrapBuffer;
typedef VecWrapBuffer<Vec2f> Vec2fWrapBuffer;
typedef VecWrapBuffer<Vec2s> Vec2sWrapBuffer;
/// a class that is a wrapper for WrapBuffer that deals with data items as Vec3s rather than bytes (thus sizes, etc, are in terms of number of Vec2's)
typedef VecWrapBuffer<Vec3> Vec3WrapBuffer;
typedef VecWrapBuffer<Vec3f> Vec3fWrapBuffer;

#endif
