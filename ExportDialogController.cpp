#include "ExportDialogController.h"
#include <QDialog>
#include <QButtonGroup>

ExportDialogController::ExportDialogController(QWidget *parent)
	: QObject(parent) 
{
	dialogW = new QDialog(parent);
	dialog = new Ui::ExportDialog;
	dialog->setupUi(dialogW);
	
	// button group for format radio buts
	QButtonGroup *bg = new QButtonGroup(this);
	bg->addButton(dialog->binRadio);
	bg->addButton(dialog->csvRadio);
	
	bg = new QButtonGroup(this);
	bg->addButton(dialog->allRadio);
	bg->addButton(dialog->allShownRadio);
	bg->addButton(dialog->customRadio);
	
	bg = new QButtonGroup(this);
	bg->addButton(dialog->allScansRadio);
	bg->addButton(dialog->selectionRadio);
	bg->addButton(dialog->customRangeRadio);
}

ExportDialogController::~ExportDialogController() 
{
	delete dialog, dialog = 0;
	delete dialogW, dialogW = 0;
}


bool ExportDialogController::exec()
{
	int ret = dialogW->exec();
	return ret == QDialog::Accepted;
}


