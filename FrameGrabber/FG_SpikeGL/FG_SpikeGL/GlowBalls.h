#pragma once
#include "stdafx.h"
#include "SpikeGLHandlerThread.h"

// Sapera-related
extern SapAcquisition *acq;
extern SapBuffer      *buffers;
extern SapTransfer    *xfer;
#define BUFFER_MEMORY_MB 16
extern int desiredHeight; // 32
extern int desiredWidth; // 144
#define NUM_BUFFERS()  ((BUFFER_MEMORY_MB*1024*1024) / (desiredHeight*desiredWidth) )
extern unsigned long long frameNum; // starts at 0, first frame sent is 1
extern unsigned frameIdx;
extern XtCmdImg *frames;


extern std::string configFilename;
extern int serverIndex, resourceIndex;

// SpikeGL communication related
extern SpikeGLOutThread    *spikeGL;
extern SpikeGLInputThread  *spikeGLIn;
extern UINT_PTR timerId;

/// Misc
extern std::vector<BYTE> spikeGLFrameBuf;
extern bool gotFirstXferCallback, gotFirstStartFrameCallback;
extern int bpp, pitch, width, height;

class FPGA;
extern FPGA *fpga;