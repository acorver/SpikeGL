#ifndef ChanMap_H
#define ChanMap_H
#include <QVector>

struct ChanMapDesc 
{
    unsigned graphNum, intan, intanCh, pch, ech;
    ChanMapDesc() : graphNum(0), intan(0), intanCh(0), pch(0), ech(0) {}
};

typedef QVector<ChanMapDesc> ChanMap;


#endif
