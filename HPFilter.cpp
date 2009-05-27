#include "HPFilter.h"
#include <math.h>

HPFilter::HPFilter(unsigned ssize, double coff, double rez)
    : rez(rez)
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
    a1 = a2 = a3 = b1 = b2 = 0.;
    virgin = true;
}


void HPFilter::setCutoffFreqHz(double f)
{
    if (f <= 0.) return; // error!
    cutoffHz = f;
    lastDt = 0.f; // force a recompute of c, a1, a2, a3, etc on next call to apply()
}

void HPFilter::setResonance(double r) 
{ 
    rez = r; 
    lastDt = 0.f; // force a recompute of c, a1, a2, a3, etc on next call to apply()
}

/*
From: www.musicdsp.org
Type : biquad, tweaked butterworth
References : Posted by Patrice Tarrabia
Code :
r = rez amount, from sqrt(2) to ~ 0.1
f = cutoff frequency
(from ~0 Hz to SampleRate/2 - though many
synths seem to filter only up to SampleRate/4)

The filter algo:
out(n) = a1 * in + a2 * in(n-1) + a3 * in(n-2) - b1*out(n-1) - b2*out(n-2)

Lowpass:
c = 1.0 / tan(pi * f / sample_rate);

a1 = 1.0 / ( 1.0 + r * c + c * c);
a2 = 2* a1;
a3 = a1;
b1 = 2.0 * ( 1.0 - c*c) * a1;
b2 = ( 1.0 - r * c + c * c) * a1;

Hipass:
c = tan(pi * f / sample_rate);

a1 = 1.0 / ( 1.0 + r * c + c * c);
a2 = -2*a1;
a3 = a1;
b1 = 2.0 * ( c*c - 1.0) * a1;
b2 = ( 1.0 - r * c + c * c) * a1;
*/
void HPFilter::apply(short *scan, double dt)
{
    if (dt <= 0.) return; // error!
    if (lastDt != dt || virgin) {
        lastDt = dt;
        c = tan(M_PI * cutoffHz * dt);
        a1 = 1.0 / ( 1.0 + rez*c + c*c );
        a2 = -2.*a1;
        a3 = a1;
        b1 = 2.0 * ( c*c - 1.0 ) * a1;
        b2 = ( 1.0 - rez*c + c*c ) * a1;
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
            double out = a1 * in + a2 * prevIn[0][i]  + a3 * prevIn[1][i] -  b1*prevOut[0][i] - b2*prevOut[1][i];
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

