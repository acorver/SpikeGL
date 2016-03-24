#ifndef Version_H
#define Version_H

#define VERSION 0x20162403
#ifdef WIN64
#  define VERSION_STR "SpikeGL Win64 v.20162403"
#elif defined(MACX)
#  define VERSION_STR "SpikeGL OSX v.20162403"
#else
#  define VERSION_STR "SpikeGL v.20162403"
#endif


#endif
