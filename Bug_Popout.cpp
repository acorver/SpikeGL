#include "Bug_Popout.h"
#include "Util.h"
#include <QPainter>
#include "MainApp.h"

Bug_Popout::Bug_Popout(DAQ::BugTask *task, QWidget *parent)
: QWidget(parent), task(task), p(task->bugParams()), avgPower(0.0), nAvg(0)
{
	ui = new Ui::Bug_Popout;
	ui->setupUi(this);
	
	
	setupGraphs();
	
	if (p.hpf > 0) { ui->hpfChk->setChecked(true); ui->hpfSB->setValue(p.hpf); } else { ui->hpfChk->setChecked(false); }
	ui->snfChk->setChecked(p.snf);
	ui->statusLabel->setText("Startup...");
	
	Connect(ui->snfChk, SIGNAL(toggled(bool)), this, SLOT(filterSettingsChanged()));
	Connect(ui->hpfChk, SIGNAL(toggled(bool)), this, SLOT(filterSettingsChanged()));
	Connect(ui->hpfSB, SIGNAL(valueChanged(int)), this, SLOT(filterSettingsChanged()));
	
	mainApp()->sortGraphsByElectrodeAct->setEnabled(false);
	
	lastStatusT = getTime();
	lastStatusBlock = -1;
	lastRate = 0.;
}

Bug_Popout::~Bug_Popout()
{
	mainApp()->sortGraphsByElectrodeAct->setEnabled(true);
	delete ui; ui = 0;
}

void Bug_Popout::writeMetaToDataFile(DataFile &f, const DAQ::BugTask::BlockMetaData &m, int fudge)
{
	if (!seenMetaFiles.contains(f.metaFileName())) {
		f.writeCommentToMetaFile("USB Bug3 \"Meta data\" is in the comments BELOW!! Format is as follows:");
		f.writeCommentToMetaFile("blockNum "
								 "framesPerBlock "
								 "spikeGL_DataFile_ScanCount "
								 "spikeGL_DataFile_SampleCount "
								 "spikeGL_ScansInBlock "
								 "boardFrameCtr_BlockBegin "
								 "boardFrameCtr_BlockEnd "
								 "frameTimer_BlockBegin "
								 "frameTimer_BlockEnd "
								 "chipID "
								 "chipFrameCtr_BlockBegin "
								 "chipFrameCtr_BlockEnd "
								 "missingFrameCount "
								 "falseFrameCount "
								 "bitErrorRate "
								 "wordErrorRate "
								 "avgVunreg_FramesThisBlock "
								 "avgVunreg_Last120Blocks ");
		seenMetaFiles.insert(f.metaFileName());
	}		
	f.writeCommentToMetaFile (	
      QString("%1 %2 %3 %4 %5 %6 %7 %8 %9 %10 %11 %12 %13 %14 %15 %16 %17 %18")
	   .arg(m.blockNum)
 	   .arg(DAQ::BugTask::FramesPerBlock)
	   .arg(f.scanCount()+fudge)
	   .arg(f.sampleCount()+fudge)
	   .arg(DAQ::BugTask::SpikeGLScansPerBlock)
	   .arg(m.boardFrameCounter[0])
	   .arg(m.boardFrameCounter[DAQ::BugTask::FramesPerBlock-1])
	   .arg(m.boardFrameTimer[0])
	   .arg(m.boardFrameTimer[DAQ::BugTask::FramesPerBlock-1])
	   .arg(m.chipID[0])
	   .arg(m.chipFrameCounter[0])
	   .arg(m.chipFrameCounter[DAQ::BugTask::FramesPerBlock-1])
	   .arg(m.missingFrameCount)
	   .arg(m.falseFrameCount)
	   .arg(m.BER)
	   .arg(m.WER)
	   .arg(m.avgVunreg)
	   .arg(avgPower) 
	 );
}

void Bug_Popout::plotMeta(const DAQ::BugTask::BlockMetaData & meta, bool call_update)
{		
	avgPower = (avgPower*double(nAvg) + meta.avgVunreg)/double(++nAvg);
	if (nAvg > DAQ::BugTask::MaxMetaData) nAvg = DAQ::BugTask::MaxMetaData;

	vgraph->pushPoint(1.0-((meta.avgVunreg-1.)/4.0));
	
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
	
	// graph voltages and bit/word error rate here..
	errgraph->pushPoint(((-logBER)-1.)/4.0, 0);
	errgraph->pushPoint(((-logWER)-1.)/4.0, 1);

//		Debug() << "logWER: " << logWER << " logBER: " << logBER;
	
	
	if (call_update) { // last iteration, update the labels with the "most recent" info
		ui->chipIdLbl->setText(QString::number(meta.chipID[DAQ::BugTask::FramesPerBlock-1]));
		ui->dataFoundLbl->setText(QString::number((meta.blockNum+1)*DAQ::BugTask::FramesPerBlock - (meta.missingFrameCount+meta.falseFrameCount)));
		ui->missingLbl->setText(QString::number(meta.missingFrameCount));
		ui->falseLbl->setText(QString::number(meta.falseFrameCount));
		ui->recVolLbl->setText(QString::number(avgPower,'f',3));
		ui->berLbl->setText(QString::number(logBER,'3',4));
		const quint64 samplesPerBlock = task->usbDataBlockSizeSamps();
		const double now = getTime(), diff = now - lastStatusT;
		if (diff >= 1.0) {
			lastRate = ((meta.blockNum-lastStatusBlock) * samplesPerBlock)/diff;
			lastStatusT = now;
			lastStatusBlock = meta.blockNum;
		}
		ui->statusLabel->setText(QString("Read %1 USB blocks (%2 MS) - %3 KS/sec").arg(meta.blockNum+1).arg((meta.blockNum*samplesPerBlock)/1e6,3,'f',2).arg(lastRate/1e3));
		
		
		// colorize the BER/WER graph based on missing frame count
		if (meta.missingFrameCount == 0 && meta.falseFrameCount == 0) { 
			// do nothing special
		} else if (meta.missingFrameCount) {
			// blue....
			QList<QPair<QRectF, QColor> >  bgs (errgraph->bgRects());
			int i = 0;
			for (QList<QPair<QRectF, QColor> >::iterator it = bgs.begin(); it != bgs.end(); ++it, ++i) {
				QPair<QRectF, QColor> & pair(*it);
				pair.second = i%2 ? QColor(0,0,200,64) : QColor(0,0,64,64); // bright blue odd, dark blue even					
			}
			errgraph->setBGRects(bgs);
		} else if (meta.falseFrameCount) {
			// pink...
			QList<QPair<QRectF, QColor> >  bgs (errgraph->bgRects());
			int i = 0;
			for (QList<QPair<QRectF, QColor> >::iterator it = bgs.begin(); it != bgs.end(); ++it, ++i) {
				QPair<QRectF, QColor> & pair(*it);
				pair.second = i%2 ? QColor(200,0,0,64) : QColor(64,0,0,64); // bright pink odd, dark pink even					
			}
			errgraph->setBGRects(bgs);
		}

		errgraph->update();
		vgraph->update();

	}	
}

void Bug_Popout::filterSettingsChanged()
{
	task->setNotchFilter(ui->snfChk->isChecked());
	task->setHPFilter(ui->hpfChk->isChecked() ? ui->hpfSB->value() : 0);
}

void Bug_Popout::setupGraphs()
{
	vgraph = new Bug_Graph(ui->uvsW);
	vgraph->setGeometry(0,0,ui->uvsW->width(), ui->uvsW->height());
	QPair<QRectF, QColor> pair;
	QList<QPair<QRectF, QColor> > pairs;
	// setup bg zones for vunreg graph
	pair.first = QRectF(0.,0.,1.,1.4/4.0);
	pair.second = QColor(255,255,0,32); // yellow top area, 5V at top to 1.6V at bottom
	pairs.push_back(pair);
	pair.first = QRectF(0.,3.4/4.0, 1., .25/4.);
	pairs.push_back(pair); // second yellow area just under green area
	pair.first = QRectF(0.,1.0-(2.6/4.),1.,2./4.); // green area, 3.6V at top to 1.6V at bottom
	pair.second = QColor(0,255,0,64); // green
	pairs.push_back(pair);
	pair.second = QColor(255,0,0,64); // red area at bottom
	pair.first = QRectF(0.,1.0-(.35/4.0),1.,.35/4.0);
	pairs.push_back(pair);
	vgraph->setBGRects(pairs);	
	vgraph->update();
	
	errgraph = new Bug_Graph(ui->errW, 2);
	errgraph->setGeometry(0,0,ui->errW->width(), ui->errW->height());
	pairs.clear();
	for (int i = 0; i < 4; ++i) {
		pair.second = i%2 ? QColor(255,255,255,64) : QColor(128,128,128,64); // white odd, gray even
		pair.first = QRectF(0.,i/4.,1.,1./4.);
		pairs.push_back(pair);
	}
	errgraph->setBGRects(pairs);
	errgraph->setPlotColor(1, QColor(255,0,0,255));
	errgraph->update();
}

Bug_Graph::Bug_Graph(QWidget *parent, unsigned nplots, unsigned maxPts)
: QWidget(parent), maxPts(maxPts) 
{
	if (nplots == 0) nplots = 1;
	if (nplots > 10) nplots = 10;
	pts.resize(nplots);
	colors.resize(nplots);
	for (int i = 0; i < (int)nplots; ++i)
		colors[i] = QColor(0,0,0,255); // black
}

void Bug_Graph::pushPoint(float y, unsigned plotNum)
{
	if (plotNum >= (unsigned)pts.size()) plotNum = pts.size()-1;
	pts[plotNum].push_back(y);
	while (pts[plotNum].count() > (int)maxPts) pts[plotNum].pop_front();
}

void Bug_Graph::setBGRects(const QList<QPair<QRectF,QColor> > & bgs_in)
{
	bgs = bgs_in;
}

void Bug_Graph::setPlotColor(unsigned p, const QColor & c)
{
	if (p >= (unsigned)colors.size()) p = colors.size()-1;
	colors[p] = c;
}

void Bug_Graph::paintEvent(QPaintEvent *e)
{
	(void) e;
	QPainter p(this);
	for (QList<QPair<QRectF, QColor> >::iterator it = bgs.begin(); it != bgs.end(); ++it) {
		QRectF & r = (*it).first; QColor & c = (*it).second;
		QRect rt(QPoint(r.topLeft().x() * width(), (r.topLeft().y()) * height()), QSize(r.width()*width(), r.height()*height()));
		p.fillRect(rt, c);
	}
	
	for (int plotNum = 0; plotNum < pts.size(); ++plotNum) {
		const int ct = pts[plotNum].count();
		if (ct > 0) {
			int i = 0;
            QPoint *pbuf = new QPoint[ct];
			for (QList<float>::iterator it = pts[plotNum].begin(); it != pts[plotNum].end(); ++it, ++i) {
				const float x = float(i)/float(maxPts) + 1./float(maxPts*2);
				const float y = *it;
				pbuf[i].setX(x*width());
				pbuf[i].setY(y*height());
			}
			
			QPen pen = p.pen(); pen.setColor(colors[plotNum]); pen.setWidth(0); // 'cosmetic' pen, using black
			p.setPen(pen);
			p.drawPoints(pbuf, ct);
            delete pbuf;
		}
	}
	p.end();
}
