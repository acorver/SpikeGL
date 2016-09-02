#ifndef Version_H
#define Version_H

#define VERSION 0x20160902
#ifdef WIN64
#  define VERSION_STR "SpikeGL Win64 v.20160902"
#elif defined(MACX)
#  define VERSION_STR "SpikeGL OSX v.20160902"
#else
#  define VERSION_STR "SpikeGL v.20160902"
#endif


#endif
