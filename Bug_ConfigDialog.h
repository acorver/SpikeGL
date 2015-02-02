/*
 *  Bug_ConfigDialog.h
 *  SpikeGL
 *
 *  Created by calin on 1/29/15.
 *  Copyright 2015 Calin Culianu <calin.culianu@gmail.com>. All rights reserved.
 *
 */
#ifndef Bug_ConfigDialog_H
#define Bug_ConfigDialog_H

#include "ui_Bug_ConfigDialog.h"
#include <QObject>
#include "SpikeGL.h"
#include <QVector>
#include "DAQ.h"

class QSettings;
class QDialog;

class Bug_ConfigDialog : public QObject
{
    Q_OBJECT
public:
    Bug_ConfigDialog(DAQ::Params & params, QObject *parent);
    ~Bug_ConfigDialog();
	
	DAQ::Params & acceptedParams;
	
    int exec();
	
    Ui::Bug_ConfigDialog *dialog; 
			
    bool isDialogVisible() const { return dialogW->isVisible(); }
	
    //QString paramsToString();
    //QString paramsFromString(const QString & paramString); ///< returns QString::null on success, or an explanatory error message on error
	
    //void loadSettings();
    //void saveSettings() const;
		
//	protected slots:    
	
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

