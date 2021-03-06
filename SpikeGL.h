#ifndef SpikeGL_H
#define SpikeGL_H

/**
   @file SpikeGL.h - some constants and other shared values.
*/

#include "Util.h"
#include "TypeDefs.h"
#include "Version.h"

#define INTAN_SRATE 29630
#define DAQ_TIMEOUT 2.5
#define DEFAULT_FAST_SETTLE_TIME_MS 15
#define LOCK_TIMEOUT_MS 2000
#define DEF_TASK_READ_FREQ_HZ_ 20
#define DEF_AI_BUFFER_SIZE_CENTISECONDS 9
#define DEF_AO_BUFFER_SIZE_CENTISECONDS 9
#define DEF_TASK_READ_FREQ_HZ Util::getTaskReadFreqHz()
#define TASK_WRITE_FREQ_HZ 10
#define APPNAME "SpikeGL"
#define DOWNSAMPLE_TARGET_HZ 1000
#define DEFAULT_GRAPH_TIME_SECS 3.0
#define MOUSE_OVER_UPDATE_INTERVAL_MS 1000
#define NUM_INTANS_MAX 8
#define NUM_MUX_CHANS_MAX (512)
#define DEFAULT_PD_SILENCE .010 /* 10 ms silence default */
#define SETTINGS_DOMAIN "janelia.hhmi.org"
#define SETTINGS_APP APPNAME
#define MAX_NUM_GRAPHS_PER_GRAPH_TAB 64
#define DEFAULT_NUM_GRAPHS_PER_GRAPH_TAB 36
#define SAMPLE_BUF_Q_SIZE 128

#define SAMPLES_SHM_NAME "SpikeGL_SampleData"
#ifdef WIN64
#define DEF_SAMPLES_SHM_SIZE_FG (1024ULL*1024ULL*2000ULL) /* 2GB samples shm/buffer size */
#define DEF_SAMPLES_SHM_SIZE_REG (1024ULL*1024ULL*384ULL) /* 384 MB samples shm/buffer size */
#else
#define DEF_SAMPLES_SHM_SIZE_FG (1024*1024*384) /* 384 MB samples shm/buffer size */
#define DEF_SAMPLES_SHM_SIZE_REG (1024*1024*384) /* 384 MB samples shm/buffer size */
#endif
#define SAMPLES_SHM_DESIRED_PAGETIME_MS (33) /* 33 ms  */

extern bool excessiveDebug; ///< If true, print lots of debug output.. mainly daq related.. enable in console with control-D
#endif
