#include <QTextEdit>
#include <QMenuBar>
#include <QApplication>
#include <QStatusBar>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QMessageBox>
#include "ConsoleWindow.h"
#include "MainApp.h"
#include "Util.h"

ConsoleWindow::ConsoleWindow(QWidget *p, Qt::WindowFlags f)
    : QMainWindow(p, f)
{
    QTextEdit *te = new QTextEdit(this);
    te->setUndoRedoEnabled(false);
    te->setReadOnly(true);
    setCentralWidget(te);
    QMenuBar *mb = menuBar();
    QMenu *m = mb->addMenu("&File");
    MainApp *app = mainApp();
    m->addAction(app->newAcqAct);
    m->addAction(app->stopAcq);
    m->addSeparator();
    m->addAction(app->quitAct);    
    statusBar()->showMessage("");

    m = mb->addMenu("&Options");
    m->addAction(app->toggleDebugAct);
    m->addAction(app->chooseOutputDirAct);
    m->addAction(app->hideUnhideConsoleAct);
    m->addAction(app->hideUnhideGraphsAct);
    m->addAction(app->stimGLIntOptionsAct);
    m->addAction(app->aoPassthruAct);
	m->addAction(app->commandServerOptionsAct);
	m->addAction(app->showChannelSaveCBAct);
    m = mb->addMenu("&Tools");
    m->addAction(app->verifySha1Act);
    m->addAction(app->par2Act);
    m = mb ->addMenu("&Help");
    m->addAction(app->helpAct);
    m->addSeparator();
    m->addAction(app->aboutAct);
    m->addAction(app->aboutQtAct);

    setWindowIcon(app->appIcon);
}



QTextEdit *ConsoleWindow::textEdit() const
{
    return centralWidget() ? dynamic_cast<QTextEdit *>(centralWidget()) : 0;
}

void ConsoleWindow::closeEvent(QCloseEvent *e)
{
    mainApp()->quitAct->trigger();
    // if we got here, the quit failed..
    e->ignore();
}
