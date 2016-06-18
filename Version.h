#ifndef Version_H
#define Version_H

#define VERSION 0x20161806
#ifdef WIN64
#  define VERSION_STR "SpikeGL Win64 v.20161806"
#elif defined(MACX)
#  define VERSION_STR "SpikeGL OSX v.20161806"
#else
#  define VERSION_STR "SpikeGL v.20161806"
#endif


#endif
