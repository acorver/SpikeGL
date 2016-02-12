#include "PagedRingbuffer.h"
#include <string.h>

PagedRingbuffer::PagedRingbuffer(void *m, unsigned long sz, unsigned long psz)
    : mem(reinterpret_cast<char *>(m)), size_bytes(sz), page_size(psz)
{
    lastPageRead = 0; pageIdx = -1;
    npages = size_bytes/(page_size + sizeof(Header));
    if (page_size > size_bytes || !page_size || !size_bytes || !npages) {
        mem = 0; page_size = 0; size_bytes = 0; npages = 0;
    }
}

void *PagedRingbuffer::getCurrentReadPage()
{
    if (!mem || !npages || !size_bytes) return 0;
    if (pageIdx < 0 || pageIdx >= (int)npages) return 0;
    Header *h = reinterpret_cast<Header *>(&mem[ (page_size+sizeof(Header)) * pageIdx ]);
    if (h->magic != unsigned(PAGED_RINGBUFFER_MAGIC) || h->pageNum != lastPageRead) return 0;
    return reinterpret_cast<char *>(h)+sizeof(Header);
}

void *PagedRingbuffer::nextReadPage(int *nSkips)
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



PagedRingbufferWriter::PagedRingbufferWriter(void *mem, unsigned long sz, unsigned long psz)
    : PagedRingbuffer(mem, sz, psz)
{
    lastPageWritten = 0;
    nWritten = 0;
}

PagedRingbufferWriter::~PagedRingbufferWriter() {}

void PagedRingbufferWriter::initializeForWriting()
{
    if (mem && size_bytes) memset(mem, 0, size_bytes);
    pageIdx = -1;
    nWritten = 0;
    lastPageWritten = 0;
}

void *PagedRingbufferWriter::grabNextPageForWrite()
{
    if (!mem || !npages || !size_bytes) return 0;
    int nxt = (pageIdx+1) % npages;
    if (nxt < 0) nxt = 0;
    Header *h = reinterpret_cast<Header *>(&mem[ (page_size+sizeof(Header)) * nxt ]);
    h->magic = 0; h->pageNum = 0;
    pageIdx = nxt;
    return reinterpret_cast<char *>(h)+sizeof(Header);
}

bool PagedRingbufferWriter::commitCurrentWritePage()
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

