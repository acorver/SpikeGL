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

    enum SettingsCategory {
        ALL = 0xffffffff, BASE = 1, BUG = 2, FG = 4
    };

    void loadSettings();
    void saveSettings(int sc = ALL) const;

    static QString parseAIChanString(const QString & aichanstr, QVector<unsigned> & aiChannels_out, bool *parse_error = 0, bool emptyOk = false);
    static QMap<unsigned,unsigned> parseAOPassthruString(const QString & aochanstr, bool *parse_error = 0);
	static QString generateAIChanString(const QVector<unsigned> & aiChans_sorted_ascending);
    static bool chopNumberFromFilename(const QString & filename, QString & numberless, int & number);
	static int setFilenameTakingIntoAccountIncrementHack(DAQ::Params & p, DAQ::AcqStartEndMode m, const QString & filename, QWidget *dialogW, bool isGUI=true);

    void resetAOPassFromParams(Ui::AoPassThru *ao, DAQ::Params *p = 0, const unsigned *srate_override = 0);
    static void updateAOBufferSizeLabel(Ui::AoPassThru *aopass);
    void updateAORangeOnCBChange(Ui::AoPassThru *aoPassthru);
    QString getAODevName(Ui::AoPassThru *ao);

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
	void aoBufferSizeSliderChanged();
	void dualDevModeChkd();
	void secondIsAuxChkd();
	void aoNote();

private:
    void resetFromParams(DAQ::Params *p = 0);
    static void paramsFromSettingsObject(DAQ::Params & p, const QSettings & settings);
	void probeDAQHardware();
	
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
	QDialog *helpWindow;
};


#endif
