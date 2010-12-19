/*
 *  ClickableLabel.cpp
 *  SpikeGL
 *
 *  Created by calin on 12/19/10.
 *  Copyright 2010 Calin Culianu <calin.culianu@gmail.com>. All rights reserved.
 *
 */

#include "ClickableLabel.h"
#include <QMouseEvent>

ClickableLabel::ClickableLabel(QWidget *parent, Qt::WindowFlags f)
: QLabel(parent, f) {}

ClickableLabel::ClickableLabel(const QString & txt, QWidget *parent, Qt::WindowFlags f)
: QLabel(txt, parent, f) {}


void ClickableLabel::mousePressEvent(QMouseEvent *e)
{
	if (e->type() == QEvent::MouseButtonPress && e->button() == Qt::LeftButton) {
		emit clicked();
		e->accept();
	} else
		QLabel::mousePressEvent(e);
}
