#include "ExportDialogController.h"
#include <QDialog>
#include <QButtonGroup>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include "Util.h"
#include "MainApp.h"
#include "ConfigureDialogController.h"

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
	
	Connect(dialog->browseBut, SIGNAL(clicked()), this, SLOT(browseButSlot()));
	Connect(dialog->binRadio, SIGNAL(clicked()), this, SLOT(exportFormatSlot()));
	Connect(dialog->csvRadio, SIGNAL(clicked()), this, SLOT(exportFormatSlot()));
	Connect(dialog->allRadio, SIGNAL(clicked()), this, SLOT(exportGraphsSlot()));
	Connect(dialog->allShownRadio, SIGNAL(clicked()), this, SLOT(exportGraphsSlot()));
	Connect(dialog->customRadio, SIGNAL(clicked()), this, SLOT(exportGraphsSlot()));
	Connect(dialog->exportChansLE, SIGNAL(textChanged(const QString &)), this, SLOT(exportGraphsSlot()));
	Connect(dialog->allScansRadio, SIGNAL(clicked()), this, SLOT(exportRangeSlot()));
	Connect(dialog->selectionRadio, SIGNAL(clicked()), this, SLOT(exportRangeSlot()));
	Connect(dialog->customRangeRadio, SIGNAL(clicked()), this, SLOT(exportRangeSlot()));
	Connect(dialog->fromSB, SIGNAL(valueChanged(int)), this, SLOT(exportRangeSlot()));
	Connect(dialog->toSB, SIGNAL(valueChanged(int)), this, SLOT(exportRangeSlot()));
}

ExportDialogController::~ExportDialogController() 
{
	delete dialog, dialog = 0;
	delete dialogW, dialogW = 0;
}


void ExportDialogController::dialogFromParams()
{
	dialog->filenameLE->setText(params.filename);
	if (params.format == ExportParams::Bin) dialog->binRadio->setChecked(true);
	else if (params.format == ExportParams::Csv) dialog->csvRadio->setChecked(true);
	dialog->fromSB->setValue(params.from);
	dialog->fromSB->setMinimum(0);
	dialog->fromSB->setMaximum(params.nScans-1);
	dialog->toSB->setValue(params.to);
	dialog->toSB->setMinimum(0);
	dialog->toSB->setMaximum(params.nScans-1);
	if (params.allScans) dialog->allScansRadio->setChecked(true);	
	else if (params.from > 0 || params.to > 0) dialog->customRangeRadio->setChecked(true);
	else dialog->selectionRadio->setChecked(true);
	QVector<unsigned> chans;
	const int n = params.chanSubset.size();
	chans.reserve(n);
	for (int i = 0; i < n; ++i)
		if (params.chanSubset.testBit(i)) {
			chans.push_back(i);
		}
	dialog->exportChansLE->setEnabled(false);
	QString subsetStr = ConfigureDialogController::generateAIChanString(chans);
	dialog->exportChansLE->setText(subsetStr);
	if (params.customSubset) dialog->customRadio->setChecked(true);
	else if (params.allChans) dialog->allRadio->setChecked(true);
	else dialog->allShownRadio->setChecked(true);
	dialog->exportChansLE->setEnabled(dialog->customRadio->isChecked());
	dialog->selectionLbl->setText(QString::number(params.selectionFrom) + "-" + QString::number(params.selectionTo));
	dialog->selectionLbl->setEnabled(params.selectionFrom >= 0);
	dialog->selectionRadio->setEnabled(params.selectionFrom >= 0);
	dialog->allScansLbl->setText(QString("0-") + QString::number(params.nScans-1));
	chans.clear();
	chans.reserve(params.chansNotHidden.count(true));
	for (int i = 0; i < (int)params.chansNotHidden.size(); ++i)
		if (params.chansNotHidden.testBit(i)) chans.push_back(i);
	QString s = ConfigureDialogController::generateAIChanString(chans);
	dialog->shownChannelsSpecLbl->setText(s);
	estimateFileSize();
}

bool ExportDialogController::exec()
{
	dialogFromParams();
	bool again;
	
	do {
		again = false;
		do {
			int ret = dialogW->exec();
			if (ret == QDialog::Accepted) {
				// validate form..
				ExportParams & p (params);
				QString fname = dialog->filenameLE->text();
				if (QFileInfo(fname).exists()) {
					QMessageBox::StandardButton b = QMessageBox::question(dialogW, "Output File Exists", QString("The specified output file ") + QFileInfo(fname).fileName() + " already exists, overwrite it?", QMessageBox::Yes|QMessageBox::No, QMessageBox::Yes);
					if (b != QMessageBox::Yes) {
						again = true;
						break;
					}
				}
				p.format = dialog->binRadio->isChecked() ? ExportParams::Bin : ExportParams::Csv;
				p.filename = fname;
				p.allChans = dialog->allRadio->isChecked();
				p.allShown = dialog->allShownRadio->isChecked();
				p.customSubset = dialog->customRadio->isChecked();
				if (p.allChans) p.chanSubset.fill(true, p.nChans);
				else if (p.allShown) p.chanSubset = p.chansNotHidden;
				else if (p.customSubset) {
					QString s = dialog->exportChansLE->text();
					bool err = false;
					QVector<unsigned> chans;
					ConfigureDialogController::parseAIChanString(s, chans, &err);
					if (err) {
						QMessageBox::critical(dialogW, "Export Chans Error", "The custom export chans string could not be parsed.  Try again.");
						again = true;
						break;
					}
					p.chanSubset.fill(false, p.nChans);
					foreach(unsigned i, chans) {
						p.chanSubset.setBit(i, true);
					}
				}
				if (dialog->allScansRadio->isChecked()) 
					p.from = 0, p.to = p.nScans-1;
				else if (dialog->selectionRadio->isChecked())
					p.from = p.selectionFrom, p.to = p.selectionTo;
				else
					p.from = dialog->fromSB->value(), p.to = dialog->toSB->value();
				
				if (!p.chanSubset.count(true) || ((p.to-p.from)+1) < 0) {
					QMessageBox::critical(dialogW, "Export File Is Empty", "You have specified an empty export file!  Please ensure you export at least 1 channel and at least 1 scan!");
					again = true;
					break;					
				}	
				if (p.format == ExportParams::Csv) {
					QMessageBox::StandardButton b = QMessageBox::question(dialogW, "Confirm CSV Format", "You chose to export data in the CSV format.  While this format can easily be read by other programs, you will lose some meta-information such as sampling rate, electrode id, gain, etc.\n\nProceed with CSV export?", QMessageBox::Yes|QMessageBox::No, QMessageBox::Yes);
					if (b != QMessageBox::Yes) {
						again = true;
						break;
					}					
				}
				return true;
			}
		} while (false);
	} while (again);
	return false;
}


void ExportDialogController::browseButSlot()
{
	QStringList types;
	types.push_back("Binary File (*.bin)");
	types.push_back("CSV Text (*.csv *.txt)");
	QString f = dialog->filenameLE->text();
	f = QFileDialog::getSaveFileName(dialogW, "Specify an export file", f, types.join(";;"), &types[dialog->binRadio->isChecked() ? 0 : 1]);
	QString suff = QFileInfo(f).suffix();
	if (suff != "bin" && suff != "csv" && suff != "txt") {
		if (dialog->binRadio->isChecked())
			f += ".bin";
		else if (dialog->csvRadio->isChecked())
			f += ".csv";
	} else {
		if (suff == "bin" && !dialog->binRadio->isChecked()) {
			dialog->binRadio->setChecked(true);
		} else if ((suff == "csv" || suff == "txt") && !dialog->csvRadio->isChecked()) {
			dialog->csvRadio->setChecked(true);
		}
	}
	dialog->filenameLE->setText(f);
	estimateFileSize();
}

void ExportDialogController::exportFormatSlot()
{
	QString f = dialog->filenameLE->text();
	QString suff = QFileInfo(f).suffix();
	int sufflen = suff.length();
	if (sufflen > 0) ++sufflen;
	f = f.mid(0,f.length()-sufflen);
	if (dialog->binRadio->isChecked())
		f += ".bin";
	if (dialog->csvRadio->isChecked())
		f += ".csv";
	dialog->filenameLE->setText(f);
	estimateFileSize();
}

void ExportDialogController::exportGraphsSlot()
{
	dialog->exportChansLE->setEnabled(dialog->customRadio->isChecked());
	estimateFileSize();
}

void ExportDialogController::exportRangeSlot()
{
	estimateFileSize();
}


void ExportDialogController::estimateFileSize()
{
	const qint64 sampleSize = dialog->binRadio->isChecked() ? sizeof(int16) : 8 /* arbitrary estimate of txt csv field size*/;
	qint64 nch = params.nChans, nsc = params.nScans;
	if (dialog->customRadio->isChecked()) {
		QVector<unsigned> chVec;
		ConfigureDialogController::parseAIChanString(dialog->exportChansLE->text(), chVec);
		nch = chVec.size();
	} else if (dialog->allShownRadio->isChecked()) {
		nch = params.chansNotHidden.count(true);
	} 		
	if (dialog->selectionRadio->isChecked())
		nsc = (params.selectionTo - params.selectionFrom) + 1;
	else if (dialog->customRangeRadio->isChecked())
		nsc = (dialog->toSB->value() - dialog->fromSB->value()) + 1;
	
	double filesizeMB = (sampleSize * nch * nsc) / (1024.*1024.);
	
	dialog->filesizeLbl->setText(QString("(Est. file size: ") + QString::number(filesizeMB, 'f', 2) + " MB)");
}

ExportParams::ExportParams()
: nScans(0), nChans(0), filename(""), format(Bin), allChans(false), allShown(false), customSubset(false), allScans(0), from(0), to(0)
{
}
