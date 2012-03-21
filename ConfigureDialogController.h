#ifndef ConfigureDialogController_H
#define ConfigureDialogController_H

#include "ui_ConfigureDialog.h"
#include "ui_AcqPDParams.h"
#include "ui_AcqTimedParams.h"
#include "ui_AOPassthru.h"
#include "ui_ApplyDialog.h"
#include <QObject>
#include "SpikeGL.h"
#include <QVector>
#include "DAQ.h"
#include "ChanMappingController.h"

class QSettings;

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
    Ui::AoPassThru *aoPassthru;
    Ui::ApplyDialog *applyDlg;

    DAQ::Params acceptedParams;
    
    const ChanMappingController & chanMappingController() const { return chanMapCtl; }

    void showAOPassThruDlg();

    bool isDialogVisible() const { return dialogW->isVisible(); }

    QString acqParamsToString();
    QString acqParamsFromString(const QString & paramString); ///< returns QString::null on success, or an explanatory error message on error

    void loadSettings();
    void saveSettings() const;

    static QString parseAIChanString(const QString & aichanstr, QVector<unsigned> & aiChannels_out, bool *parse_error = 0, bool emptyOk = false);
    static QMap<unsigned,unsigned> parseAOPassthruString(const QString & aochanstr, bool *parse_error = 0);
	static QString generateAIChanString(const QVector<unsigned> & aiChans_sorted_ascending);
	
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
    void applyAOPass();
	void bufferSizeSliderChanged();
	void dualDevModeChkd();

private:
    void resetFromParams(DAQ::Params *p = 0);
    void resetAOPassFromParams(Ui::AoPassThru *);
    static void paramsFromSettingsObject(DAQ::Params & p, const QSettings & settings);
    
    enum ValidationResult {
        AGAIN = -1,
        ABORT =  0,
        OK    =  1
    };
    
    ValidationResult validateForm(QString & errTitle, QString & errMsg, bool isGUI = false);

    QWidget *dialogW, *acqPdParamsW, *acqTimedParamsW, *aoPassW;
    ChanMappingController chanMapCtl;
    DAQ::DeviceRangeMap aiDevRanges, aoDevRanges;
    QVector<QString> devNames, aoDevNames;
    DAQ::DeviceChanMap aiChanLists, aoChanLists;
};


#endif
