#include "ConfigureDialogController.h"
#include "LeoDAQGL.h"
#include "Util.h"
#include "MainApp.h"
#include <QTimeEdit>
#include <QTime>
#include <QMessageBox>
#include <QSet>
#include <QSettings>
#include <QFileInfo>
#include <QFileDialog>
#include "Icon-Config.xpm"

ConfigureDialogController::ConfigureDialogController(QObject *parent)
    : QObject(parent)    
{
    dialogW = new QDialog(0);
    dialog = new Ui::ConfigureDialog;
    acqPdParams = new Ui::AcqPDParams;
    acqTimedParams = new Ui::AcqTimedParams;
    aiDevRanges = DAQ::ProbeAllAIRanges();
    aoDevRanges = DAQ::ProbeAllAORanges();
    
    aiChanLists = DAQ::ProbeAllAIChannels();
    aoChanLists = DAQ::ProbeAllAOChannels();

    dialog->setupUi((QDialog *)dialogW);
    acqPdParamsW = new QWidget(dialog->acqFrame);
    acqTimedParamsW = new QWidget(dialog->acqFrame);
    dialogW->setWindowIcon(QIcon(QPixmap(Icon_Config_xpm)));
    acqPdParams->setupUi(acqPdParamsW);
    acqTimedParams->setupUi(acqTimedParamsW);
    
    // connect signal slots
    Connect(dialog->acqStartEndCB, SIGNAL(activated(int)), this, SLOT(acqStartEndCBChanged()));
    Connect(dialog->acqModeCB, SIGNAL(activated(int)), this, SLOT(acqModeCBChanged()));
    Connect(dialog->deviceCB, SIGNAL(activated(const QString &)), this, SLOT(deviceCBChanged()));
    Connect(dialog->aoDeviceCB, SIGNAL(activated(const QString &)), this, SLOT(aoDeviceCBChanged()));
    Connect(dialog->browseBut, SIGNAL(clicked()), this, SLOT(browseButClicked()));
    Connect(dialog->aoPassthruGB, SIGNAL(toggled(bool)), this, SLOT(aoPassthruChkd()));
    Connect(acqPdParams->pdPassthruAOChk, SIGNAL(toggled(bool)), this, SLOT(aoPDChanChkd()));
}

ConfigureDialogController::~ConfigureDialogController()
{
    delete acqPdParams;
    delete acqTimedParams;
    delete dialog;
    delete acqPdParamsW;
    delete acqTimedParamsW;
    delete dialogW;
}

void ConfigureDialogController::resetFromParams()
{
    loadSettings();
    DAQ::Params & p (acceptedParams); // this just got populated from settings

    // initialize the dialogs with some values from settings
    dialog->outputFileLE->setText(p.outputFile);
    if (int(p.mode) < dialog->acqModeCB->count())
        dialog->acqModeCB->setCurrentIndex((int)p.mode);
    dialog->aoPassthruLE->setText(p.aoPassthruString);
    dialog->deviceCB->clear();
    dialog->aoDeviceCB->clear();
    dialog->clockCB->setCurrentIndex(p.extClock ? 0 : 1);
    dialog->srateSB->setValue(p.srate);
    dialog->aoPassthruGB->setChecked(p.aoPassthru);
    int ci = dialog->aiTerminationCB->findText(DAQ::termConfigToString(p.aiTerm), Qt::MatchExactly|Qt::CaseInsensitive);
    dialog->aiTerminationCB->setCurrentIndex(ci > -1 ? ci : 0);
    if (int(p.doCtlChan) < dialog->doCtlCB->count())
        dialog->doCtlCB->setCurrentIndex(p.doCtlChan);
    dialog->channelListLE->setText(p.aiString);
    dialog->acqStartEndCB->setCurrentIndex((int)p.acqStartEndMode);
    acqPdParams->pdAIThreshSB->setValue(static_cast<int>(p.pdThresh) + 32768);
    acqPdParams->pdAISB->setValue(p.pdChan);
    acqPdParams->pdPassthruAOChk->setChecked(p.pdPassThruToAO > -1);
    acqPdParams->pdPassthruAOSB->setValue(p.pdPassThruToAO > -1 ? p.pdPassThruToAO : 0);   

    QList<QString> devs = aiChanLists.uniqueKeys();
    devNames.clear();
    int sel = 0, i = 0;
    for (QList<QString>::const_iterator it = devs.begin(); it != devs.end(); ++it, ++i) {
        if (p.dev == *it) sel = i;
        dialog->deviceCB->addItem(QString("%1 (%2)").arg(*it).arg(DAQ::GetProductName(*it)));
        devNames.push_back(*it);
    }    
    if ( dialog->deviceCB->count() ) // empty CB sometimes on missing AI??
        dialog->deviceCB->setCurrentIndex(sel);

    aoDevNames.clear();
    devs = aoChanLists.uniqueKeys();
    sel = 0, i = 0;
    for (QList<QString>::const_iterator it = devs.begin(); it != devs.end(); ++it, ++i) {
        if (p.aoDev == *it) sel = i;
        dialog->aoDeviceCB->addItem(QString("%1 (%2)").arg(*it).arg(DAQ::GetProductName(*it)));
        aoDevNames.push_back(*it);
    }
    if ( dialog->aoDeviceCB->count() ) // empty CB sometimes on missing AO??
        dialog->aoDeviceCB->setCurrentIndex(sel);
    
    dialog->disableGraphsChk->setChecked(p.suppressGraphs);
    
    // now the timed params stuff
    acqTimedParams->startHrsSB->setValue(int(p.startIn/(60.*60.)));
    acqTimedParams->startMinsSB->setValue(int((int(p.startIn)%(60*60))/60.));
    acqTimedParams->startSecsSB->setValue(int((int(p.startIn)%(60*60)))%60);
    acqTimedParams->durHrsSB->setValue(int(p.duration/(60.*60.)));
    acqTimedParams->durMinsSB->setValue(int((int(p.duration)%(60*60))/60.));
    acqTimedParams->durSecsSB->setValue((int(p.duration)%(60*60))%60);
    acqTimedParams->indefCB->setChecked(p.isIndefinite);
    acqTimedParams->nowCB->setChecked(p.isImmediate);
    
    // fire off the slots to polish?
    acqStartEndCBChanged();
    acqModeCBChanged();
    deviceCBChanged();
    aoDeviceCBChanged();
    aoPassthruChkd();
    aoPDChanChkd();
}

void ConfigureDialogController::acqStartEndCBChanged()
{
    acqPdParamsW->hide();
    acqTimedParamsW->hide();
    DAQ::AcqStartEndMode mode = (DAQ::AcqStartEndMode)dialog->acqStartEndCB->currentIndex();
    dialog->acqStartEndDescrLbl->hide();
    
    switch (mode) {
    case DAQ::Immediate:
        dialog->acqStartEndDescrLbl->setText("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\"><html><head><meta name=\"qrichtext\" content=\"1\" /><style type=\"text/css\">p, li { white-space: pre-wrap; }</style></head><body style=\" font-family:'Sans Serif'; font-size:9pt; font-weight:400; font-style:normal;\"><p style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:10pt; font-style:italic; \">The acquisition will start immediately.</span></p></body></html>");
        dialog->acqStartEndDescrLbl->show();
        break;
    case DAQ::PDStartEnd:
    case DAQ::PDStart:
        acqPdParamsW->setParent(dialog->acqFrame);
        acqPdParamsW->show();
        break;
    case DAQ::Timed:
        acqTimedParamsW->setParent(dialog->acqFrame);
        acqTimedParamsW->show();
        break;
    case DAQ::StimGLStartEnd:
        dialog->acqStartEndDescrLbl->setText("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\"><html><head><meta name=\"qrichtext\" content=\"1\" /><style type=\"text/css\">p, li { white-space: pre-wrap; }</style></head><body style=\" font-family:'Sans Serif'; font-size:9pt; font-weight:400; font-style:normal;\"><p style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:10pt; font-style:italic; color:#294928;\">The acquisition will be triggered to start and end by the external StimGL II program.</span></p></body></html>");
        dialog->acqStartEndDescrLbl->show();
        break;
    case DAQ::StimGLStart:
        dialog->acqStartEndDescrLbl->setText("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\"><html><head><meta name=\"qrichtext\" content=\"1\" /><style type=\"text/css\">p, li { white-space: pre-wrap; }</style></head><body style=\" font-family:'Sans Serif'; font-size:9pt; font-weight:400; font-style:normal;\"><p style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:10pt; font-style:italic; color:#294928;\">The acquisition will be triggered to start by the external StimGL II program.</span></p></body></html>");
        dialog->acqStartEndDescrLbl->show();
        break;
    }
}

void ConfigureDialogController::acqModeCBChanged()
{
    int idx = dialog->acqModeCB->currentIndex();
    bool enabled = idx == 1;
    bool intan = !enabled;

    dialog->srateLabel->setEnabled(enabled);
    dialog->srateSB->setEnabled(enabled);
    dialog->clockCB->setEnabled(enabled);

    if (intan) {
        dialog->srateSB->setValue(INTAN_SRATE);    
        dialog->clockCB->setCurrentIndex(0);
    }
    dialog->doCtlLabel->setEnabled(intan);
    dialog->doCtlCB->setEnabled(intan);
}

void ConfigureDialogController::deviceCBChanged()
{
    if (!dialog->deviceCB->count()) return;
    QString devStr = devNames[dialog->deviceCB->currentIndex()];
    QList<DAQ::Range> ranges = aiDevRanges.values(devStr);
    QString curr = dialog->aiRangeCB->currentText();

    // do control combo box setup..
    int doCtl = dialog->doCtlCB->currentIndex();
    QStringList DOS = DAQ::GetDOChans(devStr);
    if (doCtl < 0) doCtl = 0;
    dialog->doCtlCB->clear();
    
    for (int i = 0; i < DOS.count(); ++i) {
        dialog->doCtlCB->addItem(DOS.at(i).section('/',1,-1));
    }
    if (doCtl < dialog->doCtlCB->count())
        dialog->doCtlCB->setCurrentIndex(doCtl);

    if (!curr.length()) {
        curr = QString("%1 - %2").arg(acceptedParams.range.min).arg(acceptedParams.range.max);
    }
    int sel = 0, i = 0;
    dialog->aiRangeCB->clear();
    for (QList<DAQ::Range>::const_iterator it = ranges.begin(); it != ranges.end(); ++it, ++i) {
        const DAQ::Range & r (*it);
        QString txt = QString("%1 - %2").arg(r.min).arg(r.max);
        if (txt == curr) sel = i;
        dialog->aiRangeCB->insertItem(i, txt);
    }
    if (dialog->aiRangeCB->count())
        dialog->aiRangeCB->setCurrentIndex(sel);

    dialog->srateSB->setMinimum((unsigned)DAQ::MinimumSampleRate(devStr));
    dialog->srateSB->setMaximum((unsigned)DAQ::MaximumSampleRate(devStr));
}

void ConfigureDialogController::aoDeviceCBChanged()
{
    if (!dialog->aoDeviceCB->count()) return;
    QString devStr = aoDevNames[dialog->aoDeviceCB->currentIndex()];
    QList<DAQ::Range> ranges = aoDevRanges.values(devStr);
    QString curr = dialog->aoRangeCB->currentText();

    if (!curr.length()) {
        curr = QString("%1 - %2").arg(acceptedParams.aoRange.min).arg(acceptedParams.aoRange.max);
    }
    int sel = 0, i = 0;
    dialog->aoRangeCB->clear();
    for (QList<DAQ::Range>::const_iterator it = ranges.begin(); it != ranges.end(); ++it, ++i) {
        const DAQ::Range & r (*it);
        QString txt = QString("%1 - %2").arg(r.min).arg(r.max);
        if (txt == curr) sel = i;
        dialog->aoRangeCB->insertItem(i, txt);
    }
    if (dialog->aoRangeCB->count())
        dialog->aoRangeCB->setCurrentIndex(sel);
}

void ConfigureDialogController::browseButClicked()
{
    QString fn = QFileDialog::getSaveFileName(dialogW, "Select output file", dialog->outputFileLE->text());
    if (fn.length()) dialog->outputFileLE->setText(fn);
}

void ConfigureDialogController::aoPassthruChkd()
{
    if (!dialog->aoDeviceCB->count()) {
        // if no AO subdevice present on the current machine, just disable all GUI related to AO and return
        acqPdParams->pdPassthruAOChk->setEnabled(false);
        acqPdParams->pdPassthruAOChk->setChecked(false); 
        acqPdParams->pdPassthruAOSB->setEnabled(false);
        dialog->aoPassthruGB->setChecked(false);
        dialog->aoPassthruGB->setEnabled(false);
        QString theTip = "AO missing on installed hardware, AO passthru unavailable.";
        dialog->aoPassthruGB->setToolTip(theTip);
        acqPdParams->pdPassthruAOChk->setToolTip(theTip); 
        acqPdParams->pdPassthruAOSB->setToolTip(theTip);
        return;
    }
    bool chk = dialog->aoPassthruGB->isChecked();
    acqPdParams->pdPassthruAOChk->setEnabled(chk);
    if (!chk && acqPdParams->pdPassthruAOChk->isChecked()) 
        acqPdParams->pdPassthruAOChk->setChecked(false);    
    acqPdParams->pdPassthruAOSB->setEnabled(chk && acqPdParams->pdPassthruAOChk->isChecked());
}

void ConfigureDialogController::aoPDChanChkd()
{
    bool chk = acqPdParams->pdPassthruAOChk->isChecked();
    acqPdParams->pdPassthruAOSB->setEnabled(chk && dialog->aoPassthruGB->isChecked() );
}

int ConfigureDialogController::exec()
{
    bool again;
    int ret;

    resetFromParams();

    do {
        ret = ((QDialog *)dialogW)->exec();
        again = false;

        // TODO process/validate form here... reject on invalid params!
    
        if (ret == QDialog::Accepted) {
            const QString dev = dialog->deviceCB->count() ? devNames[dialog->deviceCB->currentIndex()] : "";
            const QString aoDev = dialog->aoDeviceCB->count() ? aoDevNames[dialog->aoDeviceCB->currentIndex()] : "";

            bool err;
            QVector<unsigned> chanVect;
            QString chans = parseAIChanString(dialog->channelListLE->text(), chanVect, &err);
            if (err) {
                QMessageBox::critical(dialogW, "AI Chan List Error", "Error parsing AI channel list!\nSpecify a string of the form 0,1,2,3 or 0-3,5,6 etc!");
                again = true;
                continue;
            }
            const DAQ::Mode acqMode = (DAQ::Mode)dialog->acqModeCB->currentIndex();

            if ( (acqMode == DAQ::AI60Demux || acqMode == DAQ::AI120Demux)
                 && !DAQ::SupportsAISimultaneousSampling(dev) ) {
                QMessageBox::critical(dialogW, "INTAN Requires Simultaneous Sampling", QString("INTAN (60/120 demux) mode requires a board that supports simultaneous sampling, and %1 does not!").arg(dev));
                again = true;
                continue;
            }

            if ( (acqMode == DAQ::AI60Demux && chanVect.size() != 4)
                 || (acqMode == DAQ::AI120Demux && chanVect.size() != 8) ) {
                QMessageBox::critical(dialogW, "AI Chan List Error", "INTAN (60/120 demux) mode requires precisely 4 or 8 channels!");
                again = true;
                continue;                
            }
            if (!chanVect.size()) {
                QMessageBox::critical(dialogW, "AI Chan List Error", "Need at least 1 channel in the AI channel list!");
                again = true;
                continue;
            }
            
            QMap<unsigned, unsigned> aopass;            
            if (dialog->aoPassthruGB->isChecked() && aoDevNames.count()) {
                aopass = parseAOPassthruString(dialog->aoPassthruLE->text(), &err);
                if (err) {
                    QMessageBox::critical(dialogW, "AO Passthru Error", "Error parsing AO Passthru list, specify a string of the form UNIQUE_AOCHAN=CHANNEL_INDEX_POS!");
                    again = true;
                    continue;
                }
            }
            QVector<unsigned> aoChanVect = aopass.uniqueKeys().toVector();

            const unsigned srate = dialog->srateSB->value();
            if (srate > DAQ::MaximumSampleRate(dev, chanVect.size())) {
                QMessageBox::critical(dialogW, "Sampling Rate Invalid", QString().sprintf("The sample rate specified (%d) is too high for the number of channels (%d)!", (int)srate, (int)chanVect.size()));
                again = true;
                continue;
            }

            const DAQ::AcqStartEndMode acqStartEndMode = (DAQ::AcqStartEndMode)dialog->acqStartEndCB->currentIndex();

            int pdChan = acqPdParams->pdAISB->value();
            if ((acqStartEndMode == DAQ::PDStartEnd || acqStartEndMode == DAQ::PDStart)
                && (chanVect.contains(pdChan) || pdChan < 0 || pdChan >= aiChanLists[dev].count())
                ) {
                QMessageBox::critical(dialogW, "PDChannel Invalid", QString().sprintf("Specified photodiode channel (%d) is invalid or clashes with another AI channel!", pdChan));
                again = true;
                continue;
            }
            int pdAOChan = acqPdParams->pdPassthruAOSB->value();
            if ((acqStartEndMode == DAQ::PDStartEnd || acqStartEndMode == DAQ::PDStart)
                && acqPdParams->pdPassthruAOChk->isChecked() 
                && (aoChanVect.contains(pdAOChan) || pdAOChan < 0 || pdAOChan >= aoChanLists[aoDev].count())) {
                QMessageBox::critical(dialogW, "PDChannel AO Invalid", QString().sprintf("Specified photodiode AO passthru channel (%d) is invalid or clashes with another AO passthru channel!", pdAOChan));
                again = true;
                continue;
            } 
            unsigned nVAI = chanVect.size();
            bool isMux = acqMode == DAQ::AI60Demux || acqMode == DAQ::AI120Demux;
            if (isMux) nVAI *= MUX_CHANS_PER_PHYS_CHAN;

            bool usePD = false;
            if (acqStartEndMode == DAQ::PDStart || acqStartEndMode == DAQ::PDStartEnd) {
                chanVect.push_back(pdChan);
                ++nVAI;
                usePD = true;
            }

            if (!acqPdParams->pdPassthruAOChk->isChecked()) {
                pdAOChan = -1;
            } else {
                aoChanVect.push_back(pdAOChan);
                aopass[pdAOChan]=chanVect.indexOf(pdChan);
            }

            // validate AO channels in AO passthru map
            if (aoChanVect.size()) {
                QStringList aol = aoChanLists[aoDev];
                for (int i = 0; i < (int)aoChanVect.size(); ++i) {
                    if ((int)aoChanVect[i] >= (int)aol.size()) {
                        QMessageBox::critical(dialogW, "AO Passthru Error", QString().sprintf("The AO channel specified in the passthru string (%d) is illegal/invalid!", (int)(aoChanVect[i])));
                        again = true;
                        break;
                    }
                }
                if (again) continue;
            }

            {
                // validate AI channels in AO passthru map
                QList<unsigned> vl = aopass.values();
                for (QList<unsigned>::const_iterator it = vl.begin(); it != vl.end(); ++it)
                    if (*it >= nVAI) {
                        QMessageBox::critical(dialogW, "AO Passthru Invalid", QString().sprintf("Chan index %d specified as an AO passthru source but it is > the number of virtual AI channels (%d)!", (int)*it, (int)nVAI));
                        again = true;
                        break;
                    }
                if (again) continue;
            }

            DAQ::Params & p (acceptedParams);
            p.outputFile = dialog->outputFileLE->text();
            p.dev = dev;
            QStringList rngs = dialog->aiRangeCB->currentText().split(" - ");
            if (rngs.count() != 2) {
                Error() << "INTERNAL ERROR: AI Range ComboBox needs numbers of the form REAL - REAL!";
                return QDialog::Rejected;
            }
            p.range.min = rngs.first().toDouble();
            p.range.max = rngs.last().toDouble();
            p.mode = acqMode;
            p.srate = srate;
            p.extClock = dialog->clockCB->currentIndex() == 0;
            p.aiChannels = chanVect;
            p.nVAIChans = nVAI;
            p.aiString = chans;
            p.doCtlChan = dialog->doCtlCB->currentIndex();
            p.doCtlChanString = QString("%1/%2").arg(p.dev).arg(dialog->doCtlCB->currentText());
            p.usePD = usePD;
            if (!aoDevNames.empty()) {                
                p.aoPassthru = dialog->aoPassthruGB->isChecked();
                p.aoDev = aoDev;
                p.aoSrate = p.srate;
                rngs = dialog->aoRangeCB->currentText().split(" - ");
                if (rngs.count() != 2) {
                    Error() << "INTERNAL ERROR: AO Range ComboBox needs numbers of the form REAL - REAL!";
                    return QDialog::Rejected;
                }            
                p.aoRange.min = rngs.first().toDouble();
                p.aoRange.max = rngs.last().toDouble();
                p.aoPassthruMap = aopass;
                p.aoChannels = aoChanVect;
                p.aoPassthruString = dialog->aoPassthruLE->text();
            } else {
                p.aoPassthruMap.clear();
                p.aoChannels.clear();
                p.aoPassthruString = "";
                p.aoDev = "";
                p.aoPassthru = false;
            }
            p.suppressGraphs = dialog->disableGraphsChk->isChecked();

            p.acqStartEndMode = acqStartEndMode;
            p.isIndefinite = acqTimedParams->indefCB->isChecked();
            p.isImmediate = acqTimedParams->nowCB->isChecked();
            p.startIn = acqTimedParams->startHrsSB->value()*60.*60. 
                       +acqTimedParams->startMinsSB->value()*60
                       +acqTimedParams->startSecsSB->value();
            p.duration = acqTimedParams->durHrsSB->value()*60.*60. 
                        +acqTimedParams->durMinsSB->value()*60
                        +acqTimedParams->durSecsSB->value();
            
            p.pdChan = pdChan;
            p.pdThresh = static_cast<signed short>(acqPdParams->pdAIThreshSB->value() - 32768);
            p.pdPassThruToAO = pdAOChan;

            p.aiTerm = DAQ::toTermConfig(dialog->aiTerminationCB->currentText());
            
            saveSettings();
        }
    } while (again);

    return ret;
}

/*static*/ 
QString
ConfigureDialogController::parseAIChanString(const QString & str, 
                                             QVector<unsigned> & aiChannels,
                                             bool *parse_error) 
{
    QString ret("");
    QVector<unsigned> aiChans;
    QSet<unsigned> aiSet;

    QStringList vals = str.split(",", QString::SkipEmptyParts);
    for (QStringList::iterator it = vals.begin(); it != vals.end(); ++it) {
        QString v = (*it).trimmed();
        QStringList ranges = v.split(":", QString::SkipEmptyParts);
        if (ranges.count() == 2) {
            bool ok1, ok2;
            unsigned f = ranges.first().toUInt(&ok1), l = ranges.last().toUInt(&ok2);
            if (!ok1 || !ok2 || f >= l) {
                if (parse_error) *parse_error = true;
                return QString();                
            }
            for (unsigned i = f; i <= l; ++i) {
                if (aiSet.contains(i)) { // parse error! dupe chan!
                    Error() << "parseAIChanString: duplicate channel in channel list: " << i << "!";
                    if (parse_error) *parse_error = true;
                    return QString();                                    
                }
                aiChans.push_back(i);
                aiSet.insert(i);
            }

            ret.append(QString().sprintf("%s%d:%d", (ret.length()?",":""), f, l));
        } else if (ranges.count() == 1) {
            bool ok;
            unsigned i = ranges.first().toUInt(&ok);
            if (!ok) {
                if (parse_error) *parse_error = true;
                return QString();                
            }
            if (aiSet.contains(i)) { // parse error! dupe chan!
                Error() << "parseAIChanString: duplicate channel in channel list: " << i << "!";
                if (parse_error) *parse_error = true;
                return QString();                                    
            }
            aiChans.push_back(i);
            aiSet.insert(i);
            ret.append(QString().sprintf("%s%d", (ret.length()?",":""), i));
        } else {
            if (parse_error) *parse_error = true;
            return QString();                
        }
    }
    if (!ret.length() && parse_error) *parse_error = true;
    else if (parse_error) *parse_error = false;
    aiChannels = aiChans;
    return ret;
}
 
/*static*/
QMap<unsigned,unsigned> 
ConfigureDialogController::parseAOPassthruString(const QString & str,
                                                 bool *parse_error)
{
    QMap<unsigned,unsigned> ret;

    QStringList vals = str.split(",", QString::SkipEmptyParts);
    for (QStringList::iterator it = vals.begin(); it != vals.end(); ++it) {
        QString v = (*it).trimmed();
        QStringList nvp = v.split("=", QString::SkipEmptyParts);
        if (nvp.count() == 2) {
            bool ok1, ok2;
            unsigned n = nvp.first().toUInt(&ok1), v = nvp.last().toUInt(&ok2);
            if (!ok1 || !ok2) {
                if (parse_error) *parse_error = true;
                return QMap<unsigned,unsigned>();                
            }
            if (!ret.contains(n))
                ret.insert(n,v);
            else {
                if (parse_error) *parse_error = true;
                return QMap<unsigned,unsigned>();
            }
        } else {
            if (parse_error) *parse_error = true;
            return QMap<unsigned,unsigned>();
        }
    }
    if (ret.empty() && parse_error) *parse_error = true;
    else if (parse_error) *parse_error = false;
    return ret;
}

void ConfigureDialogController::loadSettings()
{
    DAQ::Params & p(acceptedParams);

    QSettings settings("janelia.hhmi.org", "LeoDAQGL");

    settings.beginGroup("ConfigureDialogController");

    p.outputFile = mainApp()->outputDirectory() + PATH_SEPARATOR + settings.value("lastOutFile", "data.bin").toString();
    p.dev = settings.value("dev", "").toString();
    p.doCtlChan = settings.value("doCtlChan", "0").toUInt();
    p.range.min = settings.value("rangeMin", "-2.5").toDouble();
    p.range.max = settings.value("rangeMax", "2.5").toDouble();
    p.mode = (DAQ::Mode)settings.value("acqMode", 0).toInt();
    p.srate = settings.value("srate", INTAN_SRATE).toUInt();
    p.extClock = settings.value("extClock", true).toBool();
    p.aiString = settings.value("aiString", "0:3").toString();
    p.aoPassthru = settings.value("aoPassthru", false).toBool();
    p.aoPassthruString = settings.value("aoPassthruString", "0=1,1=2").toString();
    p.aoDev = settings.value("aoDev", "").toString();
    p.suppressGraphs = settings.value("suppressGraphs", false).toBool();

    p.acqStartEndMode =  (DAQ::AcqStartEndMode)settings.value("acqStartEndMode", 0).toInt();
    p.isImmediate = settings.value("acqStartTimedImmed", false).toBool();
    p.isIndefinite = settings.value("acqStartTimedIndef", false).toBool();
    p.startIn = settings.value("acqStartTimedTime", 0.0).toDouble();
    p.duration = settings.value("acqStartTimedDuration", 60.0).toDouble();

    p.pdThresh = settings.value("acqPDThresh", 48000-32768).toInt();
    p.pdChan = settings.value("acqPDChan", 4).toInt();
    p.pdPassThruToAO = settings.value("acqPDPassthruChanAO", 2).toInt();

    p.aiTerm = (DAQ::TermConfig)settings.value("aiTermConfig", (int)DAQ::Default).toInt();
}

void ConfigureDialogController::saveSettings()
{
    DAQ::Params & p(acceptedParams);

    QSettings settings("janelia.hhmi.org", "LeoDAQGL");

    settings.beginGroup("ConfigureDialogController");
    
    settings.setValue("lastOutFile", QFileInfo(p.outputFile).fileName());
    QString path = QFileInfo(p.outputFile).path();
    mainApp()->setOutputDirectory(path.length() ? path : QString(PATH_SEPARATOR));
    settings.setValue("dev", p.dev);
    settings.setValue("rangeMin", p.range.min);
    settings.setValue("rangeMax", p.range.max);
    settings.setValue("acqMode", (int)p.mode);
    settings.setValue("doCtlChan", p.doCtlChan);
    settings.setValue("srate", p.srate);
    settings.setValue("extClock", p.extClock);
    settings.setValue("aiString", p.aiString);
    settings.setValue("aoDev", p.aoDev);
    settings.setValue("aoPassthru", p.aoPassthru);
    settings.setValue("aoPassthruString", p.aoPassthruString);
    settings.setValue("suppressGraphs", p.suppressGraphs);    
    
    settings.setValue("acqStartEndMode", (int)p.acqStartEndMode);
    settings.setValue("acqStartTimedImmed", p.isImmediate);
    settings.setValue("acqStartTimedIndef", p.isIndefinite);
    settings.setValue("acqStartTimedTime", p.startIn);
    settings.setValue("acqStartTimedDuration", p.duration);

    settings.setValue("acqPDThresh", p.pdThresh);
    settings.setValue("acqPDChan", p.pdChan);
    settings.setValue("acqPDPassthruChanAO", p.pdPassThruToAO);
    settings.setValue("aiTermConfig", (int)p.aiTerm);
}
