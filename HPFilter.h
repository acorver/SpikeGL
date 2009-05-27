#ifndef HPFilter_H
#define HPFilter_H

#include <vector>

/// A high-pass filter.  Feed it a scan at a time and have it 
/// high-pass filter based on the cutoff freq.
/// This is basically a biquad filter.
class HPFilter
{
public:
    HPFilter(unsigned scanSize, double cutoffHz);
    void setCutoffFreqHz(double fHz);
    double cutoffFreqHz() const { return cutoffHz; }
    
    void setScanSize(unsigned);
    unsigned scanSize() const { return state.size(); }

    /// Convolves the scan in-place using the filter
    void apply(short *scan, double dt);

private:
    void recomputeCoeffs();

    double cutoffHz, lastDt, A, B;
    std::vector<double> state;
    bool virgin;
    
};

#endif
