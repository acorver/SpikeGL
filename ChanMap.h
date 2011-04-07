#ifndef ChanMap_H
#define ChanMap_H
#include <QString>
#include <QVector>
#include <QBitArray>

struct ChanMapDesc 
{
    unsigned graphNum, intan, intanCh, pch, ech;
    ChanMapDesc() : graphNum(0), intan(0), intanCh(0), pch(0), ech(0) {}
	QString toString() const;
	QString toTerseString() const;
	static ChanMapDesc fromString(const QString &);
	static ChanMapDesc fromTerseString(const QString &);
};

struct ChanMap : public QVector<ChanMapDesc>
{
	QString toString() const;
	QString toTerseString(const QBitArray & chansOnBitMap) const;
	static ChanMap fromString(const QString &);
	static ChanMap fromTerseString(const QString &);
};


#endif
