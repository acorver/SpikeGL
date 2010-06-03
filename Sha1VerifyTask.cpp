#include "Sha1VerifyTask.h"
#include "Util.h"
#include "MainApp.h"
#include <QProgressDialog>
#include <QFileInfo>
#include <QFile>
#include "sha1.h"
#include "ConsoleWindow.h"

Sha1VerifyTask::Sha1VerifyTask(const QString & dfn, 
                               const Params & params_in, QObject *parent)
    : QThread(parent), Sha1Verifier(dfn, params_in)
{
    prog = new QProgressDialog(QString("Verifying SHA1 hash of " + dataFileNameShort + "..."), "Cancel", 0, 100, mainApp()->console());
    Connect(this, SIGNAL(progress(int)), prog, SLOT(setValue(int)));
    Connect(prog, SIGNAL(canceled()), this, SLOT(cancel()));
}

Sha1VerifyTask::~Sha1VerifyTask()
{
    pleaseStop = true;
    if (!isFinished()) wait();
    delete prog, prog = 0;
}

void Sha1VerifyTask::cancel()
{
    pleaseStop = true;
//    emit canceled();
}

Sha1Verifier::Sha1Verifier(const QString & dataFileName, const Params & params)   ///< c'tor just initializes values to 0
: dataFileName(dataFileName), pleaseStop(false), params(params)
{
    dataFileNameShort = QFileInfo(dataFileName).fileName();
}

Sha1Verifier::Result
Sha1Verifier::verify()
{
    QByteArray buf(65536, 0);
    QFile f(dataFileName);
    QFileInfo fi(dataFileName);
    SHA1 sha1;
    QString sha1FromMeta = params["sha1"].toString().trimmed();
    
    if (sha1FromMeta.isNull() || !sha1FromMeta.length()) {
        extendedError = "Meta file for " + dataFileNameShort + " does not appear to contain a saved sha1 sum!";    
        return Failure;
    }
    
    if (!f.open(QIODevice::ReadOnly)) {
        extendedError = dataFileNameShort + " could not be opened for reading!";
        return Failure;
    }
    
    qint64 size = fi.size(), read = 0, step = size/100, lastPct = -1;
    if (size != params["fileSizeBytes"].toLongLong()) {
        Warning() << dataFileNameShort << " file size on disk: `" << size << "' does not match file size saved in meta file: `" << params["fileSizeBytes"].toString() << "' !!";
    }
    if (!step) step = 1;
    extendedError = QString::null;
    while (!pleaseStop && !f.atEnd() && extendedError.isNull()) {
        qint64 ret = f.read(buf.data(), buf.size());
        if (ret < 0) {
            extendedError = f.errorString();
        } else if (ret > 0) {
            sha1.UpdateHash((uint8_t *)buf.data(), ret);
            read += ret;
            qint64 pct = read/step;
            if (pct > lastPct) {
                progress(pct);
                lastPct = pct;            
            }
        } else if (ret == 0 && f.atEnd()) {
            break;
        }
    }
    Result r = Failure;
    if (!pleaseStop && f.atEnd() && extendedError.isNull()) {
        progress(100);
        sha1.Final();
        if (sha1FromMeta.compare(sha1.ReportHash().c_str(), Qt::CaseInsensitive) == 0) {
            r = Success;
        } else {
            extendedError = "Computed SHA1 does not match saved hash in meta file!\n(This could mean the data file has some corruption!)";
            r = Failure;
        }
    } else if (pleaseStop)
        r = Canceled;
    return r;
}

void Sha1VerifyTask::run()
{
    Result res = verify();
    switch(res) {
        case Success: emit success(); break;
        case Failure: emit failure(); break;
        case Canceled: emit canceled(); break;
    }
}
