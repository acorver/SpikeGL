#ifndef Par2Window_H
#define Par2Window_H
#include <QWidget>
#include "ui_Par2Window.h"
#include <QProcess>
class QCloseEvent;

class Par2Window : public QWidget
{
    Q_OBJECT
public:
    Par2Window(QWidget *parent = 0);
    ~Par2Window();

    /// manually force an operation to start -- simulates a user picking an 
    /// operation and file from GUI
    /// @return an error string on error, or QString::null on successful start.
    QString startOperation(const QString & command, /**< one of "c", "v", or "r" */
                           const QString & file);
    
signals:
    /// emitted by this object when the par2 subprocess sent some lines.  Useful for the network command server.
    void gotLines(const QString &);
    void subprocessError(const QString &);
    void subprocessEnded();
    
    /// emitted when the window closes so that we can remove it from the app Window menu
	void closed();
	
protected:
    void closeEvent(QCloseEvent *);

protected slots:
    void browseButClicked();
    void radioButtonsClicked();
    void goButClicked();
    void forceCancelButClicked();
    void readyOutput();
    void procStarted();
    void procFinished(int,QProcess::ExitStatus);
    void procError(QProcess::ProcessError);
private:
    void killProc();
    bool go(QString & errTitle, QString & errMsg);
    
    enum Op {
        Unknown, Verify, Create, Repair
    } op;

    Ui::Par2Window *gui;
    QProcess *process;
};

#endif
