#ifndef Version_H
#define Version_H

#define VERSION 0x20162803
#ifdef WIN64
#  define VERSION_STR "SpikeGL Win64 v.20162803"
#elif defined(MACX)
#  define VERSION_STR "SpikeGL OSX v.20162803"
#else
#  define VERSION_STR "SpikeGL v.20162803"
#endif


#endif
