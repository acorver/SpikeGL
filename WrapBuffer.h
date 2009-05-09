#ifndef WrapBuffer_H
#define WrapBuffer_H

/// essentially a ring-buffer but it clobbers old data as you add new data.
class WrapBuffer
{
public:
    WrapBuffer(unsigned size = 0); // initializes a wrapbuffer of size 0!
    ~WrapBuffer();

    /// invalidates (clears) old data if reserve is called!
    void reserve(unsigned newSize);
    unsigned capacity() const { return bufsz; } ///< the capacity of the buffer in bytes
    void clear() { head = len = 0; }

    unsigned size() const { return len; } ///< the size in bytes of real valid data in the buffer.. tops off at size() bytes
    unsigned unusedCapacity() const { return capacity() - size(); }
    /// insert data into buffer, wrapping and possibly overwriting old data.
    /// returns sizeOfData(), basically, which may be less than nBytes
    /// if nBytes > size()
    unsigned putData(const void *data, unsigned nBytes);

    bool isBufferWrapped() const { return head+len > (int)capacity(); }

    /// returns a pointer to the first piece of the data in the ringbuffer, along with its length.  if buffer is empty ptr will be valid but length will be 0
    void dataPtr1(void * & ptr, unsigned & lenBytes) const;
    /// returns a pointer to the second piece of the data or NULL pointer if buffer is not wrapped (1 piece)
    void dataPtr2(void * & ptr, unsigned & lenBytes) const;

private:
    char *buf;  
    unsigned bufsz;
    int head, len; ///< head of buffer (first index) and length in bytes
};

#endif
