#pragma once
#include "stdafx.h"
#include "SpikeGLHandlerThread.h"

// Sapera-related
extern SapAcquisition *acq;
extern SapBuffer      *buffers;
extern SapTransfer    *xfer;
extern SapView        *view;
#define NUM_BUFFERS 200
extern std::string configFilename;
extern int serverIndex, resourceIndex;

// SpikeGL communication related
extern SpikeGLOutThread    *spikeGL;
extern SpikeGLInputThread  *spikeGLIn;
extern UINT_PTR timerId;
extern bool gotFirstXferCallback;
extern std::vector<BYTE> spikeGLFrameBuf;

class FPGA;
extern FPGA *fpga;