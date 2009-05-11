#include "Par2Window.h"
#include "Util.h"
#include <QFileDialog>
#include <QMessageBox>
#include "Params.h"
#include <QCloseEvent>
#include <QTextEdit>
#include "MainApp.h"

Par2Window::Par2Window(QWidget *parent)
    : QWidget(parent)
{    
    process = 0;
    gui = new Ui::Par2Window;
    gui->setupUi(this);
    op = Verify;
    gui->verifyRB->setChecked(true);
    Connect(gui->browseBut, SIGNAL(clicked()), this, SLOT(browseButClicked()));
    Connect(gui->verifyRB, SIGNAL(clicked()), this, SLOT(radioButtonsClicked()));
    Connect(gui->createRB, SIGNAL(clicked()), this, SLOT(radioButtonsClicked()));
    Connect(gui->repairRB, SIGNAL(clicked()), this, SLOT(radioButtonsClicked()));
    Connect(gui->goBut, SIGNAL(clicked()), this, SLOT(goButClicked()));
    Connect(gui->forceCancelBut, SIGNAL(clicked()), this, SLOT(forceCancelButClicked()));
    gui->forceCancelBut->setEnabled(false);
}

void Par2Window::browseButClicked()
{
    QString f;
    if (op == Create) {
        f = QFileDialog::getOpenFileName(this, "Select a data file for PAR2 operation", mainApp()->outputDirectory());
    } else {
        f = QFileDialog::getOpenFileName(this, "Select a PAR2 file for verify", mainApp()->outputDirectory(), "PAR2 Files (*.par2 *.PAR2)");
    }
    if (f.endsWith(".meta") && op == Create) {
        Params p;
        p.fromFile(f);
        if (!p["outputFile"].toString().isNull()) 
            f = p["outputFile"].toString();
        else {
            QMessageBox::critical(this, "Error parsing .meta file", "Error parsing .meta file!\nThe specified .meta file does not contain the requisite 'outputFile' key!");
            return;
        }
    }   
    gui->fileLE->setText(f);
}

void Par2Window::killProc()
{
    if (process) {
        if (process->state() != QProcess::NotRunning) {
            process->terminate();
            process->waitForFinished(150);
            process->kill();
            process->waitForFinished(150);
        }
        delete process, process = 0;
    }
    gui->specifyGB->setEnabled(true);
    gui->forceCancelBut->setEnabled(false);
}

void Par2Window::forceCancelButClicked()
{
    Debug() << "Process cancel.";
    killProc();
}

void Par2Window::procStarted()
{
    if (process) {
        Debug() << "Process " << process->pid() << " started.";
    } else {
        Error() << "procStarted with no process!";
    }
    gui->forceCancelBut->setEnabled(true);

}

void Par2Window::procFinished(int exitCode,QProcess::ExitStatus status)
{
    Debug() << "Process " << (process?QString::number(process->pid()):"") << " finished code:" << exitCode << " status: " << (status == QProcess::CrashExit ? "(CRASH!)" : "(Normal)");
    killProc();
}

void Par2Window::procError(QProcess::ProcessError err)
{
    QString s;
    switch (err) {
    case QProcess::FailedToStart: s = "The process failed to start."; break;
    case QProcess::Crashed: s = "The process crashed."; break;
    case QProcess::Timedout: s = "The process timed out."; break;
    case QProcess::WriteError: s = "Process write error."; break;
    case QProcess::ReadError: s = "Process read error."; break;
    default: s = "An unknown error occurred"; break;
    }
    Error() << "Process " << (process ? QString::number(process->pid()) : "") << " error: " << s;
    killProc();
}


void Par2Window::goButClicked()
{
    QString f = gui->fileLE->text().trimmed();
    if ((f.endsWith(".par2") || !QFileInfo(f).exists()) && op == Create) {
            QMessageBox::critical(this, "Specify a file", "Please specify a data file for which to create a PAR2 volume set.");
            return;        
    }
    if (!QFileInfo(f).exists()) {
        QMessageBox::critical(this, "Specify a file", "Please specify a file that exists!");
            return;        
    }
    if (process) {
        Error() << "Par2Window goButClicked() already has a process!";
        return;
    }
    static bool regd = false;
    if (!regd) {
        int id = qRegisterMetaType<QProcess::ExitStatus>("QProcess::ExitStatus");
        id = qRegisterMetaType<QProcess::ProcessError>("QProcess::ProcessError");
        (void)id;
        regd = true;
    }
    process = new QProcess(this);
    process->setProcessChannelMode(QProcess::MergedChannels);
    Connect(process, SIGNAL(readyReadStandardOutput()), this, SLOT(readyOutput()));
    Connect(process, SIGNAL(started()), this, SLOT(procStarted()));
    Connect(process, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(procFinished(int,QProcess::ExitStatus)));
    Connect(process, SIGNAL(error(QProcess::ProcessError)), this, SLOT(procError(QProcess::ProcessError)));
    gui->specifyGB->setEnabled(false);
    QString opStr = "c";
    if (op == Verify) opStr = "v";
    else if (op == Repair) opStr = "r";
    QString files = gui->fileLE->text();
    if (op == Create) {        
        QFileInfo fi(files.trimmed());
        if (fi.suffix() == ".par2") {
            files = fi.path() + "/" + fi.baseName() + ".bin";
            gui->fileLE->setText(files);
        }
        opStr += QString(" -r%1").arg(gui->redundancySB->value());
        QString par2 = fi.path() + "/" + fi.baseName() + ".par2";
        files = par2 + " " + files;
    } else if (!files.endsWith(".par2")) {
        QFileInfo fi(files.trimmed());
        files = fi.path() + "/" + fi.baseName() + ".par2";
        gui->fileLE->setText(files);
    }
    QString cmd = QString("par2 %1 %2").arg(opStr).arg(files);
    Debug() << "Executing: " << cmd;
    process->start(cmd);
}

void Par2Window::readyOutput()
{
    if (!process) return;
    QTextEdit *te = gui->outputTE;
    te->moveCursor(QTextCursor::End);
    QString out = process->readAllStandardOutput();
        
    // clobber lines that were supposed to be clobbered with the '\r' char..

    if (out.endsWith("\r") || out.startsWith("\r")) {
        te->moveCursor(QTextCursor::StartOfLine, QTextCursor::KeepAnchor);
        te->textCursor().removeSelectedText();
        te->textCursor().deletePreviousChar();
    }

    QStringList strs = out.split("\r", QString::SkipEmptyParts);
    out = "";
    for (QStringList::iterator it = strs.begin(); it != strs.end(); ++it) {
        out = *it;
    }

    te->append(out);
    te->moveCursor(QTextCursor::End);    
}

void Par2Window::radioButtonsClicked()
{
    QRadioButton *rb = dynamic_cast<QRadioButton *>(sender());
    if (!rb) {
        Error() << "Par2Window::radioButtonClicked() sender should be a radio button!";
        return;
    }
    bool rEn = false;
    if (rb == gui->verifyRB) {
        op = Verify;
    } else if (rb == gui->createRB) {
        op = Create;
        rEn = true;
    } else if (rb == gui->repairRB) {
        op = Repair;
    }
    gui->redundancyLbl->setEnabled(rEn);
    gui->redundancySB->setEnabled(rEn);
}

void Par2Window::closeEvent(QCloseEvent *e)
{
    if (process) {
        int but = QMessageBox::question(this, "Stop current task?", "A PAR2 task is running -- are you sure you wish to close this window?", QMessageBox::Yes, QMessageBox::No);
        if (but == QMessageBox::Yes) {
            killProc();
            e->accept();
        } else {
            e->ignore();
        }
    } else
        e->accept();
}
