/*
 *  FileViewerWindow.cpp
 *  SpikeGL
 *
 *  Created by calin on 10/26/10.
 *  Copyright 2010 Calin Culianu <calin.culianu@gmail.com>. All rights reserved.
 *
 */

#include "FileViewerWindow.h"
#include <QFileInfo>

FileViewerWindow::FileViewerWindow()
: QMainWindow(0)
{	
}

FileViewerWindow::~FileViewerWindow()
{	
}


bool FileViewerWindow::viewFile(const QString & fname)
{
	if (!dataFile.openForRead(fname)) return false;

	setWindowTitle(QString(APPNAME) + QString(" File Viewer - ") + QFileInfo(fname).fileName() + " " 
				   + QString::number(dataFile.numChans()) + " channels @ " 
				   + QString::number(dataFile.samplingRateHz()) 
				   + " Hz"
				   + ", " + QString::number(dataFile.scanCount()) + " scans"
				   );
	resize(640, 480);
	
	return true;
}

bool FileViewerWindow::queryCloseOK() 
{
	// for now, always ok..
	return true;
}

