#include "ChanMap.h"
#include <QTextStream>
#include <QRegExp>
#include <QStringList>

QString ChanMapDesc::toString() const
{
	return  QString("graphNum:") + QString::number(graphNum)
			+ ", intan:" + QString::number(intan)
			+ ", intanCh:" + QString::number(intanCh)
			+ ", pch:" + QString::number(pch) 
			+ ", ech:" + QString::number(ech);
}

/*static*/ ChanMapDesc ChanMapDesc::fromString(const QString & s_in)
{
	ChanMapDesc ret;
	
	const QStringList sl = s_in.split(QRegExp(",\\s*"), QString::SkipEmptyParts);
	foreach (QString s, sl) {
		const QStringList il = s.split(QRegExp(":\\s*"));
		if (il.size() <= 2) {
			QString name = il.at(0);
			unsigned value = il.at(1).toUInt();
			if (name == "graphNum") ret.graphNum = value;
			if (name == "intan") ret.intan = value;
			if (name == "intanCh") ret.intanCh = value;
			if (name == "pch") ret.pch = value;
			if (name == "ech") ret.ech = value;
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

/*static*/ ChanMap ChanMap::fromString(const QString & s_in)
{
	const QStringList sl = s_in.split(QRegExp("(^\\()|(\\),)|(\\)$)"), QString::SkipEmptyParts);
	ChanMap ret;
	ret.reserve(sl.size());
	foreach(QString s, sl) {		
		QString inParens = s.length() > 2 ? s.mid(1, s.length()-2) : "";
		ret.push_back(ChanMapDesc::fromString(inParens));
	}
	return ret;
}
