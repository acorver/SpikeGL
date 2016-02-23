#ifndef Version_H
#define Version_H

#define VERSION 0x20162302
#ifdef WIN64
#  define VERSION_STR "SpikeGL Win64 v.20162302"
#elif defined(MACX)
#  define VERSION_STR "SpikeGL OSX v.20162302"
#else
#  define VERSION_STR "SpikeGL v.20162302"
#endif


#endif
