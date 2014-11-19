/*
 *  AOWriteThread.h
 *  SpikeGL
 *
 *  Created by calin on 11/14/10.
 *  Copyright 2010 Calin Culianu <calin.culianu@gmail.com>. All rights reserved.
 *
 */
#ifdef HAVE_NIDAQmx
#ifndef AOWriteThread_H
#define AOWriteThread_H
#include "DAQ.h"
#include "SampleBufQ.h"
#include <QObject>
#include <QThread>
//#ifdef HAVE_NIDAQmx
#  include "NI/NIDAQmx.h"
//#endif

namespace DAQ {

class AOWriteThread : public QThread, public SampleBufQ {
	Q_OBJECT
public:
	AOWriteThread(QObject * parent, const QString & aoChanString, const Params & params, AOWriteThread *oldAOWriteThreadToDeleteAfterStart = 0);
	~AOWriteThread();
	void stop();

    static const unsigned QueueSz;

signals:
	void daqError(const QString &);
protected:
	void run();
	void overflowWarning(); ///< prints a more helpful warning to console
private:
	volatile bool pleaseStop;
	QString aoChanString;
	const Params & params;
	AOWriteThread *old2Delete;
};
	
}
#endif
#endif