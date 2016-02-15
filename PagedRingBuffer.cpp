#include "stdafx.h"
#include "PagedRingBuffer.h"
#include <string.h>

PagedRingBuffer::PagedRingBuffer(void *m, unsigned long sz, unsigned long psz)
    : mem(reinterpret_cast<char *>(m)), size_bytes(sz), page_size(psz)
{
    resetToBeginning();
}

void PagedRingBuffer::resetToBeginning()
{
    lastPageRead = 0; pageIdx = -1;
    npages = size_bytes/(page_size + sizeof(Header));
    if (page_size > size_bytes || !page_size || !size_bytes || !npages) {
        mem = 0; page_size = 0; size_bytes = 0; npages = 0;
    }
}

void *PagedRingBuffer::getCurrentReadPage()
{
    if (!mem || !npages || !size_bytes) return 0;
    if (pageIdx < 0 || pageIdx >= (int)npages) return 0;
    Header *h = reinterpret_cast<Header *>(&mem[ (page_size+sizeof(Header)) * pageIdx ]);
    if (h->magic != unsigned(PAGED_RINGBUFFER_MAGIC) || h->pageNum != lastPageRead) return 0;
    return reinterpret_cast<char *>(h)+sizeof(Header);
}

void *PagedRingBuffer::nextReadPage(int *nSkips)
{
    if (!mem || !npages || !size_bytes) return 0;
    int nxt = (pageIdx+1) % npages;
    if (nxt < 0) nxt = 0;
    Header *h = reinterpret_cast<Header *>(&mem[ (page_size+sizeof(Header)) * nxt ]);
    if (h->magic == unsigned(PAGED_RINGBUFFER_MAGIC) && h->pageNum >= lastPageRead+1U) {
        if (nSkips) *nSkips = int(h->pageNum-(lastPageRead+1)); // record number of overflows/lost pages here!
        lastPageRead = h->pageNum;
        pageIdx = nxt;
        return reinterpret_cast<char *>(h)+sizeof(Header);
    }
    // if we get to this point, a new read page isn't 'ready' yet!
    if (nSkips) *nSkips = 0;
    return 0;
}

void PagedRingBuffer::bzero() {
    if (mem && size_bytes) memset(mem, 0, size_bytes);
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
    if (!mem || !npages || !size_bytes) return 0;
    int nxt = (pageIdx+1) % npages;
    if (nxt < 0) nxt = 0;
    Header *h = reinterpret_cast<Header *>(&mem[ (page_size+sizeof(Header)) * nxt ]);
    h->magic = 0; h->pageNum = 0;
    pageIdx = nxt;
    return reinterpret_cast<char *>(h)+sizeof(Header);
}

bool PagedRingBufferWriter::commitCurrentWritePage()
{
    if (!mem || !npages || !size_bytes) return false;
    int pg = pageIdx % npages;
    if (pg < 0) pg = 0;
    Header *h = reinterpret_cast<Header *>(&mem[ (page_size+sizeof(Header)) * pg ]);
    pageIdx = pg;
    h->pageNum = ++lastPageWritten;
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
    pageOffset = 0;
    scanCt = 0;
    sampleCt = 0;
    currPage = 0;
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
        currPage = 0; pageOffset = 0;
    }
}

