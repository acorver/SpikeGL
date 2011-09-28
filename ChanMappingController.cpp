#include "ChanMappingController.h"
#include <QDialog>
#include <QSettings>
#include <QMessageBox>
#include <QSet>

static bool setupDefaults = false;

/* static */
ChanMap ChanMappingController::defaultMapping[DAQ::N_Modes];

ChanMappingController::ChanMappingController(QObject *parent)
:  QObject(parent), currentMode(DAQ::AI60Demux)
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
		}
	}
	
    dialogParent = new QDialog(0);    
    dialog = new Ui::ChanMapping;
    dialog->setupUi(dialogParent);
    resetFromSettings();
}

void ChanMappingController::resetFromSettings()
{
    loadSettings();
	const ChanMap & cm (mapping[currentMode]);
	
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

ChanMappingController::~ChanMappingController()
{
    delete dialog, dialog = 0;
    delete dialogParent, dialogParent = 0;
}

bool ChanMappingController::mappingFromForm()
{
	ChanMap cm (mapping[currentMode]);
    const int rowct = dialog->tableWidget->rowCount();
	QSet<int> seen;
    for (int i = 0; i < rowct; ++i) {
        QTableWidgetItem *ti = dialog->tableWidget->item(i, 2);
		bool ok;
		int val = ti->text().toInt(&ok);
		if (ok) {
			cm[i].electrodeId = val;
			if (seen.contains(val)) {
				return false;
			}
			seen.insert(val);
		}
	}
	// now that everything's ok, save mapping to class state...
	mapping[currentMode] = cm;
	return true;
}

bool ChanMappingController::exec()
{
    resetFromSettings();
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
	}						
}

void ChanMappingController::saveSettings()
{
    QSettings settings(SETTINGS_DOMAIN, SETTINGS_APP);

	settings.beginGroup("ChanMappingController2");
	
	for (int i = 0; i < (int)DAQ::N_Modes; ++i) {
		settings.setValue(QString("terseString_%1").arg(i), mapping[i].toTerseString(QBitArray(mapping[i].size(),true)));
	}
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
