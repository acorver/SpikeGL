#ifndef Bug_Popout_H
#define Bug_Popout_H

#include <QWidget>
#include <QTimer>
#include "DAQ.h"
#include "ui_Bug_Popout.h"

class Bug_Popout : public QWidget
{
	Q_OBJECT
public:
	Bug_Popout(DAQ::BugTask *task, QWidget *parent = 0);
	~Bug_Popout();

private slots:
	void updateUI();
	void filterSettingsChanged();
	
private:
	DAQ::BugTask *task;
	const DAQ::Params::Bug & p;
	Ui::Bug_Popout *ui;
	QTimer *uiTimer;
	double lastStatusT; quint64 lastStatusBlock; double lastRate;
};

#endif