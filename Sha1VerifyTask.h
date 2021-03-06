#ifndef Sha1VerifyTask_H
#define Sha1VerifyTask_H
#include <QThread>
#include "Params.h"

class QProgressDialog;

struct Sha1Verifier {
    QString dataFileName, dataFileNameShort, extendedError; 
    QString metaFilePath;
    volatile bool pleaseStop;
    Params params;    
    
    virtual void progress(int) {} /**< no-op, reimplemented in subclasses */
    
    enum Result { Success, Failure, Canceled, MetaFileMissingSha1 };
    
    Sha1Verifier(const QString & dataFileName, const QString & completeMetaFilepath, const Params & params);   ///< c'tor just initializes values to 0
    
    Result verify(QString *hash_out = 0); ///< the meat and potatoes of all this is here -- performs the verification, calling the optional function, etc
};

class Sha1VerifyTask : public QThread, public Sha1Verifier
{
    Q_OBJECT
public:
    
    Sha1VerifyTask(const QString & dataFileName, const QString & meta, const Params & params,
                   QObject *parent);
    ~Sha1VerifyTask();

    QProgressDialog *prog;

signals:    
    void success();
    void failure();
    void progress(int);
    void canceled();
    void metaFileMissingSha1(QString computedSha1);
public slots:
    void cancel();

protected:
    void run(); 
};

#endif
