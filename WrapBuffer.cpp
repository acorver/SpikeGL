#include "WrapBuffer.h"
#include <string.h>

WrapBuffer::WrapBuffer(unsigned theSize)
    : buf(0), bufsz(0)
{
    reserve(theSize);
}

WrapBuffer::~WrapBuffer()
{
    delete [] buf;
    buf = 0;
}

WrapBuffer::WrapBuffer(const WrapBuffer & rhs)
    : buf(0)
{
    (*this) = rhs;
}

WrapBuffer & WrapBuffer::operator=(const WrapBuffer & rhs)
{
    if (buf) delete [] buf;
    bufsz = rhs.bufsz;
    buf = new char[bufsz];
    len = rhs.len;
    head = rhs.head;
    memcpy(buf, rhs.buf, bufsz);
    return *this;
}

void WrapBuffer::reserve(unsigned newSize)
{
    delete [] buf; buf = 0;
    if (newSize) buf = new char[newSize];
    bufsz = newSize;
    len = head = 0;
}

unsigned WrapBuffer::putData(const void *data, unsigned nBytes)
{
    const int siz = capacity();
    if (nBytes >= unsigned(siz)) {
        int diff = nBytes - siz;
        nBytes = siz;
        head = 0;
        len = nBytes;
        memcpy(&buf[0], ((const char *)data)+diff, nBytes);
    } else { // copying in < size of buffer, but may need to clobber stuff at beginning to make room at end..
        int newSize = len + nBytes;
        if (newSize > siz) { // wrap it, clobbering stuff at head to make room for new data
            int diff = newSize - siz;
            head += diff;
            if (head >= siz) head -= siz;
            len -= diff;
            // now at this point we made room for new data
        } 
        // regular insert at this point, no clobber of beginning
        int lp = head+len; // load ptr
        if (lp >= siz) lp -= siz; // wrap load ptr if off edge
        int bytesLeft = nBytes, bounds = lp+bytesLeft, b2c = bytesLeft;
        if (bounds > siz) {
            b2c = siz - lp;
        }
        memcpy(&buf[lp], data, b2c);
        lp += b2c;
        if (lp >= siz) lp -= siz;  // wrap load ptr
        bytesLeft -= b2c;
        if (bytesLeft > 0) {
            memcpy(&buf[lp], ((const char *)data)+b2c, bytesLeft);
        }
        len += nBytes;
    }
    return size();
}


void WrapBuffer::dataPtr1(void * & ptr, unsigned & lenBytes) const
{
    ptr = (void *)&buf[head];
    int bounds = head+len, siz = capacity();
    if (bounds >= siz) lenBytes = siz-head;
    else     lenBytes = len;
}

void WrapBuffer::dataPtr2(void * & ptr, unsigned & lenBytes) const
{
    if (isBufferWrapped()) {
        ptr =  (void *)&buf[0];
        lenBytes = (head+len) - capacity();
    } else {
        // not wrapped
        ptr = 0;
        lenBytes = 0;
    }
}

#ifdef TEST_WRAP_BUFFER

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    unsigned size;
    if (argc < 3 || sscanf(argv[1], "%u", &size) != 1) {
        fprintf(stderr, "Please pass a buffer size and at least 1 string to put into it!\n");
    }
    WrapBuffer w(size);
    for (int i = 2; i < argc; ++i) {
        char *str = argv[i];
        int len = strlen(argv[i]);
        int newlen = w.putData(str,len);
        void *p1, *p2;
        unsigned l1, l2;
        w.dataPtr1(p1, l1);
        w.dataPtr2(p2, l2);
        char *s1 = (char *)calloc(l1+1, 1);
        memcpy(s1, p1, l1);
        char *s2 = (char *)calloc(l2+1, 1);
        if (p2) {
            memcpy(s2, p2, l2);
        }
        printf("Iter %d:\n  "
               "Inserted string: '%s' of len %d\n"
               "Buffer ptr(s): 1: '%s' of len %u  2: '%s' of len %u  newlen: %d\n\n",
               i-2,str,len,s1,l1,s2?s2:"",l2,newlen);
        free(s1), s1 = 0; free(s2), s2 = 0;
    }
    return 0;
}

#endif

