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
#define NUM_CHANS_PER_INTAN 15
#define NUM_CHANS_PER_INTAN_2 16
#define MUX_CHANS_PER_PHYS_CHAN NUM_CHANS_PER_INTAN
#define MUX_CHANS_PER_PHYS_CHAN_2 NUM_CHANS_PER_INTAN_2
#define LOCK_TIMEOUT_MS 2000
#define DEF_TASK_READ_FREQ_HZ_ 10
#define DEF_TASK_READ_FREQ_HZ Util::getTaskReadFreqHz()
#define TASK_WRITE_FREQ_HZ 10
#define APPNAME "SpikeGL"
#define DOWNSAMPLE_TARGET_HZ 1000
#define DEFAULT_GRAPH_TIME_SECS 3.0
#define MOUSE_OVER_UPDATE_INTERVAL_MS 1000
#define NUM_INTANS_MAX 8
#define NUM_MUX_CHANS_MAX (NUM_INTANS_MAX*MUX_CHANS_PER_PHYS_CHAN_2)
#define DEFAULT_PD_SILENCE .010 /* 10 ms silence default */

#endif
