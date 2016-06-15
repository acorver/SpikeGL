#ifndef Version_H
#define Version_H

#define VERSION 0x20161506
#ifdef WIN64
#  define VERSION_STR "SpikeGL Win64 v.20161506"
#elif defined(MACX)
#  define VERSION_STR "SpikeGL OSX v.20161506"
#else
#  define VERSION_STR "SpikeGL v.20161506"
#endif


#endif
