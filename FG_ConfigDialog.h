/*
 *  Bug_ConfigDialog.h
 *  SpikeGL
 *
 *  Created by calin on 1/29/15.
 *  Copyright 2015 Calin Culianu <calin.culianu@gmail.com>. All rights reserved.
 *
 */
#ifndef FG_ConfigDialog_H
#define FG_ConfigDialog_H

#include <QObject>
#include "SpikeGL.h"
#include <QVector>
#include "DAQ.h"
#include "ui_FG_ConfigDialog.h"

class QSettings;
class QDialog;
class QCheckBox;

class FG_ConfigDialog : public QObject
{
    Q_OBJECT
public:
    FG_ConfigDialog(DAQ::Params & params, QObject *parent);
    ~FG_ConfigDialog();
	
	DAQ::Params & acceptedParams;
	
    int exec();
	
    Ui::FG_ConfigDialog *dialog; 
			
    bool isDialogVisible() const { return dialogW->isVisible(); }
		
private:
	
   enum ValidationResult {
        AGAIN = -1,
        ABORT =  0,
        OK    =  1
    };
    
    ValidationResult validateForm(QString & errTitle, QString & errMsg, bool isGUI = false);
	
	
	void guiFromSettings(); 
	void saveSettings();
	
    QDialog *dialogW;
};


#endif

