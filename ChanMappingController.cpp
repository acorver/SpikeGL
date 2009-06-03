#include "ChanMappingController.h"
#include <QDialog>
#include <QSettings>

/*static*/ unsigned ChanMappingController::defaultPinMapping[NUM_MUX_CHANS_MAX] = {
    35, 25, 15, 48, 45, 37, 36, 27, 17, 26, 16, 47, 46, 
    38, 2, 41, 44, 32, 33, 14, 24, 34, 43, 31, 21, 42, 13,
    23, 12, 22, 83, 52, 53, 61, 71, 73, 82, 72, 64, 74, 84,
    63, 62, 54, 51, 78, 68, 56, 57, 86, 76, 87, 77, 66, 67,
    55, 58, 85, 75, 65
};

ChanMappingController::ChanMappingController(QObject *parent)
 :  QObject(parent)
{
    dialogParent = new QDialog(0);    
    dialog = new Ui::ChanMapping;
    dialog->setupUi(dialogParent);
    resetFromSettings();
}

void ChanMappingController::resetFromSettings()
{
    loadSettings();

    const int rowct = NUM_MUX_CHANS_MAX; 
    dialog->tableWidget->setRowCount(NUM_MUX_CHANS_MAX);
    for (int i = 0; i < rowct; ++i) {
        // row label
        QTableWidgetItem *ti = dialog->tableWidget->verticalHeaderItem(i);
        if (!ti) {
            ti = new QTableWidgetItem;
            dialog->tableWidget->setVerticalHeaderItem(i,ti);
        }
        ti->setText(QString::number(i));
        // intan chip/channel column 0
        ti = dialog->tableWidget->item(i, 0);
        if (!ti) {
            ti = new QTableWidgetItem;
            dialog->tableWidget->setItem(i,0,ti);
        }
        ti->setText(QString("%1/%2").arg(i/NUM_CHANS_PER_INTAN + 1).arg(i%NUM_CHANS_PER_INTAN + 1));
        ti->setFlags(Qt::NoItemFlags);
        // pin channel column 1
        ti = dialog->tableWidget->item(i, 1);
        if (!ti) {
            ti = new QTableWidgetItem;
            dialog->tableWidget->setItem(i,1,ti);
        }
        ti->setText(QString::number(pinMapping[i]));
        ti->setFlags(Qt::ItemIsEditable|Qt::ItemIsEnabled);
        // electrode channel column 2
        ti = dialog->tableWidget->item(i, 2);
        if (!ti) {
            ti = new QTableWidgetItem;
            dialog->tableWidget->setItem(i,2,ti);
        }
        ti->setText(QString::number(i));
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

bool ChanMappingController::exec()
{
    resetFromSettings();
    if (dialogParent->exec() == QDialog::Accepted) {        
        saveSettings();
        return true;
    }
    return false;
}

void ChanMappingController::loadSettings()
{
    QSettings settings("janelia.hhmi.org", "LeoDAQGL");

    settings.beginGroup("ChanMappingController");

    pinMapping.clear();
    pinMapping.resize(NUM_MUX_CHANS_MAX);
    eMapping.clear();
    eMapping.resize(NUM_MUX_CHANS_MAX);
    for (int i = 0; i < (int)pinMapping.size(); ++i) {
        pinMapping[i] = settings.value(QString("pinMapping_%1").arg(i), defaultPinMapping[i]).toUInt();
        eMapping[i] =  settings.value(QString("electrodeMapping_%1").arg(i), i).toUInt();
    }
}

void ChanMappingController::saveSettings()
{
    QSettings settings("janelia.hhmi.org", "LeoDAQGL");

    settings.beginGroup("ChanMappingController");

    for (int i = 0; i < (int)pinMapping.size(); ++i) {
        settings.setValue(QString("pinMapping_%1").arg(i), pinMapping[i]);
        settings.setValue(QString("electrodeMapping_%1").arg(i), eMapping[i]);
    }
}

ChanMapDesc ChanMappingController::mappingForGraph(unsigned graphNum) const 
{
    ChanMapDesc ret;
    ret.graphNum = graphNum;
    if (graphNum < NUM_MUX_CHANS_MAX) {
        ret.intan = graphNum / NUM_CHANS_PER_INTAN + 1;
        ret.intanCh = graphNum % NUM_CHANS_PER_INTAN + 1;
        ret.pch = pinMapping[graphNum];
        ret.ech = eMapping[graphNum];
    }
    return ret;
}

ChanMapDesc ChanMappingController::mappingForIntan(unsigned intan, unsigned intan_ch) const
{
    unsigned graphNum = (intan-1)*15 + (intan_ch-1);
    return mappingForGraph(graphNum);
}

ChanMap ChanMappingController::mappingForAll() const
{
    ChanMap ret;
    ret.reserve(NUM_MUX_CHANS_MAX);
    for (int i = 0; i < NUM_MUX_CHANS_MAX; ++i)
        ret.push_back(mappingForGraph(i));
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
