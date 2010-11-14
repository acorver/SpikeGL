#ifndef ExportDialogController_H
#define ExportDialogController_H

#include "ui_ExportDialog.h"
#include <QBitArray>
#include <Qstring>

class QDialog;

struct ExportParams
{
	// INPUT parameters (not modified by the dialog)
	qint64 nScans /**< total in datafile */, nChans /**< total in datafile */;
	QBitArray chansNotHidden;
	qint64 selectionFrom, selectionTo;

	// IN/OUT parameters (modified by the dialog)
	QString filename; ///< required input as well
	enum Format { Bin = 0, Csv, N_Format } format;
	bool allChans, allShown, customSubset;
	QBitArray chanSubset; ///< input/output -- as input can prepopulate the chan subset lineedit
	bool allScans;
	qint64 from, to;
	
	ExportParams();
};

class ExportDialogController: public QObject
{
		Q_OBJECT
public:
	ExportDialogController(QWidget *parent = 0);
	~ExportDialogController();
	
	Ui::ExportDialog *dialog;
	
	ExportParams params; ///< IN/OUT variable set this before calling exec() to pre-populate the form with values, then read it back to see what user specified
	
	bool exec();

private slots:
	void browseButSlot();
	void exportFormatSlot();
	void exportGraphsSlot();
	void exportRangeSlot();
	
private:
	void estimateFileSize();
	void dialogFromParams();
	
	QDialog *dialogW;
};

#endif
