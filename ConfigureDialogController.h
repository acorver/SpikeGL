#ifndef ConfigureDialogController_H
#define ConfigureDialogController_H

#include "ui_ConfigureDialog.h"
#include "ui_AcqPDParams.h"
#include "ui_AcqTimedParams.h"
#include <QObject>
#include "LeoDAQGL.h"
#include <QVector>
#include "DAQ.h"
#include "ChanMappingController.h"

class ConfigureDialogController : public QObject
{
    Q_OBJECT
public:
    ConfigureDialogController(QObject *parent);
    ~ConfigureDialogController();

    int exec();

    Ui::ConfigureDialog *dialog; 

    Ui::AcqPDParams *acqPdParams;
    Ui::AcqTimedParams *acqTimedParams;

    DAQ::Params acceptedParams;
    
    const ChanMappingController & chanMappingController() const { return chanMapCtl; }

protected slots:    
    void acqStartEndCBChanged();
    void acqModeCBChanged();
    void deviceCBChanged();
    void aoDeviceCBChanged();
    void browseButClicked();
    void aoPassthruChkd();
    void aoPDChanChkd();
    void aiRangeChanged();
    void aoPDPassthruUpdateLE();

private:
    void loadSettings();
    void saveSettings();
    void resetFromParams();
    QWidget *dialogW, *acqPdParamsW, *acqTimedParamsW;
    ChanMappingController chanMapCtl;
    DAQ::DeviceRangeMap aiDevRanges, aoDevRanges;
    QVector<QString> devNames, aoDevNames;
    DAQ::DeviceChanMap aiChanLists, aoChanLists;

    static QString parseAIChanString(const QString & aichanstr, QVector<unsigned> & aiChannels_out, bool *parse_error = 0);
    static QMap<unsigned,unsigned> parseAOPassthruString(const QString & aochanstr, bool *parse_error = 0);
    
};


#endif
