#ifndef PAGEDRINGBUFFER_H
#define PAGEDRINGBUFFER_H

#include <string.h>

#define PAGED_RINGBUFFER_MAGIC 0x4a6ef00d
#define PAGED_RINGBUFFER_DEFAULT_PAGESIZE 65536

class PagedRingBuffer
{
public:
    PagedRingBuffer(void *mem, unsigned long size_bytes, unsigned long page_size = PAGED_RINGBUFFER_DEFAULT_PAGESIZE);

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
class PagedRingBufferWriter : public PagedRingBuffer
{
public:
    PagedRingBufferWriter(void *mem, unsigned long size_bytes, unsigned long page_size = PAGED_RINGBUFFER_DEFAULT_PAGESIZE);
    ~PagedRingBufferWriter();

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

class PagedScanReader : protected PagedRingBuffer
{
public:
    PagedScanReader(unsigned scan_size_samples, unsigned meta_data_size_bytes, void *mem, unsigned long size_bytes, unsigned long page_size = PAGED_RINGBUFFER_DEFAULT_PAGESIZE)
        : PagedRingBuffer(mem, size_bytes, page_size), scan_size_samps(scan_size_samples), meta_data_size_bytes(meta_data_size_bytes)
    {
        if (meta_data_size_bytes > page_size) meta_data_size_bytes = page_size;
        nScansPerPage = ((page_size-meta_data_size_bytes)/(scan_size_samps*sizeof(short)));
        scanCt = scanCtV = 0;
    }

    unsigned metaDataSizeBytes() const { return meta_data_size_bytes; }
    unsigned long long scansRead() const { return scanCt; }
    unsigned long long samplesRead() const { return scanCt * static_cast<unsigned long long>(scan_size_samps); }
    /// includes 'dead' or 'skipped' scans in total (due to overflows)
    unsigned long long scansReadVirtual() const { return scanCtV; }
    /// includes 'dead' or 'skipped' scans in total (due to overflows)
    unsigned long long samplesReadVirtual() const { return scanCtV * static_cast<unsigned long long>(scan_size_samps); }

    const short *next(int *nSkips, void **metaPtr = 0) {
        int sk = 0;
        const short *scans = (short *)nextReadPage(&sk);
        if (nSkips) *nSkips = sk;
        if (scans) {
            scanCtV += static_cast<unsigned long long>(nScansPerPage*(sk+1));
            scanCt += static_cast<unsigned long long>(nScansPerPage);
            if (metaPtr) *metaPtr = const_cast<short *>(scans+(nScansPerPage*scan_size_samps));
        } else if (metaPtr) *metaPtr = 0;
        return scans;
    }

private:
    unsigned scan_size_samps, meta_data_size_bytes;
    unsigned nScansPerPage;
    unsigned long long scanCt, scanCtV;
};

class PagedScanWriter : protected PagedRingBufferWriter
{
public:
    PagedScanWriter(unsigned scan_size_samples, unsigned meta_data_size_bytes, void *mem, unsigned long size_bytes, unsigned long page_size = PAGED_RINGBUFFER_DEFAULT_PAGESIZE)
        : PagedRingBufferWriter(mem, size_bytes, page_size), scan_size_samps(scan_size_samples), scan_size_bytes(scan_size_samples*sizeof(short)), meta_data_size_bytes(meta_data_size_bytes)
    {
        if (meta_data_size_bytes > page_size) meta_data_size_bytes = page_size;
        nScansPerPage = ((page_size-meta_data_size_bytes)/scan_size_bytes);
        nBytesPerPage = nScansPerPage * scan_size_bytes;
        pageOffset = 0;
        scanCt = 0;
        sampleCt = 0;
        currPage = 0;
    }

    ~PagedScanWriter() {
        //commit(); // commented-out because we don't want to commit partial writes...
    }
    /// in samples
    unsigned scanSize() const { return scan_size_samps; }
    unsigned scanSizeBytes() const { return scan_size_bytes; }
    unsigned scansPerPage() const { return nScansPerPage; }
    unsigned long long scansWritten() const { return scanCt; }
    unsigned long long samplesWritten() const { return sampleCt; }

    bool write(const short *scans, unsigned nScans, const void *meta = 0) {
        unsigned scansOff = 0; //in scans
        if (meta_data_size_bytes && (!meta || nScans != nScansPerPage))
            // if we are using metadata mode, we require caller to pass a meta data pointer
            // and we have to write scans in units of 1 page at a time... yes, it sucks, i know.
            return false;
        while (nScans) {
            if (!currPage) { currPage = (short *)grabNextPageForWrite(); pageOffset = 0; }
            unsigned spaceLeft = nScansPerPage - pageOffset;
            if (!spaceLeft) { return false; /* this should *NEVER* be reached!! A safeguard, though, in case of improper use of class and/or too small a pagesize */ }
            unsigned n2write = nScans > spaceLeft ? spaceLeft : nScans;
            memcpy(currPage+(pageOffset*scan_size_samps), scans+(scansOff*scan_size_samps), n2write*scan_size_bytes);
            pageOffset += n2write;
            scansOff += n2write;
            scanCt += static_cast<unsigned long long>(n2write);
            sampleCt += static_cast<unsigned long long>(n2write*scan_size_samps);
            nScans -= n2write;
            spaceLeft -= n2write;

            if (!spaceLeft) {  // if clause here is needed as above block may modify spaceLeft
                if (meta_data_size_bytes)
                    // append metadata to end of each page
                    memcpy(currPage+(pageOffset*scan_size_samps), meta, meta_data_size_bytes);
                commit(); // 'commit' the page
            }
        }
        return true;
    }
protected:
    void commit() {
        if (currPage) {
            commitCurrentWritePage();
            currPage = 0; pageOffset = 0;
        }
    }

private:
    short *currPage;
    unsigned scan_size_samps, scan_size_bytes, meta_data_size_bytes;
    unsigned nScansPerPage, nBytesPerPage, pageOffset /*in scans*/;
    unsigned long long scanCt, sampleCt;
};

#endif // PAGEDRINGBUFFER_H
