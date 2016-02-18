#include "stdafx.h"
#include "PagedRingBuffer.h"
#include <string.h>

PagedRingBuffer::PagedRingBuffer(void *m, unsigned long sz, unsigned long psz)
    : memBuffer(m), mem(reinterpret_cast<char *>(m)+sizeof(unsigned int)), real_size_bytes(sz), avail_size_bytes(sz-sizeof(unsigned int)), page_size(psz)
{
    resetToBeginning();
}

void PagedRingBuffer::resetToBeginning()
{
    lastPageRead = 0; pageIdx = -1;
    npages = avail_size_bytes/(page_size + sizeof(Header));
    if (page_size > avail_size_bytes || !page_size || !avail_size_bytes || !npages || !real_size_bytes || avail_size_bytes > real_size_bytes) {
        memBuffer = 0; mem = 0; page_size = 0; avail_size_bytes = 0; npages = 0; real_size_bytes = 0;
    }
}

void *PagedRingBuffer::getCurrentReadPage()
{
    if (!mem || !npages || !avail_size_bytes) return 0;
    if (pageIdx < 0 || pageIdx >= (int)npages) return 0;
    Header *h = reinterpret_cast<Header *>(&mem[ (page_size+sizeof(Header)) * pageIdx ]);
    Header hdr; memcpy(&hdr, h, sizeof(hdr)); // hopefully avoid some potential race conditions
    if (hdr.magic != unsigned(PAGED_RINGBUFFER_MAGIC) || hdr.pageNum != lastPageRead) return 0;
    return reinterpret_cast<char *>(h)+sizeof(Header);
}

void *PagedRingBuffer::nextReadPage(int *nSkips)
{
    if (!mem || !npages || !avail_size_bytes) return 0;
    int nxt = (pageIdx+1) % npages;
    if (nxt < 0) nxt = 0;
    Header *h = reinterpret_cast<Header *>(&mem[ (page_size+sizeof(Header)) * nxt ]);
    Header hdr; memcpy(&hdr, h, sizeof(hdr)); // hopefully avoid some potential race conditions
    if (hdr.magic == unsigned(PAGED_RINGBUFFER_MAGIC) && hdr.pageNum >= lastPageRead+1U) {
        if (nSkips) *nSkips = int(hdr.pageNum-(lastPageRead+1)); // record number of overflows/lost pages here!
        lastPageRead = hdr.pageNum;
        pageIdx = nxt;
        return reinterpret_cast<char *>(h)+sizeof(Header);
    }
    // if we get to this point, a new read page isn't 'ready' yet!
    if (nSkips) *nSkips = 0;
    return 0;
}

void PagedRingBuffer::bzero() {
    if (mem && avail_size_bytes) memset(mem, 0, avail_size_bytes);
}

PagedRingBufferWriter::PagedRingBufferWriter(void *mem, unsigned long sz, unsigned long psz)
    : PagedRingBuffer(mem, sz, psz)
{
    lastPageWritten = 0;
    nWritten = 0;
}

PagedRingBufferWriter::~PagedRingBufferWriter() {}

void PagedRingBufferWriter::initializeForWriting()
{
    bzero();
    pageIdx = -1;
    nWritten = 0;
    lastPageWritten = 0;
}

void *PagedRingBufferWriter::grabNextPageForWrite()
{
    if (!mem || !npages || !avail_size_bytes) return 0;
    int nxt = (pageIdx+1) % npages;
    if (nxt < 0) nxt = 0;
    Header *h = reinterpret_cast<Header *>(&mem[ (page_size+sizeof(Header)) * nxt ]);
    h->magic = 0; h->pageNum = 0;
    pageIdx = nxt;
    return reinterpret_cast<char *>(h)+sizeof(Header);
}

bool PagedRingBufferWriter::commitCurrentWritePage()
{
    if (!mem || !npages || !avail_size_bytes) return false;
    int pg = pageIdx % npages;
    if (pg < 0) pg = 0;
    Header *h = reinterpret_cast<Header *>(&mem[ (page_size+sizeof(Header)) * pg ]);
    pageIdx = pg;
    h->pageNum = *latestPNum = ++lastPageWritten;
    h->magic = (unsigned)PAGED_RINGBUFFER_MAGIC;
    ++nWritten;
    return true;
}

PagedScanReader::PagedScanReader(unsigned scan_size_samples, unsigned meta_data_size_bytes, void *mem, unsigned long size_bytes, unsigned long page_size)
    : PagedRingBuffer(mem, size_bytes, page_size), scan_size_samps(scan_size_samples), meta_data_size_bytes(meta_data_size_bytes)
{
    if (meta_data_size_bytes > page_size) meta_data_size_bytes = page_size;
    nScansPerPage = scan_size_samps ? ((page_size-meta_data_size_bytes)/(scan_size_samps*sizeof(short))) : 0;
    scanCt = scanCtV = 0;
}

PagedScanReader::PagedScanReader(const PagedScanReader &o)
    : PagedRingBuffer(o.rawData(), o.totalSize(), o.pageSize()), scan_size_samps(o.scanSizeSamps()), meta_data_size_bytes(o.meta_data_size_bytes)
{
    if (meta_data_size_bytes > page_size) meta_data_size_bytes = page_size;
    nScansPerPage = scan_size_samps ? ((page_size-meta_data_size_bytes)/(scan_size_samps*sizeof(short))) : 0;
    scanCt = scanCtV = 0;
}

PagedScanReader::PagedScanReader(const PagedScanWriter &o)
    : PagedRingBuffer(o.rawData(), o.totalSize(), o.pageSize()), scan_size_samps(o.scanSizeSamps()), meta_data_size_bytes(o.metaDataSizeBytes())
{
    if (meta_data_size_bytes > page_size) meta_data_size_bytes = page_size;
    nScansPerPage = scan_size_samps ? ((page_size-meta_data_size_bytes)/(scan_size_samps*sizeof(short))) : 0;
    scanCt = scanCtV = 0;
}


const short *PagedScanReader::next(int *nSkips, void **metaPtr, unsigned *scans_returned)
{
    int sk = 0;
    const short *scans = (short *)nextReadPage(&sk);
    if (nSkips) *nSkips = sk;
    if (scans_returned) *scans_returned = 0;
    if (metaPtr) *metaPtr = 0;
    if (scans) {
        if (scans_returned) *scans_returned = nScansPerPage;
        scanCtV += static_cast<unsigned long long>(nScansPerPage*(sk+1));
        scanCt += static_cast<unsigned long long>(nScansPerPage);
        if (metaPtr && meta_data_size_bytes)
            *metaPtr = const_cast<short *>(scans+(nScansPerPage*scan_size_samps));
    }
    return scans;
}


PagedScanWriter::PagedScanWriter(unsigned scan_size_samples, unsigned meta_data_size_bytes, void *mem, unsigned long size_bytes, unsigned long page_size)
    : PagedRingBufferWriter(mem, size_bytes, page_size), scan_size_samps(scan_size_samples), scan_size_bytes(scan_size_samples*sizeof(short)), meta_data_size_bytes(meta_data_size_bytes)
{
    if (meta_data_size_bytes > page_size) meta_data_size_bytes = page_size;
    nScansPerPage = scan_size_bytes ? ((page_size-meta_data_size_bytes)/scan_size_bytes) : 0;
    nBytesPerPage = nScansPerPage * scan_size_bytes;
    pageOffset = 0; partial_offset = 0; partial_bytes_written = 0;
    scanCt = 0;
    sampleCt = 0;
    currPage = 0;
}

void PagedScanWriter::writePartialBegin()
{
    partial_offset = pageOffset*scan_size_bytes; 
    partial_bytes_written = 0;
}
bool PagedScanWriter::writePartialEnd()
{
    bool ret = !(partial_offset % scan_size_bytes); // should always be aligned.  if not, return false and indicate to caller something is off.

    pageOffset = partial_offset / scan_size_bytes;
    scanCt += static_cast<unsigned long long>(partial_bytes_written / scan_size_bytes);
    sampleCt += static_cast<unsigned long long>(partial_bytes_written / sizeof(short));
    partial_bytes_written = 0; // guard against shitty use of class

    if (pageOffset > nScansPerPage) return false; // should never happen.  indicates bug in this code.
    if (pageOffset == nScansPerPage) commit();  // should never happen!
    return ret;
}
bool PagedScanWriter::writePartial(const void *data, unsigned nbytes)
{
    unsigned dataOffset = 0;
    while (nbytes) {
        if (!currPage) { currPage = (short *)grabNextPageForWrite(); pageOffset = 0; partial_offset = 0; }
        unsigned spaceLeft = (nScansPerPage*scan_size_bytes) - partial_offset;
        if (!spaceLeft) return false; // should not be reached.. a safeguard in case of improper pagesize of improper use of class
        unsigned n2write = nbytes > spaceLeft ? spaceLeft : nbytes;
        memcpy(reinterpret_cast<char *>(currPage)+partial_offset, reinterpret_cast<const char *>(data)+dataOffset, n2write);
        dataOffset += n2write;
        partial_offset += n2write;
        partial_bytes_written += n2write;
        nbytes -= n2write;
        spaceLeft -= n2write;

        if (!spaceLeft) commit();
    }
    return true;
}

bool PagedScanWriter::write(const short *scans, unsigned nScans, const void *meta) {
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
        partial_offset = pageOffset * scan_size_bytes;
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

void PagedScanWriter::commit()
{
    if (currPage) {
        commitCurrentWritePage();
        currPage = 0; pageOffset = 0; partial_offset = 0;
    }
}

