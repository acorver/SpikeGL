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

Bug_ConfigDialog::Bug_ConfigDialog(QObject *parent) : QObject(parent)
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
	return dialogW->exec();
}