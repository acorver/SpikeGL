#ifndef Version_H
#define Version_H

#define VERSION 0x20161606
#ifdef WIN64
#  define VERSION_STR "SpikeGL Win64 v.20161606"
#elif defined(MACX)
#  define VERSION_STR "SpikeGL OSX v.20161606"
#else
#  define VERSION_STR "SpikeGL v.20161606"
#endif


#endif
