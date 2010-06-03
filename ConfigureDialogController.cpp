#include "ConfigureDialogController.h"
#include "SpikeGL.h"
#include "Util.h"
#include "MainApp.h"
#include "ui_Dialog.h"
#include <QTimeEdit>
#include <QTime>
#include <QMessageBox>
#include <QSet>
#include <QSettings>
#include <QFileInfo>
#include <QFileDialog>
#include <QRegExp>
#include <QTextStream>
#include "Icon-Config.xpm"
#include "ConsoleWindow.h"

#define SETTINGS_DOMAIN "janelia.hhmi.org"
#define SETTINGS_APP "SpikeGL"
#define SETTINGS_GROUP "ConfigureDialogController"

ConfigureDialogController::ConfigureDialogController(QObject *parent)
    : QObject(parent), chanMapCtl(this)
{
    aoPassW = 0;
    applyDlg = 0;
    dialogW = new QDialog(0);
    dialog = new Ui::ConfigureDialog;
    aoPassthru = new Ui::AoPassThru;
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
    aoPassthru->setupUi(dialog->aoPassthruWidget);
    acceptedParams.mode = DAQ::AIUnknown;
    // connect signal slots
    Connect(dialog->acqStartEndCB, SIGNAL(activated(int)), this, SLOT(acqStartEndCBChanged()));
    Connect(dialog->acqModeCB, SIGNAL(activated(int)), this, SLOT(acqModeCBChanged()));
    Connect(dialog->deviceCB, SIGNAL(activated(const QString &)), this, SLOT(deviceCBChanged()));
    Connect(aoPassthru->aoDeviceCB, SIGNAL(activated(const QString &)), this, SLOT(aoDeviceCBChanged()));
    Connect(dialog->browseBut, SIGNAL(clicked()), this, SLOT(browseButClicked()));
    Connect(aoPassthru->aoPassthruGB, SIGNAL(toggled(bool)), this, SLOT(aoPassthruChkd()));
    Connect(acqPdParams->pdPassthruAOChk, SIGNAL(toggled(bool)), this, SLOT(aoPDChanChkd()));
    Connect(dialog->muxMapBut, SIGNAL(clicked()), &chanMapCtl, SLOT(exec()));
    Connect(dialog->aiRangeCB, SIGNAL(currentIndexChanged(int)), this, SLOT(aiRangeChanged()));
    Connect(acqPdParams->pdPassthruAOSB, SIGNAL(valueChanged(int)), this, SLOT(aoPDPassthruUpdateLE()));
}

ConfigureDialogController::~ConfigureDialogController()
{
    delete acqPdParams;
    delete acqTimedParams;
    delete dialog;
    delete applyDlg;
    delete acqPdParamsW;
    delete acqTimedParamsW;
    delete dialogW;
    delete aoPassW;
}

void ConfigureDialogController::resetAOPassFromParams(Ui::AoPassThru *aoPassthru)
{
    DAQ::Params & p (acceptedParams); // this just got populated from settings
    aoPassthru->aoPassthruLE->setText(p.aoPassthruString);
    aoPassthru->aoDeviceCB->clear();
    aoPassthru->aoPassthruGB->setCheckable(Util::objectHasAncestor(aoPassthru->aoPassthruGB,dialogW));
    aoPassthru->aoPassthruGB->setChecked(p.aoPassthru);
    aoDevNames.clear();
    QList<QString> devs = aoChanLists.uniqueKeys();
    int sel = 0, i = 0;
    for (QList<QString>::const_iterator it = devs.begin(); it != devs.end(); ++it, ++i) {
        if (p.aoDev == *it) sel = i;
        aoPassthru->aoDeviceCB->addItem(QString("%1 (%2)").arg(*it).arg(DAQ::GetProductName(*it)));
        aoDevNames.push_back(*it);
    }
    if ( aoPassthru->aoDeviceCB->count() ) // empty CB sometimes on missing AO??
        aoPassthru->aoDeviceCB->setCurrentIndex(sel);
    
}

void ConfigureDialogController::resetFromParams(DAQ::Params *p_in)
{
    if (!p_in) {
        loadSettings();
        p_in = &acceptedParams;
    }
    
    DAQ::Params & p (*p_in); // this just got populated from settings

    // initialize the dialogs with some values from settings
    dialog->outputFileLE->setText(p.outputFile);
    if (int(p.mode) < dialog->acqModeCB->count())
        dialog->acqModeCB->setCurrentIndex((int)p.mode);    
    dialog->channelSubsetLE->setText(p.subsetString);
    dialog->channelSubsetLabel->setEnabled(p.mode != DAQ::AIRegular);
    dialog->channelSubsetLE->setEnabled(p.mode != DAQ::AIRegular);
    dialog->deviceCB->clear();
    dialog->clockCB->setCurrentIndex(p.extClock ? 0 : 1);
    dialog->srateSB->setValue(p.srate);
    dialog->fastSettleSB->setValue(p.fastSettleTimeMS);
    dialog->auxGainSB->setValue(p.auxGain);
    dialog->stimGLReopenCB->setChecked(p.stimGlTrigResave);
    int ci = dialog->aiTerminationCB->findText(DAQ::TermConfigToString(p.aiTerm), Qt::MatchExactly);
    dialog->aiTerminationCB->setCurrentIndex(ci > -1 ? ci : 0);
    if (int(p.doCtlChan) < dialog->doCtlCB->count())
        dialog->doCtlCB->setCurrentIndex(p.doCtlChan);
    dialog->channelListLE->setText(p.aiString);
    dialog->acqStartEndCB->setCurrentIndex((int)p.acqStartEndMode);
    acqPdParams->pdAIThreshSB->setValue((p.pdThresh/32768.+1.)/2. * (p.range.max-p.range.min) + p.range.min);
    acqPdParams->pdAISB->setValue(p.pdChan);
    acqPdParams->pdPassthruAOChk->setChecked(p.pdPassThruToAO > -1);
    acqPdParams->pdPassthruAOSB->setValue(p.pdPassThruToAO > -1 ? p.pdPassThruToAO : 0);   
    acqPdParams->pdStopTimeSB->setValue(p.pdStopTime);
    acqPdParams->pdPre->setValue(p.silenceBeforePD*1000.);

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


    resetAOPassFromParams(aoPassthru);
    
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
    aiRangeChanged();
}

void ConfigureDialogController::aiRangeChanged()
{
    QString devStr = devNames[dialog->deviceCB->currentIndex()];
    const QList<DAQ::Range> ranges = aiDevRanges.values(devStr);
	if (!ranges.count()) {
		QMessageBox::critical(dialogW, "NI Unknown Error", "Error with your NIDAQ setup.  Please make sure all ghost/phantom/unused devices are deleted from your NI Measurement & Autiomation Explorer", QMessageBox::Abort);
		QApplication::exit(1);
		return;
	}
    const DAQ::Range r = ranges[dialog->aiRangeCB->currentIndex()];
    acqPdParams->pdAIThreshSB->setMinimum(r.min);
    acqPdParams->pdAIThreshSB->setMaximum(r.max);
}

void ConfigureDialogController::acqStartEndCBChanged()
{
    bool entmp = false;
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
        entmp = true;
    case DAQ::PDStart:
        acqPdParams->pdStopTimeLbl->setEnabled(entmp);
        acqPdParams->pdStopTimeSB->setEnabled(entmp);
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
    default:
        Error() << "INTERNAL ERROR: INVALID ACQSTARTENDMODE!  FIXME!";
        break;
    }
}

void ConfigureDialogController::acqModeCBChanged()
{
    const int idx = dialog->acqModeCB->currentIndex();
    const bool jfrc32 = idx == 3;
    const bool enabled = idx == 1 || jfrc32;
    const bool intan = !enabled;    
    bool notStraightAI;
    
    /// HACK to auto-repopulate the lineedit based on acquisition mode!
    if ( (notStraightAI = (idx != DAQ::AIRegular)) ) { // if it's not straight AI, 
        QString txt = dialog->channelListLE->text();
        if (txt.startsWith("0:1") || txt.startsWith("0:3") || txt.startsWith("0:7"))  {
            switch (idx) {
                case DAQ::AI60Demux: txt = QString("0:3") + txt.mid(3); break;
                case DAQ::AI120Demux: txt = QString("0:7") + txt.mid(3); break;
                case DAQ::JFRCIntan32: txt = QString("0:1") + txt.mid(3); break;
            }
            dialog->channelListLE->setText(txt);
        }
    }
    dialog->channelSubsetLE->setEnabled(notStraightAI);
    dialog->channelSubsetLabel->setEnabled(notStraightAI);
    
    dialog->srateLabel->setEnabled(enabled);
    dialog->srateSB->setEnabled(enabled);
    dialog->clockCB->setEnabled(false); // NB: for now, the clockCB is always disabled!
    dialog->muxMapBut->setEnabled(intan);

    if (intan || jfrc32) {
        if (!jfrc32) dialog->srateSB->setValue(INTAN_SRATE);
        dialog->clockCB->setCurrentIndex(0); // intan always uses external clock
    } else {
        dialog->clockCB->setCurrentIndex(1); // force INTERNAL clock on !intan
    }
    dialog->doCtlLabel->setEnabled(intan);
    dialog->doCtlCB->setEnabled(intan);
    dialog->fastSettleSB->setEnabled(intan);
    dialog->fastSettleLbl->setEnabled(intan);
}

void ConfigureDialogController::deviceCBChanged()
{
    if (!dialog->deviceCB->count()) return;
    QString devStr = devNames[dialog->deviceCB->currentIndex()];
    QList<DAQ::Range> ranges = aiDevRanges.values(devStr);
    QString curr = dialog->aiRangeCB->count() ? dialog->aiRangeCB->currentText() : QString::null;
    if (curr.isEmpty()) 
        // make the current selected gain be the saved params
        curr = QString("%1 - %2").arg(acceptedParams.range.min).arg(acceptedParams.range.max);
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
    if (!aoPassthru->aoDeviceCB->count()) return;
    QString devStr = aoDevNames[aoPassthru->aoDeviceCB->currentIndex()];
    QList<DAQ::Range> ranges = aoDevRanges.values(devStr);
    QString curr = aoPassthru->aoRangeCB->currentText();

    if (!curr.length()) {
        curr = QString("%1 - %2").arg(acceptedParams.aoRange.min).arg(acceptedParams.aoRange.max);
    }
    int sel = 0, i = 0;
    aoPassthru->aoRangeCB->clear();
    for (QList<DAQ::Range>::const_iterator it = ranges.begin(); it != ranges.end(); ++it, ++i) {
        const DAQ::Range & r (*it);
        QString txt = QString("%1 - %2").arg(r.min).arg(r.max);
        if (txt == curr) sel = i;
        aoPassthru->aoRangeCB->insertItem(i, txt);
    }
    if (aoPassthru->aoRangeCB->count())
        aoPassthru->aoRangeCB->setCurrentIndex(sel);
}

void ConfigureDialogController::browseButClicked()
{
    QString fn = QFileDialog::getSaveFileName(dialogW, "Select output file", dialog->outputFileLE->text());
    if (fn.length()) dialog->outputFileLE->setText(fn);
}

void ConfigureDialogController::aoPassthruChkd()
{
    if (!aoPassthru->aoDeviceCB->count()) {
        // if no AO subdevice present on the current machine, just disable all GUI related to AO and return
        acqPdParams->pdPassthruAOChk->setEnabled(false);
        acqPdParams->pdPassthruAOChk->setChecked(false); 
        acqPdParams->pdPassthruAOSB->setEnabled(false);
        aoPassthru->aoPassthruGB->setChecked(false);
        aoPassthru->aoPassthruGB->setEnabled(false);
        QString theTip = "AO missing on installed hardware, AO passthru unavailable.";
        aoPassthru->aoPassthruGB->setToolTip(theTip);
        acqPdParams->pdPassthruAOChk->setToolTip(theTip); 
        acqPdParams->pdPassthruAOSB->setToolTip(theTip);
        return;
    }
    bool chk = aoPassthru->aoPassthruGB->isChecked();
    acqPdParams->pdPassthruAOChk->setEnabled(chk);
    if (!chk && acqPdParams->pdPassthruAOChk->isChecked()) 
        acqPdParams->pdPassthruAOChk->setChecked(false);    
    acqPdParams->pdPassthruAOSB->setEnabled(chk && acqPdParams->pdPassthruAOChk->isChecked());
    aoPDPassthruUpdateLE();
}

void ConfigureDialogController::aoPDChanChkd()
{
    bool chk = acqPdParams->pdPassthruAOChk->isChecked();
    acqPdParams->pdPassthruAOSB->setEnabled(chk && aoPassthru->aoPassthruGB->isChecked() );
    aoPDPassthruUpdateLE();
}


void ConfigureDialogController::aoPDPassthruUpdateLE()
{
    bool aochk = aoPassthru->aoPassthruGB->isChecked(),
         pdaochk = acqPdParams->pdPassthruAOChk->isChecked();
    int aopd = pdaochk && aochk ? acqPdParams->pdPassthruAOSB->value() : -1;
    QString le = aoPassthru->aoPassthruLE->text();
    le = le.remove(QRegExp(",?\\s*\\d+\\s*=\\s*PDCHAN"));
    if (le.startsWith(",")) le = le.mid(1);
    if (aopd > -1) {
        QString s = QString().sprintf("%d=PDCHAN",aopd);
        le = le + QString(le.length() ? ", " : "") + s;
    }
    aoPassthru->aoPassthruLE->setText(le);
}

/** Validates the internal form.  If ok, saves form to settings as side-effect, otherwise if not OK, sets errTitle and errMsg.
 *  OK returned, form is ok, proceed -- form is saved to settings
 *  AGAIN returned, form is erroneous, but redo ok -- form is not saved, errTitle and errMsg set appropriately
 *  ABORT returned, form is erroneous, abort altogether -- form is not saved, errTitle and errMsg set appropriately
 */
ConfigureDialogController::ValidationResult ConfigureDialogController::validateForm(QString & errTitle, QString & errMsg) 
{
    const QString dev = dialog->deviceCB->count() ? devNames[dialog->deviceCB->currentIndex()] : "";
    const QString aoDev = aoPassthru->aoDeviceCB->count() ? aoDevNames[aoPassthru->aoDeviceCB->currentIndex()] : "";
    
    bool err = false;
    errTitle = errMsg = "";
    QVector<unsigned> chanVect;
    QString chans = parseAIChanString(dialog->channelListLE->text(), chanVect, &err);
    for (int i = 0; i < (int)chanVect.size(); ++i) {
        if (chanVect[i] >= static_cast<unsigned>(aiChanLists[dev].size())) {
            err = true; 
            break;
        }
    }
    if (err) {
        errTitle = "AI Chan List Error";
        errMsg = "Error parsing AI channel list!\nSpecify a string of the form 0,1,2,3 or 0-3,5,6 of valid channel id's!";
        return AGAIN;
    }
    const DAQ::Mode acqMode = (DAQ::Mode)dialog->acqModeCB->currentIndex();
    
    int nExtraChans = 0;
    
    if ( acqMode == DAQ::AI60Demux || acqMode == DAQ::AI120Demux || acqMode == DAQ::JFRCIntan32) {
        if (!DAQ::SupportsAISimultaneousSampling(dev)) {
            errTitle = "INTAN Requires Simultaneous Sampling";
            errMsg = QString("INTAN (60/120/JFRC32 demux) mode requires a board that supports simultaneous sampling, and %1 does not!").arg(dev);
            return AGAIN;
        }
        const int minChanSize = acqMode == DAQ::AI120Demux ? 8 : (acqMode == DAQ::JFRCIntan32 ? 2 : 4);
        if ( int(chanVect.size()) < minChanSize ) {
            errTitle = "AI Chan List Error", errMsg = "INTAN (60/120/JFRC32 demux) mode requires precisely 4, 8 or 2 channels!";
            return AGAIN;
        }
        nExtraChans = chanVect.size() - minChanSize;
    }
    
    if (!chanVect.size()) {
        errTitle = "AI Chan List Error", errMsg = "Need at least 1 channel in the AI channel list!";
        return AGAIN;
    }
    
    const unsigned srate = dialog->srateSB->value();
    if (srate > DAQ::MaximumSampleRate(dev, chanVect.size())) {
        errTitle = "Sampling Rate Invalid", errMsg = QString().sprintf("The sample rate specified (%d) is too high for the number of channels (%d)!", (int)srate, (int)chanVect.size());
        return AGAIN;
    }
    
    const DAQ::AcqStartEndMode acqStartEndMode = (DAQ::AcqStartEndMode)dialog->acqStartEndCB->currentIndex();
    
    int pdChan = acqPdParams->pdAISB->value();
    bool havePD = false;
    if ( (havePD=(acqStartEndMode == DAQ::PDStartEnd || acqStartEndMode == DAQ::PDStart))
        && (chanVect.contains(pdChan) || pdChan < 0 || pdChan >= aiChanLists[dev].count())
        ) {
        errTitle = "PDChannel Invalid", errMsg = QString().sprintf("Specified photodiode channel (%d) is invalid or clashes with another AI channel!", pdChan);
        return AGAIN;
    }
    
    unsigned nVAI = chanVect.size();
    
    if (acqMode == DAQ::AI60Demux || acqMode == DAQ::AI120Demux) 
        nVAI = (nVAI-nExtraChans) * MUX_CHANS_PER_PHYS_CHAN + nExtraChans;
    else if (acqMode == DAQ::JFRCIntan32)
        nVAI = (nVAI-nExtraChans) * MUX_CHANS_PER_PHYS_CHAN32 + nExtraChans;
    
    bool usePD = false;
    if (acqStartEndMode == DAQ::PDStart || acqStartEndMode == DAQ::PDStartEnd) {
        chanVect.push_back(pdChan);
        ++nVAI;
        ++nExtraChans;
        usePD = true;
    }
    
    QMap<unsigned, unsigned> aopass;            
    if (aoPassthru->aoPassthruGB->isChecked() && aoDevNames.count()) {
        QString le = aoPassthru->aoPassthruLE->text();
        if (havePD) le=le.replace("PDCHAN", QString::number(nVAI-1)); // PD channel index is always the last index
        aopass = parseAOPassthruString(le, &err);
        if (err) {
            errTitle =  "AO Passthru Error", errMsg = "Error parsing AO Passthru list, specify a string of the form UNIQUE_AOCHAN=CHANNEL_INDEX_POS!";
            return AGAIN;
        }
    }
    
    QVector<unsigned> aoChanVect = aopass.uniqueKeys().toVector();
    
    int pdAOChan = acqPdParams->pdPassthruAOSB->value();
    
    if (!usePD || !acqPdParams->pdPassthruAOChk->isChecked()) {
        pdAOChan = -1;
    } else {
        if (!aoChanVect.contains(pdAOChan)) {
            errTitle = "AO Passthru Error", errMsg = QString().sprintf("INTERNAL ERROR: The AO channel specified for the PD (%d) does not exist in the passthru string!", (int)(pdAOChan));
            return AGAIN;
        }
    }
    
    // validate AO channels in AO passthru map
    if (aoChanVect.size()) {
        QStringList aol = aoChanLists[aoDev];
        for (int i = 0; i < (int)aoChanVect.size(); ++i) {
            if ((int)aoChanVect[i] >= (int)aol.size()) {
                errTitle = "AO Passthru Error", errMsg = QString().sprintf("The AO channel specified in the passthru string (%d) is illegal/invalid!", (int)(aoChanVect[i]));
                return AGAIN;
            }
        }
    }
    
    {
        // validate AI channels in AO passthru map
        QList<unsigned> vl = aopass.values();
        for (QList<unsigned>::const_iterator it = vl.begin(); it != vl.end(); ++it)
            if (*it >= nVAI) {
                errTitle = "AO Passthru Invalid", errMsg = QString().sprintf("Chan index %d specified as an AO passthru source but it is > the number of virtual AI channels (%d)!", (int)*it, (int)nVAI);
                return AGAIN;
            }
    }
    
    
    if (dialog->stimGLReopenCB->isChecked() &&
        (acqStartEndMode == DAQ::StimGLStartEnd
         || acqStartEndMode == DAQ::PDStartEnd) ) {
            errTitle = "Incompatible Configuration", errMsg = QString().sprintf("'Re-Open New Save File on StimGL Experiment' not compatible with 'StimGL Plugin Start & End' or 'PD Start & End' acquisition trigger modes!");
            return AGAIN;
        }
    
    DAQ::Params & p (acceptedParams);
    p.outputFile = p.outputFileOrig = dialog->outputFileLE->text().trimmed();
    p.dev = dev;
    p.stimGlTrigResave = dialog->stimGLReopenCB->isChecked();
    QStringList rngs = dialog->aiRangeCB->currentText().split(" - ");
    if (rngs.count() != 2) {
        errTitle = "AI Range ComboBox invalid!";
        errMsg = "INTERNAL ERROR: AI Range ComboBox needs numbers of the form REAL - REAL!";
        Error() << errMsg;
        return ABORT;
    }
    p.range.min = rngs.first().toDouble();
    p.range.max = rngs.last().toDouble();
    p.mode = acqMode;
    p.srate = srate;
    p.extClock = dialog->clockCB->currentIndex() == 0;
    p.aiChannels = chanVect;
    p.nVAIChans = nVAI;
    p.nExtraChans = nExtraChans;
    p.aiString = chans;
    p.doCtlChan = dialog->doCtlCB->currentIndex();
    p.doCtlChanString = QString("%1/%2").arg(p.dev).arg(dialog->doCtlCB->currentText());
    p.usePD = usePD;
    if (!aoDevNames.empty()) {                
        p.aoPassthru = aoPassthru->aoPassthruGB->isChecked();
        p.aoDev = aoDev;
        rngs = aoPassthru->aoRangeCB->currentText().split(" - ");
        if (rngs.count() != 2) {
            errTitle = "AO Range ComboBox invalid!";
            errMsg = "INTERNAL ERROR: AO Range ComboBox needs numbers of the form REAL - REAL!";
            Error() << errMsg;
            return ABORT;
        } 
        p.aoRange.min = rngs.first().toDouble();
        p.aoRange.max = rngs.last().toDouble();
        p.aoPassthruMap = aopass;
        p.aoChannels = aoChanVect;
        p.aoPassthruString = aoPassthru->aoPassthruLE->text();
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
    p.idxOfPdChan = p.nVAIChans-1 /* always the last index */;
    p.pdThresh = static_cast<signed short>((acqPdParams->pdAIThreshSB->value()-p.range.min)/(p.range.max-p.range.min) * 65535. - 32768.);
    p.pdPassThruToAO = pdAOChan;
    p.pdStopTime = acqPdParams->pdStopTimeSB->value();
    
    p.aiTerm = DAQ::StringToTermConfig(dialog->aiTerminationCB->currentText());
    p.fastSettleTimeMS = dialog->fastSettleSB->value();
    p.auxGain = dialog->auxGainSB->value();
    p.chanMap = chanMapCtl.mappingForAll();
    
    p.silenceBeforePD = acqPdParams->pdPre->value()/1000.;
    
    QVector<unsigned> subsetChans;
    if (p.mode != DAQ::AIRegular) {
        QString subsetString = dialog->channelSubsetLE->text();
        subsetString.replace("ALL", "", Qt::CaseInsensitive);
        subsetString.replace("*", "", Qt::CaseInsensitive);        
        subsetString = parseAIChanString(subsetString, subsetChans, &err, true);
        if (err) {
            errTitle = "Channel subset error.";
            errMsg = "The channel subset is incorrectly defined.";
            return AGAIN;
        }
    }
    p.demuxedBitMap.resize(p.nVAIChans - p.nExtraChans);
    if (!subsetChans.count()) p.demuxedBitMap.fill(true);
    else {
        p.demuxedBitMap.fill(false);
        for (QVector<unsigned>::iterator it = subsetChans.begin(); it != subsetChans.end(); ++it) {
            const unsigned n = *it;
            if (int(n) >= p.demuxedBitMap.size()) {
                errTitle = "Channel subset error.";
                errMsg = "The channel subset is incorrectly defined as it contains out-of-range channel id(s).";
                return AGAIN;
            }
            p.demuxedBitMap.setBit(n);
        }
    }
    p.subsetString = dialog->channelSubsetLE->text();  
    p.nVAIChansFromDAQ = p.nVAIChans;
    p.nVAIChans = p.demuxedBitMap.count(true) + p.nExtraChans;
    QString debugStr = "";
    for (int i = 0; i < p.demuxedBitMap.count(); ++i) {
        debugStr += QString::number(int(p.demuxedBitMap.at(i))) + " ";
        //if (i && !(i % 9)) debugStr += "\n";
    }
    Debug() << "Channel subset bitmap: " << debugStr;
    
    saveSettings();       
    
    return OK;
}

int ConfigureDialogController::exec()
{

	if (!aiDevRanges.count()) {
       QMessageBox::critical(dialogW, "NIDAQ Setup Error", "Something's wrong with your NI-DAQ hardware setup.  Make sure to delete unused devices in the NIDAQ Explorer and try again!");
	   return QDialog::Rejected;
	}

    bool again;
    int ret;

    if ( aoPassthru->aoPassthruGB->parent() != dialog->aoPassthruWidget )
        aoPassthru->aoPassthruGB->setParent(dialog->aoPassthruWidget);

    resetFromParams();

    do {

        // make sure these are gone..
        delete aoPassW, aoPassW = 0;
        delete applyDlg, applyDlg = 0;

        ret = ((QDialog *)dialogW)->exec();
        again = false;

   
        if (ret == QDialog::Accepted) {
            QString errTitle, errMsg;
            ValidationResult result = validateForm(errTitle, errMsg);
            
            switch ( result ) {
                case AGAIN: 
                    QMessageBox::critical(dialogW, errTitle, errMsg);
                    again = true; 
                    break;
                case ABORT: 
                    again = false; 
                    return QDialog::Rejected;
                case OK: 
                    again = false; 
                    break;
            }
        }
    } while (again);

    return ret;
}

/*static*/ 
QString
ConfigureDialogController::parseAIChanString(const QString & str, 
                                             QVector<unsigned> & aiChannels,
                                             bool *parse_error,
                                             bool emptyOk) 
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
    if (!emptyOk && !ret.length() && parse_error) *parse_error = true;
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

/*static*/ 
void ConfigureDialogController::paramsFromSettingsObject(DAQ::Params & p, const QSettings & settings)
{
    p.outputFile = settings.value("outputFile", "data.bin").toString();
    p.dev = settings.value("dev", "").toString();
    p.stimGlTrigResave = settings.value("stimGlTrigResave", false).toBool();
    p.doCtlChan = settings.value("doCtlChan", "0").toUInt();
    p.range.min = settings.value("rangeMin", -2.5).toDouble();
    p.range.max = settings.value("rangeMax", 2.5).toDouble();
    p.mode = (DAQ::Mode)settings.value("acqMode", 0).toInt();
    p.srate = settings.value("srate", INTAN_SRATE).toUInt();
    p.extClock = settings.value("extClock", true).toBool();
    p.aiString = settings.value("aiString", "0:3").toString();
    p.subsetString = settings.value("subsetString", "ALL").toString();
    p.aoPassthru = settings.value("aoPassthru", false).toBool();
    p.aoPassthruString = settings.value("aoPassthruString", "0=1,1=2").toString();
    p.aoRange.min = settings.value("aoRangeMin", -2.5).toDouble();
    p.aoRange.max = settings.value("aoRangeMax", 2.5).toDouble();
    
    p.aoDev = settings.value("aoDev", "").toString();
    p.suppressGraphs = settings.value("suppressGraphs", false).toBool();
    
    p.acqStartEndMode =  (DAQ::AcqStartEndMode)settings.value("acqStartEndMode", 0).toInt();
    p.isImmediate = settings.value("acqStartTimedImmed", false).toBool();
    p.isIndefinite = settings.value("acqStartTimedIndef", false).toBool();
    p.startIn = settings.value("acqStartTimedTime", 0.0).toDouble();
    p.duration = settings.value("acqStartTimedDuration", 60.0).toDouble();
    
    p.pdThresh = settings.value("acqPDThresh", 48000-32768).toInt();
    p.pdChan = settings.value("acqPDChan", 4).toInt();
    p.pdStopTime = settings.value("acqPDOffStopTime", .5).toDouble();
    p.pdPassThruToAO = settings.value("acqPDPassthruChanAO", 2).toInt();
    
    p.aiTerm = (DAQ::TermConfig)settings.value("aiTermConfig", (int)DAQ::Default).toInt();
    p.fastSettleTimeMS = settings.value("fastSettleTimeMS", DEFAULT_FAST_SETTLE_TIME_MS).toUInt();
    p.auxGain = settings.value("auxGain", 200.0).toDouble();    
    
    p.silenceBeforePD = settings.value("silenceBeforePD", DEFAULT_PD_SILENCE).toDouble();
    
}

void ConfigureDialogController::loadSettings()
{
    DAQ::Params & p(acceptedParams);

    QSettings settings(SETTINGS_DOMAIN, SETTINGS_APP);

    settings.beginGroup(SETTINGS_GROUP);

    paramsFromSettingsObject(p, settings);
    
    settings.endGroup();

}

void ConfigureDialogController::saveSettings() const
{
    const DAQ::Params & p(acceptedParams);

    QSettings settings(SETTINGS_DOMAIN, SETTINGS_APP);

    settings.beginGroup(SETTINGS_GROUP);
    
    settings.setValue("outputFile", QFileInfo(p.outputFile).fileName());
    QString path = QFileInfo(p.outputFile).path();
    //mainApp()->setOutputDirectory(path.length() ? path : QString(PATH_SEPARATOR));
    settings.setValue("dev", p.dev);
    settings.setValue("stimGlTrigResave", p.stimGlTrigResave);

    settings.setValue("rangeMin", p.range.min);
    settings.setValue("rangeMax", p.range.max);
    settings.setValue("acqMode", (int)p.mode);
    settings.setValue("doCtlChan", p.doCtlChan);
    settings.setValue("srate", p.srate);
    settings.setValue("extClock", p.extClock);
    settings.setValue("aiString", p.aiString);
    settings.setValue("subsetString", p.subsetString);
    settings.setValue("aoDev", p.aoDev);
    settings.setValue("aoPassthru", p.aoPassthru);
    settings.setValue("aoRangeMin", p.aoRange.min);
    settings.setValue("aoRangeMax", p.aoRange.max);
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
    settings.setValue("acqPDOffStopTime", p.pdStopTime);
    settings.setValue("aiTermConfig", (int)p.aiTerm);
    settings.setValue("fastSettleTimeMS", p.fastSettleTimeMS);
    settings.setValue("auxGain", p.auxGain);


    settings.setValue("silenceBeforePD", p.silenceBeforePD);

    settings.endGroup();
}

QString ConfigureDialogController::acqParamsToString() 
{    
    loadSettings(); // so that we have fresh settings CREATED (otherwise on first run this would be empty!!)
    saveSettings();
    
    QString out;
    QTextStream ts(&out, QIODevice::WriteOnly);
    
    QSettings settings(SETTINGS_DOMAIN, SETTINGS_APP);
    
    settings.beginGroup(SETTINGS_GROUP);    
    
    QStringList keys (settings.childKeys());
    for (QStringList::const_iterator it = keys.begin(); it != keys.end(); ++it) {
        ts << *it << " = " << settings.value(*it).toString() << "\n";
        //Debug() << "sending: " << *it << " = " << settings.value(*it).toString();
    }
    ts.flush();
    return out;
}

QString ConfigureDialogController::acqParamsFromString(const QString & str)
{
    QString err (QString::null);
    
    QSettings settings(SETTINGS_DOMAIN, SETTINGS_APP);
    settings.beginGroup("TempAcqParamsFromMatlab");
    QTextStream ts(str.toUtf8());
    QString line;
    
    while (!(line = ts.readLine(65536)).isNull()) {
        int eq = line.indexOf("=");
        if (eq > -1 && eq < line.length()) {
            QString n = line.left(eq).trimmed(), v = line.mid(eq+1).trimmed();
            settings.setValue(n, v);
            //Debug() << "Settings set value: " << n << "=" << v;
        }
    }
    
    DAQ::Params p;
    paramsFromSettingsObject(p, settings);
    
    resetFromParams(&p); // resets the dialog box form from the params
    
    QString errTitle, errMsg;
    ValidationResult result = validateForm(errTitle, errMsg);
    
    if (result != OK) err = errTitle + " " + errMsg.replace("\n", " ");
    
    return err;
}

void ConfigureDialogController::showAOPassThruDlg()
{
    if (!aoPassW) {
        aoPassW = new QWidget(0);
        applyDlg = new Ui::ApplyDialog;
        applyDlg->setupUi(aoPassW);
        Connect(applyDlg->applyButton, SIGNAL(clicked()), this, SLOT(applyAOPass()));
    }
    aoPassW->setWindowTitle("Re-Specify AO Passthru");
    QRect r = mainApp()->console()->frameGeometry();
    aoPassW->move(r.center().x()-aoPassW->width()/2, r.center().y()-aoPassW->height()/2);    
    if (aoPassthru->aoPassthruGB->parent() != applyDlg->widget)
        aoPassthru->aoPassthruGB->setParent(applyDlg->widget);
    resetAOPassFromParams(aoPassthru);
    aoPassW->show();
}

void ConfigureDialogController::applyAOPass()
{
    DAQ::Params & p (acceptedParams);
    if (/*!aoPassthru->aoPassthruGB->isChecked()*/!p.aoPassthru) {
        QMessageBox::information(0, "AO Passthru Disabled", "AO Passthru spec cannot be modified as it was disabled at acquisition start.");
        resetAOPassFromParams(aoPassthru);
        return;
    }
    aoPassthru->aoPassthruGB->setCheckable(false);
    bool hadPdChan = aoPassthru->aoPassthruLE->text().contains("PDCHAN");
    bool err;

    aoDeviceCBChanged();

    Ui::AoPassThru & aop(*aoPassthru);

    if ((!aop.aoPassthruLE->text().contains("PDCHAN") && hadPdChan)) {
        QMessageBox::critical(0, "PDCHAN missing", "PD Passthru was enabled at startup, but the PDCHAN is missing from the chan list!");
        resetAOPassFromParams(aoPassthru);
        return;            
    }
    if ((aop.aoPassthruLE->text().contains("PDCHAN") && !hadPdChan) ) {
        QMessageBox::critical(0, "PD not enabled", "PDCHAN is in the chanlist, but PD was not enabled at ACQ startup!");
        resetAOPassFromParams(aoPassthru);
        return;            
    }
    const QString aoDev = aop.aoDeviceCB->count() ? aoDevNames[aop.aoDeviceCB->currentIndex()] : "";
    QMap<unsigned, unsigned> aopass;            
    const QVector<unsigned> & chanVect = p.aiChannels;            
    int nExtraChans = 0;
    bool mux = false;
    if ( (mux = (p.mode == DAQ::AI60Demux || p.mode == DAQ::AI120Demux )) ) {
        const int minChanSize = p.mode == DAQ::AI120Demux ? 8 : 4;
        nExtraChans = chanVect.size() - minChanSize;
    }
        
    const unsigned nVAI = (chanVect.size()-nExtraChans) * (mux ? NUM_CHANS_PER_INTAN : 1) + nExtraChans;

    if (aoDevNames.count()) {
        QString le = aop.aoPassthruLE->text();
        if (p.usePD) le=le.replace("PDCHAN", /*QString::number(pdChan)*/QString::number(nVAI-1)); // PD channel index is always the last index
        aopass = parseAOPassthruString(le, &err);
        if (err) {
            QMessageBox::critical(0, "AO Passthru Error", "Error parsing AO Passthru list, specify a string of the form UNIQUE_AOCHAN=CHANNEL_INDEX_POS!");
            resetAOPassFromParams(aoPassthru);
            return;            
        }
    }
    QVector<unsigned> aoChanVect = aopass.uniqueKeys().toVector();
    if (aoChanVect.size()) {
        QStringList aol = aoChanLists[aoDev];
        bool again = false;
        for (int i = 0; i < (int)aoChanVect.size(); ++i) {
            if ((int)aoChanVect[i] >= (int)aol.size()) {
                QMessageBox::critical(0, "AO Passthru Error", QString().sprintf("The AO channel specified in the passthru string (%d) is illegal/invalid!", (int)(aoChanVect[i])));
                again = true;
                break;
            }
        }
        if (again) {
            resetAOPassFromParams(aoPassthru);
            return;            
        }
    } else {
        QMessageBox::critical(0, "AO Passthru Error", QString().sprintf("Need at least 1 AO=CHAN_INDEX spec in passthru string since passthru was enabled at startup!"));
        resetAOPassFromParams(aoPassthru);
        return;            
    }
    {
        // validate AI channels in AO passthru map
        QList<unsigned> vl = aopass.values();
        bool again = false;
        for (QList<unsigned>::const_iterator it = vl.begin(); it != vl.end(); ++it)
            if (*it >= nVAI) {
                QMessageBox::critical(dialogW, "AO Passthru Invalid", QString().sprintf("Chan index %d specified as an AO passthru source but it is > the number of virtual AI channels (%d)!", (int)*it, (int)nVAI));
                again = true;
                break;
            }
        if (again) {
            resetAOPassFromParams(aoPassthru);
            return;            
        }
    }
        
    QStringList rngs = aop.aoRangeCB->currentText().split(" - ");
    if (rngs.count() != 2) {
        Error() << "INTERNAL ERROR: AO Range ComboBox needs numbers of the form REAL - REAL!";
        resetAOPassFromParams(aoPassthru);
        return;                    
    }            
    p.lock();
    p.aoDev = aoDev;
    p.aoRange.min = rngs.first().toDouble();
    p.aoRange.max = rngs.last().toDouble();
    p.aoPassthruMap = aopass;
    p.aoChannels = aoChanVect;
    p.aoPassthruString = aop.aoPassthruLE->text();
    p.unlock();
    saveSettings();
    Log() << "Applied new AO Passthru settings";
}
