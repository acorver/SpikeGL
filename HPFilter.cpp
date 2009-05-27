#include "HPFilter.h"
#include <math.h>

HPFilter::HPFilter(unsigned ssize, double coff, double b)
    : bw(b)
{
    setScanSize(ssize);
    setCutoffFreqHz(coff);
}

void HPFilter::setScanSize(unsigned ss)
{
    prevIn[0].clear();
    prevIn[0].resize(ss, 0);
    prevIn[1] = prevIn[0];
    prevOut[0].clear();
    prevOut[0].resize(ss, 0);
    prevOut[1] = prevOut[0];
    lastDt = 0.;
    a0 = a1 = a2 = a3 = a4 = 0.;
    virgin = true;
}


void HPFilter::setCutoffFreqHz(double f)
{
    if (f <= 0.) return; // error!
    cutoffHz = f;
    lastDt = 0.f;  // force a recompute of coeffs on next call to apply()
}

void HPFilter::setBandwidth(double b)
{
    bw = b;
    lastDt = 0.f;  // force a recompute of coeffs on next call to apply()
}

void HPFilter::apply(short *scan, double dt)
{
    if (dt <= 0.) return; // error!
    if (lastDt != dt || virgin) {
        lastDt = dt;
        recomputeCoeffs();
    }

    const int ss = scanSize();
    if (virgin) {
        for (int i = 0; i < ss; ++i) {
            const double in = static_cast<double>(scan[i])/32768.;
            prevOut[0][i] = prevIn[0][i] = in;
            prevOut[1][i] = prevIn[1][i] = in;
        }
        virgin = false;
    } else {
        for (int i = 0; i < ss; ++i) {
            // out(n) = a1 * in + a2 * in(n-1)  + a3 * in(n-2) -  b1*out(n-1) - b2*out(n-2)
            const double in = static_cast<double>(scan[i])/32768.;
            double out =  a0 * in + a1 * prevIn[0][i] + a2 * prevIn[1][i] -
        a3 * prevOut[0][i] - a4 * prevOut[1][i]; 
            if (out > 1.0) out = 1.0;
            else if (out < -1.0) out = -1.0;
            prevIn[1][i] = prevIn[0][i];
            prevIn[0][i] = in;
            prevOut[1][i] = prevOut[0][i];
            prevOut[0][i] = out;
            scan[i] = static_cast<short>(out*32767.);
        }
    }
}


void HPFilter::recomputeCoeffs()
{
    double omega, sn, cs, alpha;
    double aa0, aa1, aa2, b0, b1, b2;
    const double & freq = cutoffHz;
    const double & bandwidth = bw;
    const double srate = 1.0/lastDt;

    /* setup variables */
    omega = 2 * M_PI * freq /srate;
    sn = sin(omega);
    cs = cos(omega);
    alpha = sn * sinh(M_LN2 /2. * bandwidth * omega /sn);

    b0 = (1 + cs) /2.;
    b1 = -(1 + cs);
    b2 = (1 + cs) /2.;
    aa0 = 1 + alpha;
    aa1 = -2 * cs;
    aa2 = 1 - alpha;

    /* precompute the coefficients */
    a0 = b0 /aa0;
    a1 = b1 /aa0;
    a2 = b2 /aa0;
    a3 = aa1 /aa0;
    a4 = aa2 /aa0;
}
