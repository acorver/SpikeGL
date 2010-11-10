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
#include "Vec2WrapBuffer.h"
#include <QPair>
#include "ChanMap.h"
#include <QBitArray>

class GLGraph;
class QScrollArea;
class QSpinBox;
class QDoubleSpinBox;
class QSlider;
class QLabel;
class QToolBar;
class QPushButton;
class QTimer;

/// The class that handles the window you get when opening files.
class FileViewerWindow : public QMainWindow
{
	Q_OBJECT
public:
	FileViewerWindow();
	~FileViewerWindow();

	/// Call this method to associate a file with this class and open it and view it.
	bool viewFile(const QString & fileName, QString *errMsg_out = 0);
	
	/// Returns the filename with path of the .bin file that is opened by this instance, or null QString if nothing is open
	QString file() const { if (dataFile.isOpen()) return  dataFile.fileName(); return QString::null; }

	/// Offer the user opportunity to abort a close operation, otherwise closes the window
	bool queryCloseOK();
	
protected:
	void resizeEvent(QResizeEvent *);
	void showEvent(QShowEvent *);

private slots:
	void setFilePos(int pos); // TODO/FIXME/XXX: this should take a 64-bit parameter but we need to connect to Qt gui stuff that is 32 bit
	void setFilePosSecs(double);
	void setXScaleSecs(double);
	void setYScale(double);
	void setNDivs(int);
	void colorSchemeMenuSlot();
	void setAuxGain(double);
	void mouseOverGraph(double,double);
	void mouseOverGraphInWindowCoords(int,int);
	void clickedGraphInWindowCoords(int,int);
	void showAllGraphs();
	void hideUnhideGraphSlot();
	void hideCloseTimeout();
	void viewModeMenuSlot();
	void resizeIt();
	void updateIt();
	
private:
	void loadSettings();
	void saveSettings();
	void layoutGraphs();
	void updateData();
	double timeFromPos(qint64 p) const;
	qint64 posFromTime(double) const;
	void configureMiscControls();
	qint64 nScansPerGraph() const;
	QPair<double, double> yVoltsAfterGain() const;
	void applyColorScheme(GLGraph *);
	void hideGraph(int n);
	void showGraph(int n);
	void mouseOverGraphInWindowCoords(GLGraph *, int,int);
	
	enum ViewMode { Tiled = 0, Stacked, StackedLarge, StackedHuge, N_ViewMode } viewMode;
	static const QString viewModeNames[];
	
	enum ColorScheme { Ice = 0, Fire, Green, BlackWhite, Classic, N_ColorScheme, DefaultScheme = Ice } colorScheme;
	static const QString colorSchemeNames[];
	
	DataFile dataFile;
		
	QScrollArea *scrollArea; ///< the central widget
	QWidget *graphParent;
	QVector<GLGraph *> graphs;
	QVector<Vec2WrapBuffer> graphBufs;	
	QSpinBox *posScansSB;
	QDoubleSpinBox *posSecsSB;
	QSlider *posSlider;
	QLabel *totScansLbl;
	QLabel *totSecsLbl;
	QToolBar *toolBar;
	QDoubleSpinBox *xScaleSB, *yScaleSB, *auxGainSB;
	QLabel *xDivLbl, *yDivLbl;
	QSpinBox *nDivsSB;
	QLabel *closeLbl;
	QVector<QAction *> graphHideUnhideActions;

	QMenu *channelsMenu;
	QAction *colorSchemeActions[N_ColorScheme];	
	QAction *viewModeActions[N_ViewMode];
	QTimer *hideCloseTimer;

	bool didLayout;
	
	// misc graph zoom/view/etc settings
	double nSecsZoom, yZoom, auxGain;
	unsigned nDivs;
	int maximizedGraph; ///< if non-negative, we are maximized on a particular graph
	
	qint64 pos, // in scan counts
	       pscale; // scaling factor for the QSlider since it uses 32-bit values and file pos can theoretically be 64-bit
	ChanMap chanMap;
	QBitArray hiddenGraphs;
};


#endif

