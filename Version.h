#ifndef Version_H
#define Version_H

#define VERSION 0x20160712
#ifdef WIN64
#  define VERSION_STR "SpikeGL Win64 v.20160712"
#elif defined(MACX)
#  define VERSION_STR "SpikeGL OSX v.20160712"
#else
#  define VERSION_STR "SpikeGL v.20160712"
#endif


#endif
