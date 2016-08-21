#ifndef Version_H
#define Version_H

#define VERSION 0x20160821
#ifdef WIN64
#  define VERSION_STR "SpikeGL Win64 v.20160821"
#elif defined(MACX)
#  define VERSION_STR "SpikeGL OSX v.20160821"
#else
#  define VERSION_STR "SpikeGL v.20160821"
#endif


#endif
