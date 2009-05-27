#ifndef HPFilter_H
#define HPFilter_H

#include <vector>

/// A high-pass filter.  Feed it a scan at a time and have it 
/// high-pass filter based on the cutoff freq.
/// This is basically a biquad, tweaked butterworth filter.
class HPFilter
{
public:
    HPFilter(unsigned scanSize = 1, double cutoffHz = 300.0, double resonance_amount = /*1.0*/ /*1.414*/0.1);
    void setCutoffFreqHz(double fHz);
    double cutoffFreqHz() const { return cutoffHz; }
    
    void setResonance(double r);
    double resonance() const { return rez; }

    void setScanSize(unsigned);
    unsigned scanSize() const { return prevIn[0].size(); }

    /// Convolves the scan in-place using the filter
    void apply(short *scan, double dt);

private:
    double cutoffHz, rez, lastDt, c, a1, a2, a3, b1, b2;
    std::vector<double> prevIn[2], prevOut[2];
    bool virgin;
    
};

#endif
