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
	
    void loadSettings(DAQ::Mode currentMode);
    void saveSettings();
    static unsigned defaultPinMapping[DAQ::N_Modes][NUM_MUX_CHANS_MAX];

    ChanMapDesc mappingForGraph(unsigned graphNum) const;
    ChanMapDesc mappingForIntan(unsigned intan, unsigned intan_chan) const;

    ChanMap mappingForAll() const;

public slots:
    bool exec();
#ifdef TEST_CH_MAP_CNTRL
    void show();
#endif

private:
    void loadSettings();
    void resetFromSettings();
    QDialog *dialogParent;
    Ui::ChanMapping *dialog;
    QVector<unsigned> pinMapping, eMapping;
    
};

#endif
