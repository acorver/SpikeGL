#ifndef ChanMappingController_H
#define ChanMappingController_H
#include <QObject>
#include <QVector>
#include "ui_ChanMapping.h"
#include "SpikeGL.h"
#include "ChanMap.h"
#include "DAQ.h"
class QDialog;

class ChanMappingController : public QObject
{
    Q_OBJECT
public:
    ChanMappingController(QObject *parent=0);
    ~ChanMappingController();

	DAQ::Mode currentMode;
	
    void loadSettings();
    void saveSettings();
    static ChanMap defaultMapping[DAQ::N_Modes], defaultMapping2[DAQ::N_Modes] /**< dual dev mapping */;

    static ChanMapDesc defaultMappingForIntan(unsigned intan, unsigned intan_chan,
											  unsigned chans_per_intan);


	const ChanMap & mappingForMode(DAQ::Mode m, bool isDualDevMode) const { 
		if (isDualDevMode) return mapping2[m];
		return mapping[m]; 
	}
	
public slots:
    bool exec();
#ifdef TEST_CH_MAP_CNTRL
    void show();
#endif

private:
    void resetFromSettings();
	bool mappingFromForm();
    QDialog *dialogParent;
    Ui::ChanMapping *dialog;
	ChanMap mapping[DAQ::N_Modes], mapping2[DAQ::N_Modes] /**< dual dev mapping */;
};

#endif
