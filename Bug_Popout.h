#ifndef Bug_Popout_H
#define Bug_Popout_H

#include <QWidget>
#include <QTime>
#include <QTimer>
#include <QPair>
#include <QList>
#include <QVector>
#include <QRectF>
#include <QColor>
#include <QSet>
#include <QMutex>

#include "DAQ.h"
#include "ui_Bug_Popout.h"
#include "DataFile.h"

class Bug_Graph;
class Bug_MetaPlotThread;

class Bug_Popout : public QWidget
{
	Q_OBJECT
public:
	Bug_Popout(DAQ::BugTask *task, QWidget *parent = 0);
	~Bug_Popout();

    void plotMeta(const DAQ::BugTask::BlockMetaData & meta);
    void writeMetaToBug3File(const DataFile & dataFile, const DAQ::BugTask::BlockMetaData & meta/*, int fudge_sampct = 0*/);

private slots:
	void filterSettingsChanged();
    void aoControlsChanged();

    void updateDisplay();
	
private:
	void setupGraphs();
    void setupAOPassThru();
	
	QTime* timeSinceStart;

	DAQ::BugTask *task;
    DAQ::Params::Bug & p;
    DAQ::Params & ap;
    double avgPower; int nAvg; double logBER;
	Ui::Bug_Popout *ui;
	double lastStatusT; qint64 lastStatusBlock; double lastRate;
	
	Bug_Graph *vgraph, *errgraph;

    QMutex mut;
    volatile bool hasMeta;
    DAQ::BugTask::BlockMetaData lastMeta;

    Bug_MetaPlotThread *plotThread;
};


class Bug_Graph : public QWidget
{
public:
	Bug_Graph(QWidget *parent, unsigned nPlots = 1, unsigned maxPts=DAQ::BugTask::MaxMetaData);
	void setPlotColor(unsigned plotNum, const QColor & color);
	void pushPoint(float y, unsigned plotNum = 0);
	void pushBar(const QColor & barColor);
	void setBGRects(const QList<QPair<QRectF,QColor> > & bgs);
	void setPenWidth(float width_pix) { if (width_pix < 0.f) width_pix = 0.f; pen_width = width_pix; }
	float penWidth() const { return pen_width; }
	
	const QList<QPair<QRectF, QColor> > & bgRects() const { return bgs; }
	
protected:
	void paintEvent(QPaintEvent *);
private:
	unsigned maxPts;
	float pen_width;
	QVector<QList<float> > pts;
	QVector<unsigned> ptsCounts;
	QList<QColor> bars;
	unsigned barsCount;
	QVector<QColor> colors;
	QList<QPair<QRectF, QColor> > bgs;
    QMutex mut;
};
#endif
