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

AOWriteThread::AOWriteThread(QObject *parent, const QString & aoChanString, const Params & params, AOWriteThread *oldToDelete)
: QThread(parent), SampleBufQ("AOWriteThread", 128), aoChanString(aoChanString), params(params), old2Delete(oldToDelete)
{
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

}


#endif
