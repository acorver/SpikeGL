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
    unsigned scanSize() const { return unsigned(state.size()); }

    /// Convolves the scan in-place using the filter
    void apply(short *scan, double dt);

	/// Same as above, but supports convolving only a subset of channels in the scan.
	/// Note that which_chans should be the same size as scanSize().  It specifies
	/// which channels in the scan array to filter (which_chans[i] == true -> filter).
	/// (NB: filter state is always updated for each chan even if filtering isn't applied)
	void apply(short *scan, double dt, const std::vector<bool> & which_chans);

private:
    void recomputeCoeffs();

    double cutoffHz, lastDt, A, B;
    std::vector<double> state;
    bool virgin;
    
};

#endif
