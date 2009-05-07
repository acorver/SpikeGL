#include "ConfigureDialogController.h"
#include "LeoDAQGL.h"
#include "Util.h"
#include "MainApp.h"
#include <QTimeEdit>
#include <QTime>
#include <QMessageBox>
#include <QSet>
#include <QSettings>
#include <QFileInfo>
#include <QFileDialog>
#include "Icon-Config.xpm"

ConfigureDialogController::ConfigureDialogController(QObject *parent)
    : QObject(parent)    
{
    dialogW = new QDialog(0);
    dialog = new Ui::ConfigureDialog;
    acqPdParams = new Ui::AcqPDParams;
    acqTimedParams = new Ui::AcqTimedParams;
    aiDevRanges = DAQ::ProbeAllAIRanges();
    aoDevRanges = DAQ::ProbeAllAORanges();

    dialog->setupUi((QDialog *)dialogW);
    acqPdParamsW = new QWidget(dialog->acqFrame);
    acqTimedParamsW = new QWidget(dialog->acqFrame);
    dialogW->setWindowIcon(QIcon(QPixmap(Icon_Config_xpm)));
    acqPdParams->setupUi(acqPdParamsW);
    acqTimedParams->setupUi(acqTimedParamsW);
    
    // connect signal slots
    Connect(dialog->acqStartEndCB, SIGNAL(activated(int)), this, SLOT(acqStartEndCBChanged()));
    Connect(dialog->acqModeCB, SIGNAL(activated(int)), this, SLOT(acqModeCBChanged()));
    Connect(dialog->deviceCB, SIGNAL(activated(const QString &)), this, SLOT(deviceCBChanged()));
    Connect(dialog->browseBut, SIGNAL(clicked()), this, SLOT(browseButClicked()));

}

ConfigureDialogController::~ConfigureDialogController()
{
    delete acqPdParams;
    delete acqTimedParams;
    delete dialog;
    delete acqPdParamsW;
    delete acqTimedParamsW;
    delete dialogW;
}

void ConfigureDialogController::resetFromParams()
{
    loadSettings();
    DAQ::Params & p (acceptedParams); // this just got populated from settings

    // initialize the dialogs with some values from settings
    acqTimedParams->acqStartTE->setTime(QTime::currentTime());
    acqTimedParams->acqEndTE->setTime(QTime::currentTime().addSecs(60*60));
    dialog->outputFileLE->setText(p.outputFile);
    dialog->acqModeCB->setCurrentIndex((int)p.mode);
    dialog->aoPassthruLE->setText(p.aoPassthruString);
    dialog->deviceCB->clear();
    dialog->clockCB->setCurrentIndex(p.extClock ? 0 : 1);
    dialog->srateSB->setValue(p.srate);
    dialog->aoPassthruGB->setChecked(p.aoPassthru);
    dialog->doCtlCB->setCurrentIndex(p.doCtlChan);
    dialog->channelListLE->setText(p.aiString);
    QList<QString> devs = aiDevRanges.uniqueKeys();
    devNames.clear();
    int sel = 0, i = 0;
    for (QList<QString>::const_iterator it = devs.begin(); it != devs.end(); ++it, ++i) {
        if (p.dev == *it) sel = i;
        dialog->deviceCB->addItem(QString("%1 (%2)").arg(*it).arg(DAQ::GetProductName(*it)));
        devNames.push_back(*it);
    }
    dialog->deviceCB->setCurrentIndex(sel);
    dialog->disableGraphsChk->setChecked(p.suppressGraphs);

    // fire off the slots to polish?
    acqStartEndCBChanged();
    acqModeCBChanged();
    deviceCBChanged();
}

void ConfigureDialogController::acqStartEndCBChanged()
{
    //acqPdParamsW->setParent(0);
    //acqTimedParamsW->setParent(0);
    acqPdParamsW->hide();
    acqTimedParamsW->hide();
    int idx = dialog->acqStartEndCB->currentIndex();

    switch (idx) {
    case 1:
    case 2:
        acqPdParamsW->setParent(dialog->acqFrame);
        acqPdParamsW->show();
        break;
    case 3:
        acqTimedParamsW->setParent(dialog->acqFrame);
        acqTimedParamsW->show();
        break;
    }
}

void ConfigureDialogController::acqModeCBChanged()
{
    int idx = dialog->acqModeCB->currentIndex();
    bool enabled = idx == 1;
    bool intan = !enabled;

    dialog->srateLabel->setEnabled(enabled);
    dialog->srateSB->setEnabled(enabled);
    dialog->clockCB->setEnabled(enabled);

    if (intan) {
        dialog->srateSB->setValue(INTAN_SRATE);    
        dialog->clockCB->setCurrentIndex(0);
    }
    dialog->doCtlLabel->setEnabled(intan);
    dialog->doCtlCB->setEnabled(intan);
}

void ConfigureDialogController::deviceCBChanged()
{
    QString devStr = devNames[dialog->deviceCB->currentIndex()];
    QList<DAQ::Range> ranges = aiDevRanges.values(devStr);
    QString curr = dialog->aiRangeCB->currentText();

    // do control combo box setup..
    int doCtl = dialog->doCtlCB->currentIndex();
    QStringList DOS = DAQ::GetDOChans(devStr);
    if (doCtl < 0) doCtl = 0;
    dialog->doCtlCB->clear();
    
    for (int i = 0; i < DOS.count(); ++i) {
        dialog->doCtlCB->addItem(DOS.at(i).section('/',1,-1));
    }
    dialog->doCtlCB->setCurrentIndex(doCtl);

    if (!curr.length()) {
        curr = QString("%1 - %2").arg(acceptedParams.range.min).arg(acceptedParams.range.max);
    }
    int sel = 0, i = 0;
    dialog->aiRangeCB->clear();
    for (QList<DAQ::Range>::const_iterator it = ranges.begin(); it != ranges.end(); ++it, ++i) {
        const DAQ::Range & r (*it);
        QString txt = QString("%1 - %2").arg(r.min).arg(r.max);
        if (txt == curr) sel = i;
        dialog->aiRangeCB->insertItem(i, txt);
    }
    dialog->aiRangeCB->setCurrentIndex(sel);

    dialog->srateSB->setMinimum((unsigned)DAQ::MinimumSampleRate(devStr));
    dialog->srateSB->setMaximum((unsigned)DAQ::MaximumSampleRate(devStr));
}

void ConfigureDialogController::browseButClicked()
{
    QString fn = QFileDialog::getSaveFileName(dialogW, "Select output file", dialog->outputFileLE->text());
    if (fn.length()) dialog->outputFileLE->setText(fn);
}

int ConfigureDialogController::exec()
{
    bool again;
    int ret;

    resetFromParams();

    do {
        ret = ((QDialog *)dialogW)->exec();
        again = false;

        // TODO process/validate form here... reject on invalid params!
    
        if (ret == QDialog::Accepted) {
            QString dev = devNames[dialog->deviceCB->currentIndex()];
            bool err;
            QVector<unsigned> chanVect;
            QString chans = parseAIChanString(dialog->channelListLE->text(), chanVect, &err);
            if (err) {
                QMessageBox::critical(dialogW, "AI Chan List Error", "Error parsing AI channel list!\nSpecify a string of the form 0,1,2,3 or 0-3,5,6 etc!");
                again = true;
                continue;
            }
            DAQ::Mode acqMode = (DAQ::Mode)dialog->acqModeCB->currentIndex();

            if ( (acqMode == DAQ::AI60Demux || acqMode == DAQ::AI120Demux)
                 && !DAQ::SupportsAISimultaneousSampling(dev) ) {
                QMessageBox::critical(dialogW, "INTAN Requires Simultaneous Sampling", QString("INTAN (60/120 demux) mode requires a board that supports simultaneous sampling, and %1 does not!").arg(dev));
                again = true;
                continue;
            }

            if ( (acqMode == DAQ::AI60Demux && chanVect.size() != 4)
                 || (acqMode == DAQ::AI120Demux && chanVect.size() != 8) ) {
                QMessageBox::critical(dialogW, "AI Chan List Error", "INTAN (60/120 demux) mode requires precisely 4 or 8 channels!");
                again = true;
                continue;                
            }
            
            QMap<unsigned, unsigned> aopass;
            if (dialog->aoPassthruGB->isChecked()) {
                aopass = parseAOPassthruString(dialog->aoPassthruLE->text(), &err);
                if (err) {
                    QMessageBox::critical(dialogW, "AO Passthru Error", "Error parsing AO Passthru list, specify a string of the form AOCHAN=DEMUXED_AICHAN!");
                    again = true;
                    continue;
                }                
            }
            unsigned srate = dialog->srateSB->value();
            if (srate > DAQ::MaximumSampleRate(dev, chanVect.size())) {
                QMessageBox::critical(dialogW, "Sampling Rate Invalid", QString().sprintf("The sample rate specified (%d) is too high for the number of channels (%d)!", (int)srate, (int)chanVect.size()));
                again = true;
                continue;
            }

            DAQ::Params & p (acceptedParams);
            p.outputFile = dialog->outputFileLE->text();
            p.dev = dev;
            QStringList rngs = dialog->aiRangeCB->currentText().split(" - ");
            if (rngs.count() != 2) {
                Error() << "INTERNAL ERROR: AI Range ComboBox needs numbers of the form REAL - REAL!";
                return QDialog::Rejected;
            }
            p.range.min = rngs.first().toDouble();
            p.range.max = rngs.last().toDouble();
            p.mode = acqMode;
            p.srate = srate;
            p.extClock = dialog->clockCB->currentIndex() == 0;
            p.aiChannels = chanVect;
            p.nVAIChans = chanVect.size();
            bool isMux = p.mode == DAQ::AI60Demux || p.mode == DAQ::AI120Demux;
            if (isMux) p.nVAIChans *= MUX_CHANS_PER_PHYS_CHAN;
            p.aiString = chans;
            p.doCtlChan = dialog->doCtlCB->currentIndex();
            p.doCtlChanString = QString("%1/%2").arg(p.dev).arg(dialog->doCtlCB->currentText());

            p.aoPassthru = dialog->aoPassthruGB->isChecked();
            p.aoPassthruMap = aopass;
            p.aoPassthruString = dialog->aoPassthruLE->text();
            p.suppressGraphs = dialog->disableGraphsChk->isChecked();

            saveSettings();
        }
    } while (again);

    return ret;
}

/*static*/ 
QString
ConfigureDialogController::parseAIChanString(const QString & str, 
                                             QVector<unsigned> & aiChannels,
                                             bool *parse_error) 
{
    QString ret("");
    QVector<unsigned> aiChans;
    QSet<unsigned> aiSet;

    QStringList vals = str.split(",", QString::SkipEmptyParts);
    for (QStringList::iterator it = vals.begin(); it != vals.end(); ++it) {
        QString v = (*it).trimmed();
        QStringList ranges = v.split(":", QString::SkipEmptyParts);
        if (ranges.count() == 2) {
            bool ok1, ok2;
            unsigned f = ranges.first().toUInt(&ok1), l = ranges.last().toUInt(&ok2);
            if (!ok1 || !ok2 || f >= l) {
                if (parse_error) *parse_error = true;
                return QString();                
            }
            for (unsigned i = f; i <= l; ++i) {
                if (aiSet.contains(i)) { // parse error! dupe chan!
                    Error() << "parseAIChanString: duplicate channel in channel list: " << i << "!";
                    if (parse_error) *parse_error = true;
                    return QString();                                    
                }
                aiChans.push_back(i);
                aiSet.insert(i);
            }

            ret.append(QString().sprintf("%s%d:%d", (ret.length()?",":""), f, l));
        } else if (ranges.count() == 1) {
            bool ok;
            unsigned i = ranges.first().toUInt(&ok);
            if (!ok) {
                if (parse_error) *parse_error = true;
                return QString();                
            }
            if (aiSet.contains(i)) { // parse error! dupe chan!
                Error() << "parseAIChanString: duplicate channel in channel list: " << i << "!";
                if (parse_error) *parse_error = true;
                return QString();                                    
            }
            aiChans.push_back(i);
            aiSet.insert(i);
            ret.append(QString().sprintf("%s%d", (ret.length()?",":""), i));
        } else {
            if (parse_error) *parse_error = true;
            return QString();                
        }
    }
    if (!ret.length() && parse_error) *parse_error = true;
    else if (parse_error) *parse_error = false;
    aiChannels = aiChans;
    return ret;
}
 
/*static*/
QMap<unsigned,unsigned> 
ConfigureDialogController::parseAOPassthruString(const QString & str,
                                                 bool *parse_error)
{
    QMap<unsigned,unsigned> ret;

    QStringList vals = str.split(",", QString::SkipEmptyParts);
    for (QStringList::iterator it = vals.begin(); it != vals.end(); ++it) {
        QString v = (*it).trimmed();
        QStringList nvp = v.split("=", QString::SkipEmptyParts);
        if (nvp.count() == 2) {
            bool ok1, ok2;
            unsigned n = nvp.first().toUInt(&ok1), v = nvp.last().toUInt(&ok2);
            if (!ok1 || !ok2) {
                if (parse_error) *parse_error = true;
                return QMap<unsigned,unsigned>();                
            }
            ret.insert(n,v);
        } else {
            if (parse_error) *parse_error = true;
            return QMap<unsigned,unsigned>();
        }
    }
    if (ret.empty() && parse_error) *parse_error = true;
    else if (parse_error) *parse_error = false;
    return ret;
}

void ConfigureDialogController::loadSettings()
{
    DAQ::Params & p(acceptedParams);

    QSettings settings("janelia.hhmi.org", "LeoDAQGL");

    settings.beginGroup("ConfigureDialogController");

    p.outputFile = mainApp()->outputDirectory() + PATH_SEPARATOR + settings.value("lastOutFile", "data.bin").toString();
    p.dev = settings.value("dev", "").toString();
    p.doCtlChan = settings.value("doCtlChan", "0").toUInt();
    p.range.min = settings.value("rangeMin", "-2.5").toDouble();
    p.range.max = settings.value("rangeMax", "2.5").toDouble();
    p.mode = (DAQ::Mode)settings.value("acqMode", 0).toInt();
    p.srate = settings.value("srate", INTAN_SRATE).toUInt();
    p.extClock = settings.value("extClock", true).toBool();
    p.aiString = settings.value("aiString", "0:3").toString();
    p.aoPassthru = settings.value("aoPassthru", false).toBool();
    p.aoPassthruString = settings.value("aoPassthruString", "0=1,1=2").toString();
    p.suppressGraphs = settings.value("suppressGraphs", false).toBool();
}

void ConfigureDialogController::saveSettings()
{
    DAQ::Params & p(acceptedParams);

    QSettings settings("janelia.hhmi.org", "LeoDAQGL");

    settings.beginGroup("ConfigureDialogController");
    
    settings.setValue("lastOutFile", QFileInfo(p.outputFile).fileName());
    QString path = QFileInfo(p.outputFile).path();
    mainApp()->setOutputDirectory(path.length() ? path : QString(PATH_SEPARATOR));
    settings.setValue("dev", p.dev);
    settings.setValue("rangeMin", p.range.min);
    settings.setValue("rangeMax", p.range.max);
    settings.setValue("acqMode", (int)p.mode);
    settings.setValue("doCtlChan", p.doCtlChan);
    settings.setValue("srate", p.srate);
    settings.setValue("extClock", p.extClock);
    settings.setValue("aiString", p.aiString);
    settings.setValue("aoPassthru", p.aoPassthru);
    settings.value("aoPassthruString", p.aoPassthruString);
    settings.setValue("suppressGraphs", p.suppressGraphs);    
}
