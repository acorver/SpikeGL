/*
 *  FileViewerWindow.h
 *  SpikeGL
 *
 *  Created by calin on 10/26/10.
 *  Copyright 2010 Calin Culianu <calin.culianu@gmail.com>. All rights reserved.
 *
 */
#ifndef FileViewerWindow_H
#define FileViewerWindow_H
#include <QMainWindow>
#include "DataFile.h"

class FileViewerWindow : public QMainWindow
{
public:
	FileViewerWindow();
	~FileViewerWindow();

	/// Call this method to associate a file with this class and open it and view it.
	bool viewFile(const QString & fileName);
	
	QString file() const { return dataFile.fileName(); }

	/// Offer the user opportunity to abort a close operation, otherwise closes the window
	bool queryCloseOK();
	
private:
	DataFile dataFile;
};


#endif

