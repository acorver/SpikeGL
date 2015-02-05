#ifndef Bug_Popout_H
#define Bug_Popout_H

#include <QWidget>
#include <QTimer>
#include <QPair>
#include <QList>
#include <QVector>
#include <QRectF>
#include <QColor>
#include <QSet>
#include "DAQ.h"
#include "ui_Bug_Popout.h"
#include "DataFile.h"

class Bug_Graph;

class Bug_Popout : public QWidget
{
	Q_OBJECT
public:
	Bug_Popout(DAQ::BugTask *task, QWidget *parent = 0);
	~Bug_Popout();

	void plotMeta(const DAQ::BugTask::BlockMetaData & meta, bool call_QWidget_update = true);
	void writeMetaToDataFile(DataFile & dataFile, const DAQ::BugTask::BlockMetaData & meta);
	
private slots:
	void filterSettingsChanged();
	
private:
	void setupGraphs();
	
	DAQ::BugTask *task;
	const DAQ::Params::Bug & p;
	double avgPower; int nAvg;
	Ui::Bug_Popout *ui;
	double lastStatusT; qint64 lastStatusBlock; double lastRate;
	
	Bug_Graph *vgraph, *errgraph;
	
	QSet<QString> seenMetaFiles; // used to detect when meta file changes so we can write a new header...
};


class Bug_Graph : public QWidget
{
public:
	Bug_Graph(QWidget *parent, unsigned nPlots = 1, unsigned maxPts=DAQ::BugTask::MaxMetaData);
	void setPlotColor(unsigned plotNum, const QColor & color);
	void pushPoint(float y, unsigned plotNum = 0);
	void setBGRects(const QList<QPair<QRectF,QColor> > & bgs);
	
	const QList<QPair<QRectF, QColor> > & bgRects() const { return bgs; }
	
protected:
	void paintEvent(QPaintEvent *);
private:
	unsigned maxPts;
	QVector<QList<float> > pts;
	QVector<QColor> colors;
	QList<QPair<QRectF, QColor> > bgs;
};
#endif