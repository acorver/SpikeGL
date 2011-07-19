#ifndef ChanMap_H
#define ChanMap_H
#include <QString>
#include <QVector>
#include <QBitArray>

struct ChanMapDesc 
{
    unsigned intan, intanCh, electrodeId;
    ChanMapDesc() : intan(0), intanCh(0), electrodeId(0) {}
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
