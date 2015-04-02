#pragma once
#include "stdafx.h"
#include "SpikeGLHandlerThread.h"

// Sapera-related
extern SapAcquisition *acq;
extern SapBuffer      *buffers;
extern SapTransfer    *xfer;
#define NUM_BUFFERS 200
#define DESIRED_HEIGHT 32
#define DESIRED_WIDTH 144
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