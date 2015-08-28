/*
 *  AOWriteThread.cpp
 *  SpikeGL
 *
 *  Created by calin on 11/14/10.
 *  Copyright 2010 Calin Culianu <calin.culianu@gmail.com>. All rights reserved.
 *
 */
#ifdef HAVE_NIDAQmx
#include "AOWriteThread.h"
#include "SpikeGL.h"
#include "NI/NIDAQmx.h"

namespace DAQ {

const unsigned AOWriteThread::QueueSz(5);

AOWriteThread::AOWriteThread(QObject *parent, const QString & aoChanString, const Params & params, AOWriteThread *oldToDelete)
: QThread(parent), SampleBufQ("AOWriteThread", QueueSz), aoChanString(aoChanString), params(params), old2Delete(oldToDelete)
{
    dequeueWarnThresh = 10;
	pleaseStop = false;
}

AOWriteThread::~AOWriteThread()
{
	stop();
	if (old2Delete)	delete old2Delete, old2Delete = 0;
}

void AOWriteThread::stop() 
{
	if (isRunning()) {
		pleaseStop = true;
		std::vector<int16> empty;
		enqueueBuffer(empty, 0); // forces a wake-up
		wait(); // wait for thread to join
	}
}

void AOWriteThread::overflowWarning() 
{
    Warning() << name << " overflow! Buffer queue full, dropping a buffer (capacity: " <<  dataQueueMaxSize << " buffers)!\n" 
		<< "This may be due to one or more of the following:\n"
		<< "(1) the system not being able to handle the specified AI/AO rate(s)\n"
		<< "(2) the AI and AO clocks are drifting with respect to each other, they may need to be linked physically (eg AI & AO should use PFI as clock, with AO samplerate = NUM_MUX_CHANS * AI Samplerate [if in MUX mode])\n"
		<< "(3) the AI sample rate specified is not accurate and you are using external clock (PFI) for AI, and OnBoard for AO";
}
}


#endif
