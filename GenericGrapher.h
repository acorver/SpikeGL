#ifndef GENERIC_GRAPHER_H
#define GENERIC_GRAPHER_H
#include "TypeDefs.h"
#include <vector>

class GenericGrapher {
public:
    GenericGrapher() {}
    virtual ~GenericGrapher() {}

    void putScans(const std::vector<int16> & scans, u64 firstSamp) {
        if (scans.size()) putScans(&scans[0], unsigned(scans.size()), firstSamp);
    }

    virtual void putScans(const int16 *scans, unsigned scans_size_samps, u64 firstSamp) = 0;
    virtual bool threadsafeIsVisible() const = 0;
    virtual bool caresAboutSkippedScans() const = 0;
    virtual const char *grapherName() const  = 0;
};

#endif // GENERIC_GRAPHER_H
