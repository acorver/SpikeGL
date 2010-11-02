/*
 *  FileViewerWindow.cpp
 *  SpikeGL
 *
 *  Created by calin on 10/26/10.
 *  Copyright 2010 Calin Culianu <calin.culianu@gmail.com>. All rights reserved.
 *
 */

#include "FileViewerWindow.h"
#include "GLGraph.h"
#include <QFileInfo>
#include <QScrollArea>

static const QSize graphSizes[] = { 
	QSize(160, 80),  // tiled
	QSize(640, 320)   // stacked
};


FileViewerWindow::FileViewerWindow()
: QMainWindow(0)
{	
	setCentralWidget(scrollArea = new QScrollArea(this));
	graphParent = new QWidget(scrollArea);
	scrollArea->setWidget(graphParent);
}

FileViewerWindow::~FileViewerWindow()
{	
	/// scrollArea and graphParent automatically deleted here because they are children of us.
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
	
	loadSettings();
		
	resize(640,480);
	
	graphs.resize(dataFile.numChans());	
	const QSize & graphSize = graphSizes[viewMode];
	for (int i = 0, n = graphs.size(); i < n; ++i) {
		graphs[i] = new GLGraph(graphParent);
		graphs[i]->resize(graphSize.width(), graphSize.height());
	}
	
	layoutGraphs();
	
	return true;
}

bool FileViewerWindow::queryCloseOK() 
{
	// for now, always ok..
	return true;
}

void FileViewerWindow::loadSettings() 
{
    QSettings settings("janelia.hhmi.org", APPNAME);
	
    settings.beginGroup("FileViewerWindow");
	viewMode = Tiled;
    int vm = settings.value("viewMode", (int)Tiled).toInt();
	if (vm >= 0 && vm < N_ViewMode) viewMode = (ViewMode)vm;
}

void FileViewerWindow::saveSettings()
{
    QSettings settings("janelia.hhmi.org", APPNAME);
	
    settings.beginGroup("FileViewerWindow");
    settings.setValue("viewMode", (int)viewMode);
}

void FileViewerWindow::layoutGraphs()
{	
	const int w = 960, h = 640;
	const int padding = 5;
	const int n = graphs.size();
	int y = 0;
	
	graphParent->resize(w, h);

	if (viewMode == Tiled) {
		
		int x = 0;
		for (int i = 0; i < n; ++i) {
			graphs[i]->move(x,y);
			x += graphs[i]->width() + padding;
			if (x > w) x = 0, y += graphs[i]->height() + padding;
		}
		
	} else if (viewMode == Stacked) {
		
		resize(graphParent->width(), height());
		
		for (int i = 0; i < n; ++i) {
			graphs[i]->resize(w, graphs[i]->height());
			graphs[i]->move(0,y);
			y += graphs[i]->height() + padding;
		}

	}	
	
	graphParent->resize(w, y);

}
