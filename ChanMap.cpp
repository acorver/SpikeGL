#include "ChanMap.h"
#include <QTextStream>
#include <QRegExp>
#include <QStringList>

QString ChanMapDesc::toTerseString() const
{
	return  QString("g:") + QString::number(graphNum)
	+ ",i:" + QString::number(intan)
	+ ",ic:" + QString::number(intanCh)
	+ ",p:" + QString::number(pch) 
	+ ",e:" + QString::number(ech);
}

QString ChanMapDesc::toString() const
{
	return  QString("graphNum:") + QString::number(graphNum)
			+ ", intan:" + QString::number(intan)
			+ ", intanCh:" + QString::number(intanCh)
			+ ", pch:" + QString::number(pch) 
			+ ", ech:" + QString::number(ech);
}

//#include "Util.h"

/*static*/ ChanMapDesc ChanMapDesc::fromString(const QString & s_in)
{
	ChanMapDesc ret;
	
	const QStringList sl = s_in.split(QRegExp(",\\s*"), QString::SkipEmptyParts);
	//Debug() << "s_in: " << s_in;
	foreach (QString s, sl) {
		const QStringList il = s.split(QRegExp(":\\s*"));
		if (il.size() >= 2) {
			QString name = il.at(0);
			unsigned value = il.at(1).toUInt();
			//Debug() << "name: " << name << " value: " << value;
			if (name == "graphNum") ret.graphNum = value;
			if (name == "intan") ret.intan = value;
			if (name == "intanCh") ret.intanCh = value;
			if (name == "pch") ret.pch = value;
			if (name == "ech") ret.ech = value;
		}
	}
	return ret;
}

/*static*/ ChanMapDesc ChanMapDesc::fromTerseString(const QString & s_in)
{
	ChanMapDesc ret;
	
	const QStringList sl = s_in.split(QRegExp(",\\s*"), QString::SkipEmptyParts);
	//Debug() << "s_in: " << s_in;
	foreach (QString s, sl) {
		const QStringList il = s.split(QRegExp(":\\s*"));
		if (il.size() >= 2) {
			QString name = il.at(0);
			unsigned value = il.at(1).toUInt();
			//Debug() << "name: " << name << " value: " << value;
			if (name == "g") ret.graphNum = value;
			if (name == "i") ret.intan = value;
			if (name == "ic") ret.intanCh = value;
			if (name == "p") ret.pch = value;
			if (name == "e") ret.ech = value;
		}
	}
	return ret;
}


QString ChanMap::toString() const
{
	QString ret;
	QTextStream ts(&ret, QIODevice::WriteOnly|QIODevice::Truncate);
	const int n = size();
	for (int i = 0; i < n; ++i) {
		if (i) ts << ",";
		ts << "(" << (*this)[i].toString() << ")";
	}
	ts.flush();
	return ret;
}

QString ChanMap::toTerseString(const QBitArray & bm) const
{
	QString ret;
	QTextStream ts(&ret, QIODevice::WriteOnly|QIODevice::Truncate);
	const int n = size(), n2 = bm.size();
	ts << "(" << n << ")";
	for (int i = 0; i < n && i < n2; ++i) {
		if (!bm.testBit(i)) continue;
		ts << ",(" << (*this)[i].toTerseString() << ")";
	}
	ts.flush();
	return ret;
}

/*static*/ ChanMap ChanMap::fromTerseString(const QString & s_in) 
{
	const QStringList sl = s_in.split(QRegExp("(^\\()|(\\),)|(\\)$)"), QString::SkipEmptyParts);
	ChanMap ret;
	int i = 0;
	
	foreach(QString s, sl) {	
		const int sp = s.startsWith("(") ? 1 : 0, ep = s.endsWith(")") ? 1 : 0;
		//Debug() << "s: " << s;
		QString inParens = s.length() > 2 ? s.mid(sp, s.length()-(sp+ep)) : "";
		if (0==i) {
			// first thing in the list is the size of the total chanmap
			bool ok;
			unsigned chanMapSize = inParens.toUInt(&ok);
			if (!ok) ret.resize(120);
			else ret.resize(chanMapSize);
		} else {
			// rest of the items are the chan map tuples, only the "on" channels exist
			ChanMapDesc cmd (ChanMapDesc::fromTerseString(inParens));
			ret[cmd.graphNum] = cmd;
		}
		++i;
	}
	return ret;
}

/*static*/ ChanMap ChanMap::fromString(const QString & s_in)
{
	const QStringList sl = s_in.split(QRegExp("(^\\()|(\\),)|(\\)$)"), QString::SkipEmptyParts);
	ChanMap ret;
	ret.reserve(sl.size());

	foreach(QString s, sl) {	
		const int sp = s.startsWith("(") ? 1 : 0, ep = s.endsWith(")") ? 1 : 0;
		//Debug() << "s: " << s;
		QString inParens = s.length() > 2 ? s.mid(sp, s.length()-(sp+ep)) : "";
		ret.push_back(ChanMapDesc::fromString(inParens));
	}
	return ret;
}
