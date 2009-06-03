#ifndef ChanMappingController_H
#define ChanMappingController_H
#include <QObject>
#include <QVector>
#include "ui_ChanMapping.h"
#include "LeoDAQGL.h"
#include "ChanMap.h"
class QDialog;

class ChanMappingController : public QObject
{
    Q_OBJECT
public:
    ChanMappingController(QObject *parent=0);
    ~ChanMappingController();

    void loadSettings();
    void saveSettings();
    static unsigned defaultPinMapping[NUM_MUX_CHANS_MAX];

    ChanMapDesc mappingForGraph(unsigned graphNum) const;
    ChanMapDesc mappingForIntan(unsigned intan, unsigned intan_chan) const;

    ChanMap mappingForAll() const;

public slots:
    bool exec();
#ifdef TEST_CH_MAP_CNTRL
    void show();
#endif

private:
    void resetFromSettings();
    QDialog *dialogParent;
    Ui::ChanMapping *dialog;
    QVector<unsigned> pinMapping, eMapping;
    
};

#endif
