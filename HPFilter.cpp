#include "HPFilter.h"
#include <math.h>
#ifndef M_PI
# define M_PI           3.14159265358979323846
#endif
HPFilter::HPFilter(unsigned ssize, double coff)
{
    setScanSize(ssize);
    setCutoffFreqHz(coff);
}

void HPFilter::setScanSize(unsigned ss)
{
    state.clear();
    state.resize(ss, 0.);
    lastDt = 0.;    
    virgin = true;
}


void HPFilter::setCutoffFreqHz(double f)
{
    if (f <= 0.) return; // error!
    cutoffHz = f;
    virgin = true; // force a recompute of coeffs on next call to apply()
}

void HPFilter::apply(short *scan, double dt)
{
    if (dt <= 0.) return; // error!
    if (lastDt != dt || virgin) {
        lastDt = dt;
        recomputeCoeffs();
    }

    const int ss = scanSize();
    for (int i = 0; i < ss; ++i) {
            const double in = static_cast<double>(scan[i])/32768.;
            state[i] = B * in + A * state[i];            
            double out =  in - state[i];            
            if (out > 1.0) out = 1.0;
            else if (out < -1.0) out = -1.0;
            scan[i] = static_cast<short>(out*32767.);
    }
    virgin = false;
}


void HPFilter::recomputeCoeffs()
{
    A = exp(-2.0 * M_PI * cutoffHz * lastDt);
    B = 1.0 - A;
}
