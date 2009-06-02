#ifndef ChanMappingController_H
#define ChanMappingController_H
#include <QObject>
#include <QVector>
#include "ui_ChanMapping.h"
#include "LeoDAQGL.h"
class QDialog;

struct ChanMapDesc 
{
    unsigned graphNum, intan, intanCh, pch, ech;
    ChanMapDesc() : graphNum(0), intan(0), intanCh(0), pch(0), ech(0) {}
};

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

    QVector<ChanMapDesc> mappingForAll() const;

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
