#ifndef HPFilter_H
#define HPFilter_H

#include <vector>

/// A high-pass filter.  Feed it a scan at a time and have it 
/// high-pass filter based on the cutoff freq.
/// This is basically a biquad filter.
class HPFilter
{
public:
    HPFilter(unsigned scanSize, double cutoffHz, double bandwidth = 2.0);
    void setCutoffFreqHz(double fHz);
    double cutoffFreqHz() const { return cutoffHz; }
    
    void setScanSize(unsigned);
    unsigned scanSize() const { return prevIn[0].size(); }

    void setBandwidth(double b);
    double bandwidth() const { return bw; }
    

    /// Convolves the scan in-place using the filter
    void apply(short *scan, double dt);

private:
    void recomputeCoeffs();

    double cutoffHz, bw, lastDt, a0, a1, a2, a3, a4;
    std::vector<double> prevIn[2], prevOut[2];
    bool virgin;
    
};

#endif
