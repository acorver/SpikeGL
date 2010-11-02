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

class GLGraph;
class QScrollArea;

/// The class that handles the window you get when opening files.
class FileViewerWindow : public QMainWindow
{
public:
	FileViewerWindow();
	~FileViewerWindow();

	/// Call this method to associate a file with this class and open it and view it.
	bool viewFile(const QString & fileName);
	
	/// Returns the filename with path of the .bin file that is opened by this instance, or null QString if nothing is open
	QString file() const { if (dataFile.isOpen()) return  dataFile.fileName(); return QString::null; }

	/// Offer the user opportunity to abort a close operation, otherwise closes the window
	bool queryCloseOK();
	
private:
	void loadSettings();
	void saveSettings();
	void layoutGraphs();
	
	enum ViewMode { Tiled = 0, Stacked, N_ViewMode } viewMode;
	
	DataFile dataFile;
	QScrollArea *scrollArea; ///< the central widget
	QWidget *graphParent;
	QVector<GLGraph *> graphs;
};


#endif

