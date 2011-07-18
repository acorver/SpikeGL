#include <stdio.h>
#include <iostream>
#include <string>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>

static void printUsage() {
    std::cerr << "Usage: gentestdata [-c nchans] [-n nscans]\n";
}


int main(int argc, char *argv[]) {
    const float srate = 29630;
    unsigned long nchans = 60, nscans = 1000000, pd = 0;
    int ret;
    bool errFlag = false;

    while ( (ret = getopt(argc, argv, "c:n:p:")) > -1 ) {
        switch (ret) {
        case 'c': nchans = atoi(optarg); break;
        case 'n': nscans = atoi(optarg); break;
        case 'p': pd = atoi(optarg); break;
        case '?': errFlag = true; break;
        }
        if (!nchans || !nscans || errFlag) {
            printUsage();
            exit(1);
        }
    }
    unsigned long s, c;    
    for (s = 0; s < nscans; ++s) {
        for (c = 0; c < nchans; ++c) {
            short datum = 0;
            if (c % 2) { // odd ch# == square
                datum = sin((s/srate)*M_PI*(c*3)) > 0.f ? 32767 : -32767;
            } else { // even ch# == sinusoidal
                datum = short(sin((s/srate)*M_PI*(c*3)) * 32767.f);
            }
            if (pd && c == pd) {
                // pd chan -- square wave of period ~ 1s?
                datum = sin((s/srate)*M_PI) > 0.f ? -32768 : 32767;
            }
            datum = short(datum*0.75f);
            if ( fwrite(&datum, sizeof(datum), 1, stdout) != 1 ) {
                perror("fwrite");
                exit(1);
            }
        }
    }
    fclose(stdout);
    return 0;
}

