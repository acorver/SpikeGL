#include "ChanMappingController.h"
#include <QDialog>
#include <QSettings>
#include <QMessageBox>
#include <QSet>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>

static bool setupDefaults = false;

/* static */
ChanMap ChanMappingController::defaultMapping[DAQ::N_Modes],
        ChanMappingController::defaultMapping2[DAQ::N_Modes];

ChanMappingController::ChanMappingController(QObject *parent)
:  QObject(parent), currentMode(DAQ::AI60Demux), is_dual(false)
{
	
	if (!setupDefaults) {
		setupDefaults = true;
		// run once -- setup defaults array
		for (int i = 0; i < (int)DAQ::N_Modes; ++i) {
			ChanMap & cm (defaultMapping[i]);
			const unsigned numIntans = DAQ::ModeNumIntans[i], numChansPerIntan = DAQ::ModeNumChansPerIntan[i];
			cm.reserve(numIntans*numChansPerIntan);
			for (int j = 0; j < (int)numIntans; ++j)
				for (int k = 0; k < (int)numChansPerIntan; ++k)
					cm.push_back(defaultMappingForIntan(j, k, numChansPerIntan));
			ChanMap & cm2 (defaultMapping2[i]);
			const unsigned numIntans2 = DAQ::ModeNumIntans[i]*2;
			cm2.reserve(numIntans2*numChansPerIntan);
			for (int j = 0; j < (int)numIntans2; ++j)
				for (int k = 0; k < (int)numChansPerIntan; ++k)
					cm2.push_back(defaultMappingForIntan(j, k, numChansPerIntan));
		}
	}
	
    dialogParent = new QDialog(0);    
    dialog = new Ui::ChanMapping;
    dialog->setupUi(dialogParent);
	Connect(dialog->loadBut, SIGNAL(clicked()), this, SLOT(loadButPushed()));
	Connect(dialog->saveBut, SIGNAL(clicked()), this, SLOT(saveButPushed()));
    resetFromSettings();
}

void ChanMappingController::resetFromMapping(const ChanMap & cm)
{
    const int rowct = cm.size(); 
    dialog->tableWidget->setRowCount(rowct);
    for (int i = 0; i < rowct; ++i) {
        // row label
        QTableWidgetItem *ti = dialog->tableWidget->verticalHeaderItem(i);
        if (!ti) {
            ti = new QTableWidgetItem;
            dialog->tableWidget->setVerticalHeaderItem(i,ti);
        }
        ti->setText(QString::number(i));
        // intan chip column 0
        ti = dialog->tableWidget->item(i, 0);
        if (!ti) {
            ti = new QTableWidgetItem;
            dialog->tableWidget->setItem(i,0,ti);
        }
        ti->setText(QString::number(cm[i].intan+1));
        ti->setFlags(Qt::ItemIsEnabled);
        // intan channel column 1
        ti = dialog->tableWidget->item(i, 1);
        if (!ti) {
            ti = new QTableWidgetItem;
            dialog->tableWidget->setItem(i,1,ti);
        }
        ti->setText(QString::number(cm[i].intanCh+1));
        ti->setFlags(Qt::ItemIsEnabled);
        // electrode channel column 2
        ti = dialog->tableWidget->item(i, 2);
        if (!ti) {
            ti = new QTableWidgetItem;
            dialog->tableWidget->setItem(i,2,ti);
        }
        ti->setText(QString::number(cm[i].electrodeId));
        ti->setFlags(Qt::ItemIsEditable|Qt::ItemIsEnabled);        
    }
    dialog->tableWidget->setColumnWidth(0,150);
    dialog->tableWidget->setColumnWidth(1,150);
    dialog->tableWidget->setColumnWidth(2,150);
}

void ChanMappingController::resetFromSettings()
{
    loadSettings();
	const ChanMap & cm (is_dual ? mapping2[currentMode] : mapping[currentMode]);
	resetFromMapping(cm);
}

ChanMappingController::~ChanMappingController()
{
    delete dialog, dialog = 0;
    delete dialogParent, dialogParent = 0;
}

ChanMap ChanMappingController::getFormMapping() const
{
	ChanMap cm (is_dual ? mapping2[currentMode] : mapping[currentMode]);
    const int rowct = dialog->tableWidget->rowCount();
	QSet<int> seen;
    for (int i = 0; i < rowct; ++i) {
        QTableWidgetItem *ti = dialog->tableWidget->item(i, 2);
		bool ok;
		int val = ti->text().toInt(&ok);
		if (ok) {
			cm[i].electrodeId = val;
			if (seen.contains(val)) {
				return ChanMap();
			}
			seen.insert(val);
		}
	}
    return cm;
}

bool ChanMappingController::mappingFromForm()
{
    ChanMap cm( getFormMapping() );
    if (!cm.size()) return false;
	// now that everything's ok, save mapping to class state...
	if (is_dual) 
		mapping2[currentMode] = cm;
	else
		mapping[currentMode] = cm;
	return true;
}

bool ChanMappingController::exec()
{
    resetFromSettings();
	dialog->label->setText(QString (is_dual ? "Below mapping is for DUAL dev mode" :"Below mapping for SINGLE dev mode") + " " + DAQ::ModeToString(currentMode) );
	dialog->statusLbl->setText("");

	bool again;
	do {
		again = false;
		if (dialogParent->exec() == QDialog::Accepted) {    
			if (!mappingFromForm()) {
				QMessageBox::critical(dialogParent, "Invalid Electrode Mapping", "The electrode mapping specified is invalid.  This is probably due to having specified dupe electrode id's."); 
				again = true;
				continue;
			} else {
				saveSettings();
				return true;
			}
		}
	} while (again);
    return false;
}

void ChanMappingController::loadSettings()
{
    QSettings settings(SETTINGS_DOMAIN, SETTINGS_APP);

	settings.beginGroup(QString("ChanMappingController2"));

	for (int i = 0; i < (int)DAQ::N_Modes; ++i) {
		QString ts = settings.value(QString("terseString_%1").arg(i), QString("")).toString();
		if (!ts.length()) {
			mapping[i] = defaultMapping[i];	
		} else
			mapping[i] = ChanMap::fromTerseString(ts);
		ts = settings.value(QString("terseString2_%1").arg(i), QString("")).toString();
		if (!ts.length()) {
			mapping2[i] = defaultMapping2[i];	
		} else
			mapping2[i] = ChanMap::fromTerseString(ts);
	}						
	lastDir = settings.value("lastCMDlgDir", QString()).toString();
}

void ChanMappingController::saveSettings()
{
    QSettings settings(SETTINGS_DOMAIN, SETTINGS_APP);

	settings.beginGroup("ChanMappingController2");
	
	for (int i = 0; i < (int)DAQ::N_Modes; ++i) {
		settings.setValue(QString("terseString_%1").arg(i), mapping[i].toTerseString(QBitArray(mapping[i].size(),true)));
		settings.setValue(QString("terseString2_%1").arg(i), mapping2[i].toTerseString(QBitArray(mapping2[i].size(),true)));
	}
	settings.setValue("lastCMDlgDir", lastDir);
}

ChanMapDesc ChanMappingController::defaultMappingForIntan(unsigned intan, unsigned intan_ch,
														  unsigned chans_per_intan) 
{
	ChanMapDesc ret;
	ret.electrodeId = intan*chans_per_intan + intan_ch;
	ret.intan = intan;
	ret.intanCh = intan_ch;
    return ret;
}


void ChanMappingController::setDualDevMode(bool d)
{
	is_dual = d;
}


void ChanMappingController::loadButPushed()
{
	QString fn = QFileDialog::getOpenFileName(dialogParent, "Load a channel mapping", lastDir);
	if (fn.length()) {
		QFile f(fn);
		QFileInfo fi(fn);
		lastDir = fi.absolutePath();

		if (f.exists() && f.open(QIODevice::ReadOnly|QIODevice::Text)) {
			ChanMap cm(ChanMap::fromWSDelimFlatFileString(QString(f.readAll())));
			const int origsize = cm.size();
			const ChanMap &cur = currentMapping();
			if (cm.size() > cur.size()) {
				cm.resize(cur.size());
			} else if (cm.size() < cur.size()) {
				dialog->statusLbl->setText(QString("Error reading and/or mapping wrong size (from file %1)").arg(fn));
				return;
			}
			dialog->statusLbl->setText(QString("Loaded ") + QString::number(cm.size()) + "-channel mapping " + (origsize != cm.size() ? QString("(truncated from %1 chans) ").arg(origsize) : QString(""))  + "from " + fn);
			resetFromMapping(cm);
		} else {
			dialog->statusLbl->setText(QString("Failed to open ") + fn);
		}
	}
}

void ChanMappingController::saveButPushed()
{
	QString fn = QFileDialog::getSaveFileName(dialogParent, "Save channel mapping", lastDir);
    ChanMap cm( getFormMapping() );

	if (fn.length() && cm.size()) {
		QFile f(fn);
		QFileInfo fi(fn);
		lastDir = fi.absolutePath();
		/*if (f.exists()) {
			int q = QMessageBox::question(dialogParent, "Confirm Overwrite", QString(fn) + " exists.  Overwrite?", QMessageBox::Ok|QMessageBox::Cancel, QMessageBox::Cancel);
			if (q == QMessageBox::Cancel) {
				return;
			}
		}*/
		f.open(QIODevice::WriteOnly|QIODevice::Text|QIODevice::Truncate);
		if (!f.isOpen()) {
			dialog->statusLbl->setText(QString("Error opening %1 for writing").arg(fn));
			return;
		}
		int bc = f.write(cm.toWSDelimFlatFileString().toUtf8());
		if (bc < 0) {
			dialog->statusLbl->setText(QString("Error writing to %1").arg(fn));
			return;
		}
		dialog->statusLbl->setText(QString("Wrote mapping to file %1").arg(fn));
	}
}


#ifdef TEST_CH_MAP_CNTRL

void ChanMappingController::show()
{
    dialogParent->show();
}

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    ChanMappingController *c = new ChanMappingController(0);
    c->show();
    return app.exec();
}

#endif