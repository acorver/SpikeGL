/*
 *  ClickableLabel.h
 *  SpikeGL
 *
 *  Created by calin on 12/19/10.
 *  Copyright 2010 Calin Culianu <calin.culianu@gmail.com>. All rights reserved.
 *
 */
#include <QLabel>

class ClickableLabel : public QLabel
{
Q_OBJECT
public:
	ClickableLabel(QWidget *parent = 0, Qt::WindowFlags f = 0);
	ClickableLabel(const QString & txt, QWidget *parent = 0, Qt::WindowFlags f = 0);
	
signals:
	void clicked();
	
protected:
	void mousePressEvent(QMouseEvent *e);
};
