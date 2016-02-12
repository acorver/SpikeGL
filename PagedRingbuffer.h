#ifndef PAGEDRINGBUFFER_H
#define PAGEDRINGBUFFER_H

#define PAGED_RINGBUFFER_MAGIC 0x4a6ef00d
#define PAGED_RINGBUFFER_DEFAULT_PAGESIZE 65536

class PagedRingbuffer
{
public:
    PagedRingbuffer(void *mem, unsigned long size_bytes, unsigned long page_size = PAGED_RINGBUFFER_DEFAULT_PAGESIZE);

    unsigned long pageSize() const { return page_size; }
    unsigned long totalSize() const { return size_bytes; }
    unsigned int nPages() const { return npages; }

    void resetToBeginning();

    void *getCurrentReadPage();
    /// returns NULL when a new read page isn't 'ready' yet.  nSkips is the number of pages dropped due to overflows.  Normally should be 0.
    void *nextReadPage(int *nSkips = 0);

    /// clear the contents to 0.
    void bzero();

protected:
    char *mem;
    unsigned long size_bytes, page_size;
    unsigned int npages, lastPageRead;
    int pageIdx;

    struct Header {
        volatile unsigned int magic;
        volatile unsigned int pageNum;
    };
};


/// Page lifecycle for writing is:
///  1. void * page = grabNextPageForWrite()
///  2. do stuff with the page (write to it)
///  3. call commitCurrentWritePage() to indicate it now has valid data
///  4. repeat 1-3 above for next page
class PagedRingbufferWriter : public PagedRingbuffer
{
public:
    PagedRingbufferWriter(void *mem, unsigned long size_bytes, unsigned long page_size = PAGED_RINGBUFFER_DEFAULT_PAGESIZE);
    ~PagedRingbufferWriter();

    unsigned long nPagesWritten() const { return nWritten; }

    /// grab the next write page.
    /// note that grabbing more than 1 page at a time will result in an error.
    /// Grab the page, write to it, then commit it sometime later before grabbing
    /// another one.
    void *grabNextPageForWrite();
    /// Updates the magic header info so that the current write page can now
    /// be effectively written by reading processes/tasks.  Call this after
    /// being done with a page returned from grabNextPageForWrite(), and before
    /// calling grabNextPageForWrite() again after being done with the page.
    bool commitCurrentWritePage();

    void initializeForWriting(); ///< generally, call this before first writing to the buffer to clear it to 0

private:
    unsigned long nWritten;
    unsigned int lastPageWritten;

};

#endif // PAGEDRINGBUFFER_H
