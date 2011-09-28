/*
 *  AOWriteThread.h
 *  SpikeGL
 *
 *  Created by calin on 11/14/10.
 *  Copyright 2010 Calin Culianu <calin.culianu@gmail.com>. All rights reserved.
 *
 */
#ifndef AOWriteThread_H
#define AOWriteThread_H
#include "DAQ.h"
#include "SampleBufQ.h"
#include <QThread>
//#ifdef HAVE_NIDAQmx
#  include "NI/NIDAQmx.h"
//#endif

namespace DAQ {

class AOWriteThread : public QThread, public SampleBufQ {
	Q_OBJECT
public:
	AOWriteThread(QObject * parent, 
				  TaskHandle & taskHandle,
				  int32 aoBufferSize,
				  const Params & params);
	~AOWriteThread();
	void stop();
signals:
	void daqError(const QString &);
protected:
	void run();
private:
	volatile bool pleaseStop;
	TaskHandle & taskHandle;
	int32 aoBufferSize;
	const Params & params;
};
	
}
#endif
