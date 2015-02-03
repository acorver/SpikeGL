#include "Bug_Popout.h"
#include "Util.h"


Bug_Popout::Bug_Popout(DAQ::BugTask *task, QWidget *parent)
: QWidget(parent), task(task), p(task->bugParams())
{
	ui = new Ui::Bug_Popout;
	ui->setupUi(this);
	
	if (p.hpf > 0) { ui->hpfChk->setChecked(true); ui->hpfSB->setValue(p.hpf); } else { ui->hpfChk->setChecked(false); }
	ui->snfChk->setChecked(p.snf);
	ui->statusLabel->setText("Startup...");
	
	Connect(ui->snfChk, SIGNAL(toggled(bool)), this, SLOT(filterSettingsChanged()));
	Connect(ui->hpfChk, SIGNAL(toggled(bool)), this, SLOT(filterSettingsChanged()));
	Connect(ui->hpfSB, SIGNAL(valueChanged(int)), this, SLOT(filterSettingsChanged()));
	
	uiTimer = new QTimer(this);
	Connect(uiTimer, SIGNAL(timeout()), this, SLOT(updateUI()));
	lastStatusT = getTime();
	lastStatusBlock = -1;
	lastRate = 0.;
	uiTimer->start(100); // update every 100 ms
}

Bug_Popout::~Bug_Popout()
{
	delete uiTimer; uiTimer = 0; delete ui; ui = 0;
}

void Bug_Popout::updateUI()
{
	if (!task->isRunning()) return;
	std::list<DAQ::BugTask::BlockMetaData> metaData;
	int ct = task->popMetaData(metaData);
	double avgPower = 0.0; int nAvg = 0;
	for (int i = 0; i < ct; ++i) {
		DAQ::BugTask::BlockMetaData & meta(metaData.front());
		
		avgPower += meta.avgVunreg; ++nAvg;
		
		
		// BER/WER handling...
		double BER(meta.BER), WER (meta.WER), logBER, logWER;
		
		if (BER > 0.0) {
			logBER = ::log10(BER);
			if (logBER > DAQ::BugTask::MaxBER) logBER = DAQ::BugTask::MaxBER;
			else if (logBER < DAQ::BugTask::MinBER) logBER = DAQ::BugTask::MinBER;
		} else	{
			logBER = -100.0;
		}
		
		if (WER > 0.0)	{
			logWER = ::log10(WER);
			if (logWER > DAQ::BugTask::MaxBER) logWER = DAQ::BugTask::MaxBER;
			else if (logWER < DAQ::BugTask::MinBER) logWER = DAQ::BugTask::MinBER;			
		} else {
			logWER = -100.0;
		}		
		
		// todo.. graph voltages and bit/word error rate here..
		
		if ((i+1) == ct) { // last iteration, update the labels with the "most recent" info
			ui->chipIdLbl->setText(QString::number(meta.chipID[DAQ::BugTask::FramesPerBlock-1]));
			ui->dataFoundLbl->setText(QString::number((meta.blockNum+1)*DAQ::BugTask::FramesPerBlock - (meta.missingFrameCount+meta.falseFrameCount)));
			ui->missingLbl->setText(QString::number(meta.missingFrameCount));
			ui->falseLbl->setText(QString::number(meta.falseFrameCount));
			ui->recVolLbl->setText(QString::number(avgPower/(double)nAvg,'f',3));
			ui->berLbl->setText(QString::number(logBER,'3',4));
			const quint64 samplesPerBlock = task->usbDataBlockSizeSamps();
			const double now = getTime(), diff = now - lastStatusT;
			if (diff >= 1.0) {
				lastRate = ((meta.blockNum-lastStatusBlock) * samplesPerBlock)/diff;
				lastStatusT = now;
				lastStatusBlock = meta.blockNum;
			}
			ui->statusLabel->setText(QString("Read %1 USB blocks (%2 MS) - %3 KS/sec").arg(meta.blockNum+1).arg((meta.blockNum*samplesPerBlock)/1e6,3,'f',2).arg(lastRate/1e3));
		}
		
		metaData.pop_front();
	}
}

void Bug_Popout::filterSettingsChanged()
{
	task->setNotchFilter(ui->snfChk->isChecked());
	task->setHPFilter(ui->hpfChk->isChecked() ? ui->hpfSB->value() : 0);
}
