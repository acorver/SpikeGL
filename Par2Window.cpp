#include "Par2Window.h"
#include "Util.h"
#include <QFileDialog>
#include <QMessageBox>
#include "Params.h"
#include <QCloseEvent>
#include <QTextEdit>
#include "MainApp.h"
#include <QDir>
#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <QFile>
#endif
#include <QSettings>

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

Par2Window::~Par2Window()
{
    killProc();
}

static QString getLastFileName(int op, const QString & f)
{
    QSettings settings(SETTINGS_DOMAIN, SETTINGS_APP);
    settings.beginGroup("Par2Window");
	return settings.value(QString("lastOpenFileName_") + QString::number(op), f).toString();
}
static void saveLastFileName(int op, const QString & f)
{
	if (f.isNull() || f.isEmpty()) return;
    QSettings settings(SETTINGS_DOMAIN, SETTINGS_APP);
    settings.beginGroup("Par2Window");
	settings.setValue(QString("lastOpenFileName_") + QString::number(op), f);
}

void Par2Window::browseButClicked()
{
    QString f = getLastFileName(op, mainApp()->outputDirectory()); 	// get last file name form Settings, if any
    if (op == Create) {
        f = QFileDialog::getOpenFileName(this, "Select a data file for PAR2 operation", f);
    } else {
        f = QFileDialog::getOpenFileName(this, "Select a PAR2 file for verify", f, "PAR2 Files (*.par2 *.PAR2)");
    }	
	if (f.isNull() || f.isEmpty()) return;
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
	// set last file, save
	saveLastFileName(op, f);
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
    gui->goBut->setEnabled(true);
#ifdef Q_OS_WIN
    QFile::remove(QDir::tempPath() + "/par2.exe");
#endif
    
    emit subprocessEnded();
}

void Par2Window::forceCancelButClicked()
{
    Debug() << "Process cancel.";
    killProc();
}

void Par2Window::procStarted()
{
    if (process) {
#ifdef Q_OS_WIN
        int pid = process->pid() ? process->pid()->dwProcessId : 0;
#else
        int pid = process->pid();
#endif
        Debug() << "Process " << pid << " started.";
    } else {
        Error() << "procStarted with no process!";
    }
    gui->forceCancelBut->setEnabled(true);
    gui->goBut->setEnabled(false);
}

void Par2Window::procFinished(int exitCode,QProcess::ExitStatus status)
{
#ifdef Q_OS_WIN
    int pid = process && process->pid() ? process->pid()->dwProcessId : 0;
#else
    int pid = process ? process->pid() : 0;
#endif
    Debug() << "Process " << (process?QString::number(pid):"") << " finished code:" << exitCode << " status: " << (status == QProcess::CrashExit ? "(CRASH!)" : "(Normal)");
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
#ifdef Q_OS_WIN
    int pid = process && process->pid() ? process->pid()->dwProcessId : 0;
#else
    int pid = process ? process->pid() : 0;
#endif   
    Error() << "Process " << (process ? QString::number(pid) : "") << " error: " << s;
    
    emit subprocessError(s);
    
    killProc();
}


void Par2Window::goButClicked()
{
    QString errTitle, errMsg;
    if ( !go( errTitle, errMsg) && !errMsg.isNull() )
        QMessageBox::critical(this, errTitle, errMsg);
}

bool Par2Window::go(QString & errTitle, QString & errMsg) 
{
    QString f = gui->fileLE->text().trimmed();
    if ((f.endsWith(".par2") || !QFileInfo(f).exists()) && op == Create) {
        errTitle = "Specify a file", errMsg = "Please specify a data file for which to create a PAR2 volume set.";
        return false;
    }
    if (!QFileInfo(f).exists()) {
        errTitle = "Specify a file", errMsg = "Please specify a file that exists!";
        return false;
    }
    if (process) {
        Error() << "Par2Window goButClicked() already has a process!";
        return false;
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
        if (fi.suffix().compare(".par2", Qt::CaseInsensitive) == 0) {
            files = fi.path() + "/" + fi.baseName() + ".bin";
            gui->fileLE->setText(files);
            files = QString("\"") + files + "\"";
        }
        opStr += QString(" -r%1").arg(gui->redundancySB->value());
        QString par2 = fi.path() + "/" + fi.fileName() + ".par2";
        files = QString("\"") + par2 + "\" \"" + files + "\"";
    } else if (!files.endsWith(".par2")) {
        QFileInfo fi(files.trimmed());
        files = fi.path() + "/" + fi.fileName() + ".par2";
        gui->fileLE->setText(files);
        files = QString("\"") + files + "\"";
    } else {
        files = QString("\"") + files + "\"";
    }
    QString par2Prog = "par2";
#ifdef Q_OS_WIN
    par2Prog = QDir::tempPath() + "/par2.exe";
    {
        QFile f(par2Prog), f_in(":/par2.exe");        
        if (f.open( QIODevice::WriteOnly | QIODevice::Truncate )
            && f_in.open(QIODevice::ReadOnly)) {
            char buf[16384];
            qint64 ret, nCopied = 0;
            while ((ret = f_in.read(buf, sizeof(buf))) > 0) {
                f.write(buf, ret);
                nCopied += ret;
            }
            Debug() << "Copied " << nCopied << " bytes of resource par2.exe to filesystem at " << f.fileName();
            f_in.close();
            f.close();
        }
    }
#endif
    QString cmd = QString("\"%1\" %2 %3").arg(par2Prog).arg(opStr).arg(files);
    Debug() << "Executing: " << cmd;
    process->start(cmd);
    gui->goBut->setEnabled(false);
    gui->forceCancelBut->setEnabled(true);
    return true;
}

void Par2Window::readyOutput()
{
    if (!process) return;
    QTextEdit *te = gui->outputTE;
    te->moveCursor(QTextCursor::End);
    QString out = process->readAllStandardOutput();

    /*QString tmp = out;
    tmp.replace("\r", "|r");
    tmp.replace("\n", "|n");
    Debug() << "process sent: " << tmp;*/
    
    out.replace("\r\n", "\n");
        
    QStringList lines = out.split("\n", QString::KeepEmptyParts);
    for (QStringList::iterator it = lines.begin(); it != lines.end(); ++it) {
        QStringList strs = (*it).split("\r", QString::SkipEmptyParts);
        const bool rem = strs.count() > 1 || (*it).startsWith("\r") || (*it).endsWith("\r");
        for (QStringList::iterator it2 = strs.begin(); it2 != strs.end(); ++it2) {
            if (rem) {
                te->moveCursor(QTextCursor::StartOfLine, QTextCursor::KeepAnchor);
                te->textCursor().removeSelectedText();
                te->textCursor().deletePreviousChar();
            }
            te->append(*it2);
            te->moveCursor(QTextCursor::End);    

        }        
    }
    
    emit gotLines(out);    
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
	
	if (e->isAccepted()) 
		emit closed();
}

/// manually force an operation to start -- simulates a user picking an 
/// operation and file from GUI
/// @return an error string on error, or QString::null on successful start.
QString Par2Window::startOperation(const QString & command, /**< one of "c", "v", or "r" */
                                   const QString & file)
{
    if (command == "c") op = Create;
    else if (command == "v") op = Verify;
    else if (command == "r") op = Repair;
    else {
        return "Invalid operation, operation needs to be one of 'r','c','v'.";
    }
    gui->fileLE->setText(file);
    QString errTitle, errMsg;
    if (! go(errTitle, errMsg) ) {
        return errTitle + " " + errMsg; 
    }
    return QString::null; // no error
}