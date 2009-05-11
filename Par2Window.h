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

    enum Op {
        Unknown, Verify, Create, Repair
    } op;

    Ui::Par2Window *gui;
    QProcess *process;
};

#endif
