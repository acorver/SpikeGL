#ifndef Vec2WrapBuffer_H
#define Vec2WrapBuffer_H

#include "WrapBuffer.h"
#include "Vec2.h"

/// a class that is a wrapper for WrapBuffer that deals with data items as Vec2s rather than bytes (thus sizes, etc, are in terms of number of Vec2's)
class Vec2WrapBuffer : protected WrapBuffer
{
public:
    Vec2WrapBuffer(unsigned nVec2sForSize = 0)
        : WrapBuffer(nVec2sForSize*sizeof(Vec2)) {}

    void reserve(unsigned newSize) { return WrapBuffer::reserve(newSize*sizeof(Vec2)); }
    unsigned capacity() const { return WrapBuffer::capacity()/sizeof(Vec2); } 
    void clear() { WrapBuffer::clear(); }
    unsigned size() const { return WrapBuffer::size()/sizeof(Vec2); } 
    unsigned unusedCapacity() const { return WrapBuffer::unusedCapacity()/sizeof(Vec2); }
    unsigned putData(const Vec2 *data, unsigned num) {
        return WrapBuffer::putData((const void *)data, num*sizeof(Vec2));
    }

    bool isBufferWrapped() const { return WrapBuffer::isBufferWrapped(); }

    /// returns a pointer to the first piece of the data in the ringbuffer, along with its length.  if buffer is empty ptr will be valid but length will be 0
    void dataPtr1(Vec2 * & ptr, unsigned & lenVecs) const
    {
        void *p; unsigned l;
        WrapBuffer::dataPtr1(p,l);
        ptr = (Vec2 *)p;
        lenVecs = l/sizeof(Vec2);        
    }
    /// returns a pointer to the second piece of the data or NULL pointer if buffer is not wrapped (1 piece)
    void dataPtr2(Vec2 * & ptr, unsigned & lenVecs) const
    {
        void *p; unsigned l;
        WrapBuffer::dataPtr2(p,l);
        ptr = (Vec2 *)p;
        lenVecs = l/sizeof(Vec2);        
    }

    Vec2 & first() const {
        Vec2 *p; unsigned l;
        dataPtr1(p,l);
        return *p;
    }

    Vec2 & last() const {
        Vec2 *p; unsigned l;
        dataPtr2(p,l);
        if (!p || !l) {
            dataPtr1(p,l);
        }
        return p[l-1];
    }
	
	Vec2 & at(int i) const {
		if (i < 0) i = 0;
		if (i > (int)size()) i = size() - 1;
		Vec2 *p1, *p2; unsigned l1,l2;
		dataPtr1(p1, l1);
		if (i < (int)l1) return p1[i];
		dataPtr2(p2, l2);
		return p2[i-l1];
	}
	
	Vec2 & operator[](int i) { return this->at(i); }
};

#endif
