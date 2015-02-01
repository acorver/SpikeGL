/*
 *  Bug_ConfigDialog.cpp
 *  SpikeGL
 *
 *  Created by calin on 1/29/15.
 *  Copyright 2015 Calin Culianu <calin.culianu@gmail.com>. All rights reserved.
 *
 */

#include <QDialog>
#include "Bug_ConfigDialog.h"
#include "Util.h"
#include "MainApp.h"
#include "ConfigureDialogController.h"

Bug_ConfigDialog::Bug_ConfigDialog(DAQ::Params & p, QObject *parent) : QObject(parent), acceptedParams(p)
{
	dialog = new Ui::Bug_ConfigDialog;
	dialog->setupUi(dialogW = new QDialog(0));
}

Bug_ConfigDialog::~Bug_ConfigDialog()
{
	delete dialog;
	delete dialogW;
	dialog = 0;
	dialogW = 0;
}

int Bug_ConfigDialog::exec() 
{
	mainApp()->configureDialogController()->loadSettings(); // this makes the shared params object get updated form the settings

	guiFromSettings();
	
	int r = dialogW->exec();
	QString errTit, errMsg;
	if (r == QDialog::Accepted) {
		ValidationResult vr = validateForm(errTit, errMsg);
		if (vr == OK) {
			DAQ::Params & p(acceptedParams);
			
			p.bug.enabled = true;
			p.bug.rate = dialog->acqRateCB->currentIndex();
			p.bug.whichTTLs = 0;
			p.bug.ttlTrig = -1;
			if (dialog->ttlTrigCB->currentIndex()) {
				p.bug.ttlTrig = dialog->ttlTrigCB->currentIndex()-1;
				p.bug.whichTTLs = 0x1 << p.bug.ttlTrig;
			}
			p.bug.clockEdge = dialog->clkEdgeCB->currentIndex();
			p.bug.hpf = dialog->hpfChk->isChecked() ? dialog->hpfSB->value(): 0;
			p.bug.snf = dialog->notchFilterChk->isChecked();
			p.suppressGraphs = dialog->disableGraphsChk->isChecked();
			p.resumeGraphSettings = dialog->resumeGraphSettingsChk->isChecked();
			p.outputFile = dialog->outputFileLE->text();
			p.mode = DAQ::AIRegular;
			p.srate = 16.0 / 0.0006144;
			p.nVAIChans = 16 + (p.bug.whichTTLs ? 1 : 0);
			p.nVAIChans1 = p.nVAIChans;
			p.nVAIChans2 = 0;
			p.aoPassthru = 0;
			p.aiChannels2.clear();
			p.aiString2.clear();
			p.aiChannels.resize(p.nVAIChans);
			p.subsetString = dialog->channelSubsetLE->text();
			p.aiString = "";
			p.demuxedBitMap.resize(p.nVAIChans);
			for (int i = 0; i < (int)p.aiChannels.size(); ++i) {
				p.aiChannels[i] = i;
				if (i) p.aiString.append(",");
				p.aiString.append(QString::number(i));
				p.demuxedBitMap[i] = true;
			}
			p.nExtraChans1 = p.nExtraChans2 = 0;
			p.isIndefinite = true;
			p.isImmediate = true;
			p.acqStartEndMode = DAQ::Immediate;
			p.usePD = 0;
			p.dualDevMode = false;
			p.stimGlTrigResave = false;
		}
	}
	
	return r;
}

void Bug_ConfigDialog::guiFromSettings()
{
	DAQ::Params & p(acceptedParams);
	dialog->outputFileLE->setText(p.outputFile);
	dialog->channelSubsetLE->setText(p.subsetString);
}

Bug_ConfigDialog::ValidationResult Bug_ConfigDialog::validateForm(QString & errTitle, QString & errMsg, bool isGUI)
{
	errTitle = ""; errMsg = ""; (void)isGUI;
	return OK;
}
