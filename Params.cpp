#include "Params.h"
#include "Util.h"
#include <QTextStream>
#include <QRegExp>
#include <QFile>
#include "SpikeGL.h"

void Params::fromString(const QString & instr)
{
    QString instrcpy (instr);
    QTextStream ts(&instrcpy, QIODevice::ReadOnly|QIODevice::Text);
    QString line;
    clear();

    // now parse remaining lines which should be name/value pairs
    while ( !(line = ts.readLine()).isNull() ) {
        line = line.trimmed();
        QRegExp re_comments("((;)|(#)|(//)).*");
        if (line.contains(re_comments)) {
            if (excessiveDebug) Debug() << "Comment found and skipped: `" << re_comments.cap(0) << "'";
            line.replace(re_comments, "");
            line = line.trimmed();
        }
        if (!line.length()) continue;
        QRegExp re("([^=]+)=(.*)");
        if (re.exactMatch(line)) {
            QString name = re.cap(1).trimmed(), value = re.cap(2).trimmed();
            
            Debug() << "parsed name=value: '" << name << "'='" << value << "'";
            (*this)[name] = value;
        } else {
            Debug() << "did not parse, setting '" << line << "'=''";
            (*this)[line] = "";
        }
    }
}

QString Params::toString() const
{
    QString ret("");
    QTextStream ts(&ret, QIODevice::WriteOnly|QIODevice::Append|QIODevice::Text);
    for (const_iterator it = begin(); it != end(); ++it) 
        ts << it.key() << " = " << it.value().toString() << "\n";
    ts.flush();
    return ret;
}

bool Params::fromFile(const QString & fn)
{
    QFile f(fn);
    if (!f.open(QIODevice::ReadOnly|QIODevice::Text)) return false;
    QString line, str("");
    QTextStream ts(&f), strts(&str, QIODevice::WriteOnly|QIODevice::Append|QIODevice::Text);
    while ( !(line = ts.readLine()).isNull() ) {
        strts << line << "\n";
    }
    strts.flush();
    fromString(str);
    return true;
}

bool Params::toFile(const QString & fn, bool append) const
{
    QFile f(fn);
    if (!f.open(QIODevice::WriteOnly|QIODevice::Text|(append ? QIODevice::Append : QIODevice::Truncate))) return false;    
    return (QTextStream(&f) << toString()).status() == QTextStream::Ok;
}

