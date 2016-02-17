#include "Bug_Popout.h"
#include "Util.h"
#include <QPainter>
#include <QMutexLocker>
#include <QTimer>
#include <QThread>
#include "MainApp.h"
#include "ConfigureDialogController.h"
#include "PagedRingBuffer.h"

class Bug_MetaPlotThread : public QThread
{
public:
    Bug_MetaPlotThread(Bug_Popout *popout, unsigned taskRateHz, const PagedScanWriter & psw);
    ~Bug_MetaPlotThread();
protected:
    void run(); ///< reimplemented from QThread
private:
    Bug_Popout *popout;
    PagedScanReader reader;
    const unsigned taskRateHz;
    volatile bool pleaseStop;
};

Bug_MetaPlotThread::Bug_MetaPlotThread(Bug_Popout *popout, unsigned taskRateHz, const PagedScanWriter &psw)
    : QThread(popout), popout(popout), reader(psw), taskRateHz(taskRateHz), pleaseStop(false)
{}

Bug_MetaPlotThread::~Bug_MetaPlotThread()
{
    if (isRunning()) { pleaseStop = true; wait(); }
}

void Bug_MetaPlotThread::run()
{
    const unsigned sleeptime_ms = taskRateHz ? qRound((double(reader.scansPerPage()) / double(taskRateHz)) * 1e3 * 0.5) : 1 ;

    Debug() << "Bug_MetaPlotThread sleeptime_ms = " << sleeptime_ms;

    while (!pleaseStop) {
        DAQ::BugTask::BlockMetaData *meta = 0;
        int skips = 0; unsigned nscans = 0;
        const int16 *scan = reader.next(&skips,(void **)&meta, &nscans);
        if (scan && meta) {
            popout->plotMeta(*meta);
        } else {
            msleep(sleeptime_ms);
        }
    }
}

Bug_Popout::Bug_Popout(DAQ::BugTask *task, QWidget *parent)
    : QWidget(parent), task(task), p(task->bugParams()), ap(task->allParams()), avgPower(0.0), nAvg(0), logBER(0.0), mut(QMutex::Recursive)
{
    hasMeta = false;

	ui = new Ui::Bug_Popout;
	ui->setupUi(this);	
	
	setupGraphs();
	
	if (p.hpf > 0) { ui->hpfChk->setChecked(true); ui->hpfSB->setValue(p.hpf); } else { ui->hpfChk->setChecked(false); }
	ui->snfChk->setChecked(p.snf);
	ui->statusLabel->setText("Startup...");
	
	Connect(ui->snfChk, SIGNAL(toggled(bool)), this, SLOT(filterSettingsChanged()));
	Connect(ui->hpfChk, SIGNAL(toggled(bool)), this, SLOT(filterSettingsChanged()));
    Connect(ui->hpfSB, SIGNAL(valueChanged(int)), this, SLOT(filterSettingsChanged()));
	
    setupAOPassThru();

	mainApp()->sortGraphsByElectrodeAct->setEnabled(false);
	
	lastStatusT = getTime();
	lastStatusBlock = -1;
	lastRate = 0.;

    QTimer *t = new QTimer(this);
    t->setInterval(250);
    t->setSingleShot(false);
    Connect(t, SIGNAL(timeout()), this, SLOT(updateDisplay()));
    t->start();

    plotThread = new Bug_MetaPlotThread(this,task->samplingRate(),task->pagedWriter());
    plotThread->start(QThread::LowPriority);
}

Bug_Popout::~Bug_Popout()
{
    if (plotThread) delete plotThread, plotThread = 0;
	mainApp()->sortGraphsByElectrodeAct->setEnabled(true);
	delete ui; ui = 0;
}

void Bug_Popout::writeMetaToBug3File(const DataFile &df, const DAQ::BugTask::BlockMetaData &m/*, int fudge*/)
{
	QString fname (df.metaFileName());
	static const QString metaExt(".meta");
	if (fname.toLower().endsWith(metaExt)) fname = fname.left(fname.size()-metaExt.size()) + ".bug3";
	QFile f(fname);
	if (!df.scanCount()) {
		f.open(QIODevice::WriteOnly|QIODevice::Truncate|QIODevice::Text);
		Debug() << "Bug3 'extra data' file created: " << fname;
	} else {
		f.open(QIODevice::WriteOnly|QIODevice::Append|QIODevice::Text);
		f.seek(f.size()); // got to end?
	}
	QTextStream ts(&f);
	ts << "[ block " << m.blockNum << " ]\n";
	ts << "framesThisBlock = " << DAQ::BugTask::FramesPerBlock << "\n";
    ts << "spikeGL_DataFile_ScanCount = " << (df.scanCount()/*+u64(fudge/task->numChans())*/) << "\n";
    ts << "spikeGL_DataFile_SampleCount = " << (df.sampleCount()/*+u64(fudge)*/) << "\n";
	ts << "spikeGL_ScansInBlock = " << DAQ::BugTask::SpikeGLScansPerBlock << "\n";
	ts << "boardFrameCounter = ";
	for (int i = 0; i < DAQ::BugTask::FramesPerBlock; ++i) {
		if (i) ts << ",";
		ts << m.boardFrameCounter[i];
	}
	ts << "\n";
	ts << "boardFrameTimer = ";
	for (int i = 0; i < DAQ::BugTask::FramesPerBlock; ++i) {
		if (i) ts << ",";
		ts << m.boardFrameTimer[i];
	}
	ts << "\n";
	ts << "chipFrameCounter = ";
	for (int i = 0; i < DAQ::BugTask::FramesPerBlock; ++i) {
		if (i) ts << ",";
		ts << m.chipFrameCounter[i];
	}
	ts << "\n";
	ts << "chipID = ";
	for (int i = 0; i < DAQ::BugTask::FramesPerBlock; ++i) {
		if (i) ts << ",";
		ts << m.chipID[i];
	}
	ts << "\n";
	ts << "frameMarkerCorrelation = ";
	for (int i = 0; i < DAQ::BugTask::FramesPerBlock; ++i) {
		if (i) ts << ",";
		ts << m.frameMarkerCorrelation[i];
	}
	ts << "\n";
	ts << "missingFrameCount = " << m.missingFrameCount << "\n";
	ts << "falseFrameCount = " << m.falseFrameCount << "\n";
	ts << "BER = " << m.BER << "\n";
	ts << "WER = " << m.WER << "\n";
	ts << "avgVunreg = " << m.avgVunreg << "\n";
	ts.flush();
}

void Bug_Popout::plotMeta(const DAQ::BugTask::BlockMetaData & meta)
{		
    QMutexLocker l(&mut);

    avgPower = (avgPower*double(nAvg) + meta.avgVunreg)/double(nAvg+1);
    if (++nAvg > DAQ::BugTask::MaxMetaData) nAvg = DAQ::BugTask::MaxMetaData;

	vgraph->pushPoint(1.0-((meta.avgVunreg-1.)/4.0));
	
	// BER/WER handling...
    double BER(meta.BER), WER (meta.WER), logWER;
	
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
    float pt;
    pt = ((-logBER)-1.)/4.0;
    if (pt < 0.) pt = 0.f;
    if (pt >= 1.) pt = .99f;
    errgraph->pushPoint(pt, 0);
    pt = ((-logWER)-1.)/4.0;
    if (pt < 0.) pt = 0.f;
    if (pt >= 1.) pt = .99f;
    errgraph->pushPoint(pt, 1);

//		Debug() << "logWER: " << logWER << " logBER: " << logBER;
	
	// colorize the BER/WER graph based on missing frame count
	if (meta.missingFrameCount == 0 && meta.falseFrameCount == 0) { 
		// do nothing special
		errgraph->pushBar(QColor(0,0,0,0));
	} else if (meta.missingFrameCount) {
		// blue....
		/*QList<QPair<QRectF, QColor> >  bgs (errgraph->bgRects());
		 int i = 0;
		 for (QList<QPair<QRectF, QColor> >::iterator it = bgs.begin(); it != bgs.end(); ++it, ++i) {
		 QPair<QRectF, QColor> & pair(*it);
		 pair.second = i%2 ? QColor(0,0,200,64) : QColor(0,0,64,64); // bright blue odd, dark blue even					
		 }
		 errgraph->setBGRects(bgs);
		 */
		int alpha = 32;
		alpha += meta.missingFrameCount*16;
		if (alpha > 255) alpha = 255;
		errgraph->pushBar(QColor(0,0,200,alpha/*64*/));
	} else if (meta.falseFrameCount) {
		// pink...
		/*QList<QPair<QRectF, QColor> >  bgs (errgraph->bgRects());
		 int i = 0;
		 for (QList<QPair<QRectF, QColor> >::iterator it = bgs.begin(); it != bgs.end(); ++it, ++i) {
		 QPair<QRectF, QColor> & pair(*it);
		 pair.second = i%2 ? QColor(200,0,0,64) : QColor(64,0,0,64); // bright pink odd, dark pink even					
		 }
		 errgraph->setBGRects(bgs);*/
		int alpha = 32;
		alpha += meta.falseFrameCount*16;
		if (alpha > 255) alpha = 255;
		errgraph->pushBar(QColor(200,0,0,alpha/*64*/));
	}
	
    lastMeta = meta;
    hasMeta = true;
}

void Bug_Popout::updateDisplay()
{
    QMutexLocker l(&mut);

    if (hasMeta) { // last iteration, update the labels with the "most recent" info
        const DAQ::BugTask::BlockMetaData & meta(lastMeta);

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
        QString statusTxt = QString("Read %1 USB blocks (%2 MS) - %3 KS/sec").arg(meta.blockNum+1).arg((meta.blockNum*samplesPerBlock)/1e6,3,'f',2).arg(lastRate/1e3);
#ifdef Q_OS_WIN
        if (mainApp()->isDebugMode()) {
            // DEBUG MODE ONLY: show in status bar the delay/lag information from bug3 subprocess to here!
            u64 now = getAbsTimeNS();
            QString s;
            QTextStream ts(&s);
            ts << statusTxt << " -"
             /*<< " blk# " << meta.blockNum*/
               << " usb_lag: " << (double(qint64(now)-qint64(meta.creation_absTimeNS))/1e9) << "s"
               << " com_lag: " << (double(qint64(now)-qint64(meta.comm_absTimeNS))/1e9) << "s";
            ts.flush();
            statusTxt = s;
        }
#endif
        ui->statusLabel->setText(statusTxt);
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

void Bug_Popout::setupAOPassThru()
{
    ui->aoChanCB->clear();
    ui->bug3ChanCB->clear();
    ui->aoControlGB->setEnabled(false);

    if (ap.aoPassthru) {
        ui->aoControlGB->setEnabled(true);
        QStringList allao(DAQ::GetAOChans(ap.aoDev));
        if (allao.empty()) return;
        unsigned aoch = 0;
        if (!ap.aoChannels.empty()) aoch = ap.aoChannels.first();
        foreach(QString aoch, allao) {
            ui->aoChanCB->addItem(aoch);
        }
        if (aoch < DAQ::GetNAOChans(ap.aoDev)) ui->aoChanCB->setCurrentIndex(aoch);
        unsigned bug3ch = ap.aoPassthruMap.contains(aoch) ? ap.aoPassthruMap[aoch] : 0;
        unsigned i;
        for (i = 0; i < task->numChans(); ++i) {
            ui->bug3ChanCB->addItem(task->getChannelName(i),QVariant(i));
        }
        if (bug3ch < i) ui->bug3ChanCB->setCurrentIndex(bug3ch);

        Connect(ui->aoChanCB, SIGNAL(currentIndexChanged(int)), this, SLOT(aoControlsChanged()));
        Connect(ui->bug3ChanCB, SIGNAL(currentIndexChanged(int)), this, SLOT(aoControlsChanged()));
    }
}

void Bug_Popout::aoControlsChanged()
{
    if (!ui->aoControlGB->isEnabled()) return;
    unsigned ao = ui->aoChanCB->currentIndex(), bug = ui->bug3ChanCB->currentIndex();
    ap.mutex.lock();
    p.aoPassthruString = ap.aoPassthruString = QString("%1=%2").arg(ao).arg(bug);
    ap.aoPassthruMap.clear(); ap.aoPassthruMap[ao] = bug;
    ap.aoChannels.clear(); ap.aoChannels.push_back(ao);
    ap.mutex.unlock();
    mainApp()->configureDialogController()->saveSettings(ConfigureDialogController::BUG);
}

Bug_Graph::Bug_Graph(QWidget *parent, unsigned nplots, unsigned maxPts)
    : QWidget(parent), maxPts(maxPts), pen_width(2.0f), mut(QMutex::Recursive)
{
	if (nplots == 0) nplots = 1;
	if (nplots > 10) nplots = 10;
	pts.resize(nplots);
	ptsCounts.resize(nplots);
	colors.resize(nplots);
	barsCount = 0;
	for (int i = 0; i < (int)nplots; ++i)
		colors[i] = QColor(0,0,0,255); // black
}

void Bug_Graph::pushPoint(float y, unsigned plotNum)
{
    QMutexLocker l(&mut);

	if (plotNum >= (unsigned)pts.size()) plotNum = pts.size()-1;
	pts[plotNum].push_back(y);
	++ptsCounts[plotNum];
	while (ptsCounts[plotNum] > maxPts) {
		pts[plotNum].pop_front();
		--ptsCounts[plotNum];
	}
}

void Bug_Graph::pushBar(const QColor & c)
{
    QMutexLocker l(&mut);

	bars.push_back(c);
	++barsCount;
	while (barsCount > maxPts) {
		bars.pop_front();
		--barsCount;
	}
}

void Bug_Graph::setBGRects(const QList<QPair<QRectF,QColor> > & bgs_in)
{
    QMutexLocker l(&mut);
	bgs = bgs_in;
}

void Bug_Graph::setPlotColor(unsigned p, const QColor & c)
{
    QMutexLocker l(&mut);
	if (p >= (unsigned)colors.size()) p = colors.size()-1;
	colors[p] = c;
}

void Bug_Graph::paintEvent(QPaintEvent *e)
{
    QMutexLocker l(&mut);
	(void) e;
	QPainter p(this);
	for (QList<QPair<QRectF, QColor> >::iterator it = bgs.begin(); it != bgs.end(); ++it) {
		QRectF & r = (*it).first; QColor & c = (*it).second;
		QRect rt(QPoint(r.topLeft().x() * width(), (r.topLeft().y()) * height()), QSize(r.width()*width(), r.height()*height()));
		p.fillRect(rt, c);
	}
	
	const int w = width(), h = height();
	
	for (int plotNum = 0; plotNum < pts.size(); ++plotNum) {
		const unsigned ct = ptsCounts[plotNum];
		if (ct > 0) {
			int i = 0;
            QPoint *pbuf = new QPoint[ct];
			for (QList<float>::iterator it = pts[plotNum].begin(); it != pts[plotNum].end(); ++it, ++i) {
				const float x = float(i)/float(maxPts) + 1./float(maxPts*2);
				const float y = *it;
				pbuf[i].setX(x*w);
				pbuf[i].setY(y*h);
			}
			
			QPen pen = p.pen(); pen.setColor(colors[plotNum]); pen.setWidthF(pen_width); pen.setCapStyle(Qt::RoundCap);
			p.setPen(pen);
			p.drawPoints(pbuf, ct);
            delete pbuf;
		}
	}
	
	if (barsCount > 0) {
		QPen pen = p.pen();
		pen.setWidthF(1.25f);
		pen.setCapStyle(Qt::RoundCap);
		int i = 0;
		for (QList<QColor>::iterator it = bars.begin(); it != bars.end() && i < int(barsCount); ++it, ++i) {
			const float x = float(i)/float(maxPts) + 1./float(maxPts*2);
			pen.setColor(*it);
			p.setPen(pen);
			float pos = x*w;
			p.drawLine(QPointF(pos, 0.f), QPointF(pos, float(h)));
		}
	}
	p.end();
}

