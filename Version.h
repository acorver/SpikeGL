#ifndef Version_H
#define Version_H

#define VERSION 0x20161104
#ifdef WIN64
#  define VERSION_STR "SpikeGL Win64 v.20161104"
#elif defined(MACX)
#  define VERSION_STR "SpikeGL OSX v.20161104"
#else
#  define VERSION_STR "SpikeGL v.20161104"
#endif


#endif
