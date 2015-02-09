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
	Connect(dialog->ttlTrigCB, SIGNAL(currentIndexChanged(int)), this, SLOT(ttlTrigCBChanged()));
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
				p.bug.reset();
				p.bug.enabled = true;
				p.bug.rate = dialog->acqRateCB->currentIndex();
                if (dialog->ttlTrigCB->currentIndex() > 0) {
					p.bug.ttlTrig = dialog->ttlTrigCB->currentIndex()-1;
                    p.bug.whichTTLs |= (0x1 << p.bug.ttlTrig);
				}
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
				p.outputFile = dialog->outputFileLE->text();
				
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
				if (p.bug.ttlTrig > -1) {
					p.acqStartEndMode = DAQ::AITriggered;
					p.idxOfPdChan = ttlIdx;
					p.usePD = true;
					p.pdChanIsVirtual = true;
					p.pdChan = ttlIdx;
					p.pdThresh = 2.5; // volts.. 
					p.pdThreshW = static_cast<unsigned>(dialog->trigWSB->value());
					p.pdPassThruToAO = -1;
					p.pdStopTime = dialog->trigStopTimeSB->value();
					p.silenceBeforePD = dialog->trigPre->value()/1000.;
				}
				
				saveSettings();

				// this stuff doesn't need to be saved since it's constant and will mess up regular acq "remembered" values
				p.dev = "USB_Bug3";
				p.nExtraChans1 = 0;
				p.nExtraChans2 = 0;
				
				p.extClock = true;
				p.mode = DAQ::AIRegular;
				p.aoPassthru = 0;
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
	dialog->trigParams->setEnabled(dialog->ttlTrigCB->currentIndex() != 0);
}
