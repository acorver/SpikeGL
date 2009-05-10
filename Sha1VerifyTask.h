#ifndef Sha1VerifyTask_H
#define Sha1VerifyTask_H
#include <QThread>
#include "Params.h"

class QProgressDialog;

class Sha1VerifyTask : public QThread
{
    Q_OBJECT
public:
    Sha1VerifyTask(const QString & dataFileName, const Params & params,
                   QObject *parent);
    ~Sha1VerifyTask();

    QProgressDialog *prog;
    QString dataFileName, dataFileNameShort, extendedError;
    Params params;

signals:    
    void success();
    void failure();
    void progress(int);
    void canceled();
public slots:
    void cancel();

protected:
    void run(); 
private:
    volatile bool pleaseStop; 
};

#endif
