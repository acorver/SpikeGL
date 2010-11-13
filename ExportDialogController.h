#ifndef ExportDialogController_H
#define ExportDialogController_H

#include "ui_ExportDialog.h"
#include <QBitArray>
#include <Qstring>

class QDialog;

struct ExportParams
{
	QString filename;
	enum Format { Bin = 0, Csv, N_Format } format;
	bool allChans;
	QBitArray chanSubset;
	bool allScans;
	qint64 from, to;
};

class ExportDialogController: public QObject
{
		Q_OBJECT
public:
	ExportDialogController(QWidget *parent = 0);
	~ExportDialogController();
	
	Ui::ExportDialog *dialog;
	
	ExportParams acceptedParams;
	
	bool exec();
	
private:
	QDialog *dialogW;
};

#endif
