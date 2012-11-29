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
	QString toWSDelimString() const;
	static ChanMapDesc fromString(const QString &);
	static ChanMapDesc fromTerseString(const QString &);
	static ChanMapDesc fromWSDelimString(const QString &);
};

struct ChanMap : public QVector<ChanMapDesc>
{
	QString toString() const;
	QString toTerseString(const QBitArray & chansOnBitMap) const;
	QString toWSDelimFlatFileString() const;
	static ChanMap fromString(const QString &);
	static ChanMap fromTerseString(const QString &);
	static ChanMap fromWSDelimFlatFileString(const QString &);
	void scrambleToPreJuly2011Demux();
};


#endif
