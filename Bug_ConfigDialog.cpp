/*
 *  Bug_ConfigDialog.cpp
 *  SpikeGL
 *
 *  Created by calin on 1/29/15.
 *  Copyright 2015 Calin Culianu <calin.culianu@gmail.com>. All rights reserved.
 *
 */

#include <QDialog>
#include <QMessageBox>
#include <QFileDialog>
#include "Bug_ConfigDialog.h"
#include "Util.h"
#include "MainApp.h"
#include "ConfigureDialogController.h"

Bug_ConfigDialog::Bug_ConfigDialog(DAQ::Params & p, QObject *parent) : QObject(parent), acceptedParams(p)
{
    dialogW = new QDialog(0);
    dialogW->setAttribute(Qt::WA_DeleteOnClose, false);
	dialog = new Ui::Bug_ConfigDialog;
    dialog->setupUi(dialogW);    
    aoPassThru = new Ui::AoPassThru;
    aoPassThru->setupUi(dialog->aoPassthruWidget);
    aoPassThru->aoPassthruLabel->hide();
    aoPassThru->aoPassthruLE->hide();
    aoPassThru->aoNoteLink->hide();
    aoPassThru->aoPassthruGB->setGeometry(aoPassThru->aoPassthruGB->x(),aoPassThru->aoPassthruGB->y(),dialog->aoPassthruWidget->width(),120);
    dialog->aoPassthruWidget->setGeometry(dialog->aoPassthruWidget->x(),dialog->aoPassthruWidget->y(),dialog->aoPassthruWidget->width(),120);
    aoPassThru->aoPassthruGB->setToolTip("If enabled, then one of the Bug3 incoming channels can be sent to an NI-DAQ AO device in real-time.");
    Connect(aoPassThru->bufferSizeSlider, SIGNAL(valueChanged(int)), this, SLOT(aoBufferSizeSliderChanged()));
    Connect(aoPassThru->aoDeviceCB, SIGNAL(activated(const QString &)), this, SLOT(aoDeviceCBChanged()));
    Connect(dialog->ttlTrigCB, SIGNAL(currentIndexChanged(int)), this, SLOT(ttlTrigCBChanged()));
	Connect(dialog->browseBut, SIGNAL(clicked()), this, SLOT(browseButClicked()));
    Connect(dialog->ttlAltChk, SIGNAL(toggled(bool)), this, SLOT(ttlAltClicked(bool)));
	for (int i = 0; i < DAQ::BugTask::TotalTTLChans; ++i) {
		ttls[i] = new QCheckBox(QString::number(i), dialog->ttlW);
		dialog->ttlLayout->addWidget(ttls[i]);
	}
}

Bug_ConfigDialog::~Bug_ConfigDialog()
{
    for (int i = 0; i < DAQ::BugTask::TotalTTLChans; ++i) delete ttls[i], ttls[i] = 0;
    delete dialogW; dialogW = 0;
    delete dialog; dialog = 0;
}

void Bug_ConfigDialog::browseButClicked()
{
    QString fn = QFileDialog::getSaveFileName(dialogW, "Select output file", dialog->outputFileLE->text());
    if (fn.length()) {
		QFileInfo fi(fn);
		QString suff = fi.suffix();
		if (!suff.startsWith(".")) suff = QString(".") + suff;
		if (suff.toLower() != ".bin") fn += ".bin";
		dialog->outputFileLE->setText(fn);
	}
}

int Bug_ConfigDialog::exec() 
{
	mainApp()->configureDialogController()->loadSettings(); // this makes the shared params object get updated form the settings

	guiFromSettings();

	ValidationResult vr;
	int r;
	
	do {
		vr = ABORT;
		r = dialogW->exec();
		QString errTit, errMsg;
		if (r == QDialog::Accepted) {
			vr = validateForm(errTit, errMsg);
			if (vr == OK) {
				DAQ::Params & p(acceptedParams);
				p.fg.reset();
				p.bug.reset();
				p.bug.enabled = true;
				p.bug.rate = dialog->acqRateCB->currentIndex();
                if (dialog->ttlTrigCB->currentIndex() > 0) {
					p.bug.ttlTrig = dialog->ttlTrigCB->currentIndex()-1;
                    p.bug.whichTTLs |= (0x1 << p.bug.ttlTrig);
				}
                p.bug.altTTL = dialog->ttlAltChk->isChecked();
				for (int i = 0; i < DAQ::BugTask::TotalTTLChans; ++i) {
					if (ttls[i]->isChecked()) p.bug.whichTTLs |= 0x1 << i;  
				}
				int ttlIdx = DAQ::BugTask::BaseNChans;
				for (int i = 0; i < DAQ::BugTask::TotalTTLChans; ++i) {
					if (i == p.bug.ttlTrig) break;
					if (p.bug.whichTTLs & (0x1<<i)) ++ttlIdx;
				}
				p.bug.clockEdge = dialog->clkEdgeCB->currentIndex();
				p.bug.hpf = dialog->hpfChk->isChecked() ? dialog->hpfSB->value(): 0;
				p.bug.snf = dialog->notchFilterChk->isChecked();
				p.bug.errTol = dialog->errTolSB->value();
				p.suppressGraphs = dialog->disableGraphsChk->isChecked();
				p.resumeGraphSettings = dialog->resumeGraphSettingsChk->isChecked();
				
				int nttls = 0;
				for (int i = 0; i < DAQ::BugTask::TotalTTLChans; ++i)
					if ( (p.bug.whichTTLs >> i) & 0x1) ++nttls; // count number of ttls set 
				p.nVAIChans = DAQ::BugTask::BaseNChans + nttls;
				p.nVAIChans1 = p.nVAIChans;
				p.nVAIChans2 = 0;
				p.aiChannels2.clear();
                //p.aiString2.clear();
				p.aiChannels.resize(p.nVAIChans);
				p.subsetString = dialog->channelSubsetLE->text();
				p.demuxedBitMap.resize(p.nVAIChans); p.demuxedBitMap.fill(true);
                for (int i = 0; i < (int)p.nVAIChans; ++i) {
					p.aiChannels[i] = i;
				}
				if (p.subsetString.compare("ALL", Qt::CaseInsensitive) != 0) {
					QVector<unsigned> subsetChans;
					bool err;
					p.subsetString = ConfigureDialogController::parseAIChanString(p.subsetString, subsetChans, &err, true);
					if (!err) {
						p.demuxedBitMap.fill(false);
						for (int i = 0; i < subsetChans.size(); ++i) {
							int bit = subsetChans[i];
							if (bit < p.demuxedBitMap.size()) p.demuxedBitMap[bit] = true;
						}
						if (p.demuxedBitMap.count(true) == 0) {
							Warning() << "Bug3 channel subset string specified invalid. Proceeding with 'ALL' channels set to save!";
							p.demuxedBitMap.fill(true);
							p.subsetString = "ALL";
						}
					}
				}
				
				p.overrideGraphsPerTab = dialog->graphsPerTabCB->currentText().toUInt();

				p.isIndefinite = true;
				p.isImmediate = true;
				p.acqStartEndMode = DAQ::Immediate;
				p.usePD = 0;
				p.chanMap = ChanMap();
				if (p.bug.ttlTrig > -1) {
					p.acqStartEndMode = DAQ::Bug3TTLTriggered;
					p.isImmediate = false;
					p.idxOfPdChan = ttlIdx;
					p.usePD = true;
					p.pdChanIsVirtual = true;
					p.pdChan = ttlIdx;
                    p.pdThresh = 10000; // in sample vals.. high TTL line is always above 0 in sample vals.. so 10000 is safe
                    p.pdThreshW = p.bug.altTTL ? static_cast<unsigned>(dialog->trigWSB_2->value()) : static_cast<unsigned>(dialog->trigWSB->value());
					p.pdPassThruToAO = -1;
                    p.pdStopTime = p.bug.altTTL ? dialog->trigPost->value()/1000.0 : dialog->trigStopTimeSB->value();
                    p.silenceBeforePD = p.bug.altTTL ? dialog->trigPre_2->value()/1000.0 : dialog->trigPre->value()/1000.0;
				}

				if (AGAIN == ConfigureDialogController::setFilenameTakingIntoAccountIncrementHack(p, p.acqStartEndMode, dialog->outputFileLE->text(), dialogW)) {
					vr = AGAIN;
					continue;
				}
				
                if ( (p.aoPassthru = (aoPassThru->aoDeviceCB->count() && aoPassThru->aoPassthruGB->isChecked())) ) {
                    p.aoDev = mainApp()->configureDialogController()->getAODevName(aoPassThru);
                    p.bug.aoSrate = aoPassThru->srateSB->value();
                    p.aoClock = aoPassThru->aoClockCB->currentText();
                    QStringList rngs = aoPassThru->aoRangeCB->currentText().split(" - ");
                    if (rngs.count() != 2) {
                        errTit = "AO Range ComboBox invalid!";
                        errMsg = "INTERNAL ERROR: AO Range ComboBox needs numbers of the form REAL - REAL!";
                        Error() << errMsg;
                        return ABORT;
                    }
                    p.aoRange.min = rngs.first().toDouble();
                    p.aoRange.max = rngs.last().toDouble();
                    p.aoBufferSizeCS = aoPassThru->bufferSizeSlider->value();
                    if (p.aoBufferSizeCS > 100) p.aoBufferSizeCS = 100;
                    else if (p.aoBufferSizeCS <= 0) p.aoBufferSizeCS = 1;
                    bool errflg;
                    QMap<unsigned, unsigned> m = ConfigureDialogController::parseAOPassthruString(p.bug.aoPassthruString, &errflg);
                    unsigned k = 0, v = 0;
                    if (!errflg && !m.empty()) {
                        k = m.begin().key();
                        v = m.begin().value();
                    }
                    if (k >= DAQ::GetNAOChans(p.aoDev) || v >= p.nVAIChans) k = 0, v = 0;
                    p.aoChannels = QVector<unsigned>(1,k);
                    p.aoPassthruMap.clear();
                    p.aoPassthruMap.insert(k,v);
                    p.aoPassthruString = p.bug.aoPassthruString = QString("%1=%2").arg(k).arg(v);
                }

				saveSettings();

                // this stuff doesn't need to be saved since it's constant and will mess up regular acq "remembered" values
                p.aoSrate = p.bug.aoSrate; // fudged.. needed because AOWriteThread::run looks at this variable but we don't want to save it above in saveSettings()...
				p.dev = "USB_Bug3";
				p.nExtraChans1 = 0;
				p.nExtraChans2 = 0;
				
				p.extClock = true;
				p.mode = DAQ::AIRegular;
				p.dualDevMode = false;
				p.stimGlTrigResave = false;
				p.srate = DAQ::BugTask::SamplingRate;
				p.aiTerm = DAQ::Default;
				p.aiString = QString("0:%1").arg(p.nVAIChans-1);
				p.customRanges.resize(p.nVAIChans);
				p.chanDisplayNames.resize(p.nVAIChans);
				DAQ::Range rminmax(1e9,-1e9);
				for (unsigned i = 0; i < p.nVAIChans; ++i) {
					DAQ::Range r;
                    int chan_id_for_display = i;
					if (i < unsigned(DAQ::BugTask::TotalNeuralChans)) { // NEU
						r.min = -(2.3*2048/2.0)/1e6 /*-2.4/1e3*/, r.max = 2.3*2048/2.0/1e6/*2.4/1e3*/;
					} else if (i < (unsigned)DAQ::BugTask::TotalNeuralChans+DAQ::BugTask::TotalEMGChans) { //EMG
						r.min = -(23.0*2048)/2.0/1e6 /*-24.0/1e3*/, r.max = (23.0*2048)/2.0/1e6/*24.0/1e3*/;
					} else if (i < (unsigned)DAQ::BugTask::TotalNeuralChans+DAQ::BugTask::TotalEMGChans+DAQ::BugTask::TotalAuxChans) {
						//r.min = 1.2495+.62475, r.max = 1.2495*2;				
						//r.min = -(2.4*2048)/2./1e3, r.max = (2.4*2048)/2./1e3;
						r.min = DAQ::BugTask::ADCStep*1024., r.max = DAQ::BugTask::ADCStep*1024.+DAQ::BugTask::ADCStep*2048.;
					} else { // ttl lines
						r.min = -5., r.max = 5.;
                        // since ttl lines may be missing in channel set, renumber the ones that are missing for display purposes
                        int lim = (int(i)-int(DAQ::BugTask::TotalNeuralChans+DAQ::BugTask::TotalEMGChans+DAQ::BugTask::TotalAuxChans))+1;
                        for (int j=0; j < lim && j < DAQ::BugTask::TotalTTLChans ; ++j)
                            if (!(p.bug.whichTTLs & (0x1<<j))) ++chan_id_for_display, ++lim;

					}
					if (rminmax.min > r.min) rminmax.min = r.min;
					if (rminmax.max < r.max) rminmax.max = r.max;
					p.customRanges[i] = r;
                    p.chanDisplayNames[i] = DAQ::BugTask::getChannelName(chan_id_for_display);
				}
				p.range = rminmax;
				p.auxGain = 1.0;
				
			} else if (vr==AGAIN) {
				if (errTit.length() && errMsg.length())
					QMessageBox::critical(dialogW, errTit, errMsg);
			} else if (vr==ABORT) r = QDialog::Rejected;
		}
	} while (vr==AGAIN && r==QDialog::Accepted);	
	return r;
}

void Bug_ConfigDialog::guiFromSettings()
{
	DAQ::Params & p(acceptedParams);
	dialog->outputFileLE->setText(p.outputFile);
	dialog->channelSubsetLE->setText(p.subsetString);
	for (int i = 0; i < DAQ::BugTask::TotalTTLChans; ++i) {
		ttls[i]->setChecked(p.bug.whichTTLs & (0x1<<i));
	}
	dialog->ttlTrigCB->setCurrentIndex(p.bug.ttlTrig+1);
	dialog->clkEdgeCB->setCurrentIndex(p.bug.clockEdge);
	if (p.bug.hpf > 0) {
		dialog->hpfSB->setValue(p.bug.hpf);
		dialog->hpfChk->setChecked(true);
	} else {
		dialog->hpfChk->setChecked(false);
	}
	dialog->notchFilterChk->setChecked(p.bug.snf);
	dialog->errTolSB->setValue(p.bug.errTol);
	for (int i = 1; i < dialog->graphsPerTabCB->count(); ++i) {
        if (dialog->graphsPerTabCB->itemText(i).toUInt() == (unsigned)p.overrideGraphsPerTab) {
            dialog->graphsPerTabCB->setCurrentIndex(i);
            break;
        }
    }	
	
	dialog->trigWSB->setValue(p.pdThreshW);
	dialog->trigStopTimeSB->setValue(p.pdStopTime);
	dialog->trigPre->setValue(p.silenceBeforePD*1000.);
	dialog->trigParams->setEnabled(p.bug.ttlTrig >= 0);

    mainApp()->configureDialogController()->probeDAQHardware();
    mainApp()->configureDialogController()->resetAOPassFromParams(aoPassThru, &p, &p.bug.aoSrate);
    aoPassThru->aoPassthruGB->setCheckable(true);
    aoPassThru->aoPassthruGB->setChecked(p.aoPassthru && aoPassThru->aoDeviceCB->count());
    if (!aoPassThru->aoDeviceCB->count()) aoPassThru->aoPassthruGB->setEnabled(false);
    // alt ttl stuff
    dialog->ttlAltChk->setChecked(p.bug.altTTL);
    dialog->trigPre_2->setValue(p.silenceBeforePD*1000.0);
    dialog->trigPost->setValue(p.pdStopTime*1000.0);
    dialog->trigWSB_2->setValue(p.pdThreshW);
    ttlAltClicked(p.bug.altTTL);

    //polish
    aoDeviceCBChanged();
}

void Bug_ConfigDialog::saveSettings()
{
	mainApp()->configureDialogController()->saveSettings();
}

Bug_ConfigDialog::ValidationResult Bug_ConfigDialog::validateForm(QString & errTitle, QString & errMsg, bool isGUI)
{
	errTitle = ""; errMsg = ""; (void)isGUI;
/*	if (dialog->ttlTrigCB->currentIndex() && dialog->ttlTrigCB->currentIndex() == dialog->ttl2CB->currentIndex()) {
		errTitle = "Duplicate TTL Channel";
		errMsg = QString("The same TTL channel (%1) was specified twice in both TTL combo boxes. Try again.").arg(dialog->ttl2CB->currentIndex()-1);
		return AGAIN;					
	}*/
	QVector<unsigned> subsetChans;
    QString subsetString = dialog->channelSubsetLE->text().trimmed();	
	if (!subsetString.size()) subsetString = "ALL";
	const bool hasAllSubset =  !subsetString.compare("ALL", Qt::CaseInsensitive) || subsetString == "*";
	bool err = false;
	subsetString = ConfigureDialogController::parseAIChanString(subsetString, subsetChans, &err, true);
	if (err && !hasAllSubset) {
		errTitle = "Channel subset error.";
		errMsg = "The channel subset is incorrectly defined.";
		return AGAIN;
	} else if (hasAllSubset) {
		dialog->channelSubsetLE->setText("ALL");
	}
	return OK;
}

void Bug_ConfigDialog::ttlTrigCBChanged()
{
	for (int i = 0; i < DAQ::BugTask::TotalTTLChans; ++i)
		ttls[i]->setEnabled(true);
	if (dialog->ttlTrigCB->currentIndex() > 0) {
		ttls[dialog->ttlTrigCB->currentIndex()-1]->setEnabled(false);
	}
    const bool isLineSelected = dialog->ttlTrigCB->currentIndex() != 0;
    dialog->trigParams->setEnabled(isLineSelected);
    dialog->altTrigParams->setEnabled(isLineSelected);
    dialog->ttlAltChk->setEnabled(isLineSelected);
}

void Bug_ConfigDialog::aoBufferSizeSliderChanged()
{
    ConfigureDialogController::updateAOBufferSizeLabel(aoPassThru);
}

void Bug_ConfigDialog::aoDeviceCBChanged()
{
    mainApp()->configureDialogController()->updateAORangeOnCBChange(aoPassThru);
}

void Bug_ConfigDialog::ttlAltClicked(bool b)
{
    dialog->altTrigParams->setEnabled(b);
    dialog->altTrigParams->setHidden(!b);
    dialog->trigParams->setEnabled(!b);
    dialog->trigParams->setHidden(b);
}
