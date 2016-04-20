#ifndef Version_H
#define Version_H

#define VERSION 0x20162004
#ifdef WIN64
#  define VERSION_STR "SpikeGL Win64 v.20162004"
#elif defined(MACX)
#  define VERSION_STR "SpikeGL OSX v.20162004"
#else
#  define VERSION_STR "SpikeGL v.20162004"
#endif


#endif
