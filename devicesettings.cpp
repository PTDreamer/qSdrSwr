#include "devicesettings.h"
#include "ui_devicesettings.h"
#include <QProcess>
#include <QDebug>
#include <QCloseEvent>
#include <QSettings>
#include <QMessageBox>

DeviceSettings::DeviceSettings(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DeviceSettings)
{
    ui->setupUi(this);
    ui->devMinimumFreq->setReadOnly(true);
    ui->devMaximumFreq->setReadOnly(true);
    loadSettings(true);
}

DeviceSettings::~DeviceSettings()
{
    delete ui;
}

void DeviceSettings::on_pushButton_clicked()
{
    qDeleteAll(availableAmplificationElements);
    availableAmplificationElements.clear();
    foreach (QObject *w, this->children()) {
        QComboBox *cb = qobject_cast<QComboBox*>(w);
        if(cb)
            cb->clear();
    }
    QSettings settings;
    QString program = settings.value("soapy_power").toString();
    QStringList arguments;
    arguments << "--info";
    QProcess myProcess(this);
    myProcess.start(program, arguments);
    bool res =myProcess.waitForFinished();
    if(!res) {
        QMessageBox::critical(this, "Soapy Power executable not set or wrong version found", "Please set Soapy Power executable location");
        return;
    }
    QByteArray output = myProcess.readAllStandardOutput();
    QString outputStr(output);
    QStringList outputStringList = outputStr.split("\n");
    QRegExp rx("*Available RX channels*");
    rx.setPatternSyntax(QRegExp::Wildcard);
    QString val = outputStringList.at(outputStringList.indexOf(rx) + 1);
    val.remove(" ");
    ui->devChannel->addItems(parseInfo(outputStringList,"Available RX channels"));
    ui->devSampleRate->addItems(parseInfo(outputStringList, "Allowed sample rates"));
    for(int x = 0; x < ui->devSampleRate->count(); ++x) {
        ui->devSampleRate->setItemText(x,ui->devSampleRate->itemText(x) + "M");
    }
    ui->devAntenna->addItems(parseInfo(outputStringList, "Available antennas"));
    ui->devBandwidth_CB->addItems(parseInfo(outputStringList, "Allowed bandwidths"));
    QStringList totalGain = parseInfo(outputStringList, "Allowed gain range", '-');
    if(totalGain.length() == 2) {
        ui->devTotalGain->setMinimum(((QString)(totalGain.at(0))).toDouble());
        ui->devTotalGain->setMaximum(((QString)(totalGain.at(1))).toDouble());
    }
    QStringList ampElements = parseInfo(outputStringList, "Available amplification");
    foreach (QString str, ampElements) {
        QLabel *l = new QLabel(this);
        l->setText(QString("%1 gain").arg(str));
        QDoubleSpinBox *b = new QDoubleSpinBox(this);
        b->setProperty("cli", str);
        ui->formLayout->insertRow(7, l, b);
        availableAmplificationElements.append(l);
        availableAmplificationElements.append(b);
    }
    on_devSetIndividualGains_clicked();
    QStringList freqs = parseInfo(outputStringList, "Allowed frequency range", '-');
    if(freqs.length() == 2) {
        ui->devMinimumFreq->setText(freqs.at(0));
        ui->devMaximumFreq->setText(freqs.at(1));
    }
    loadSettings(false);
}

QStringList DeviceSettings::parseInfo(QStringList list, QString infoString, QChar separator)
{
    QRegExp rx(QString("*%1*").arg(infoString));
    rx.setPatternSyntax(QRegExp::Wildcard);
    QString val = list.at(list.indexOf(rx) + 1);
    val.remove(" ");
    return val.split(separator);
}

void DeviceSettings::loadSettings(bool savedValuesOnly)
{
    QSettings settings;
    foreach (QObject *w, this->children()) {
        QComboBox *cb = qobject_cast<QComboBox*>(w);
        QSpinBox *sb = qobject_cast<QSpinBox*>(w);
        QDoubleSpinBox *dsb = qobject_cast<QDoubleSpinBox*>(w);
        QCheckBox *chb = qobject_cast<QCheckBox*>(w);
        QLineEdit *le = qobject_cast<QLineEdit*>(w);
        QString setting = QString("deviceSettings/%1").arg(w->objectName());
        if(!settings.allKeys().contains(setting))
            continue;
        if(cb)
            if(savedValuesOnly) {
                if(settings.allKeys().contains(setting))
                    cb->addItem(settings.value(setting).toString());
            }
            else {
                cb->setCurrentText(settings.value(setting).toString());
            }
        else if(sb)
            sb->setValue(settings.value(setting).toInt());
        else if(dsb)
            dsb->setValue(settings.value(setting).toDouble());
        else if(chb)
            chb->setChecked(settings.value(setting).toBool());
        else if((le && !le->isReadOnly()) || (le && le->isReadOnly() && savedValuesOnly))
            le->setText(settings.value(setting).toString());
    }
}

void DeviceSettings::accept()
{
    QSettings settings;
    QString settingsString;
    foreach (QObject *w, this->children()) {
        QComboBox *cb = qobject_cast<QComboBox*>(w);
        QSpinBox *sb = qobject_cast<QSpinBox*>(w);
        QDoubleSpinBox *dsb = qobject_cast<QDoubleSpinBox*>(w);
        QCheckBox *chb = qobject_cast<QCheckBox*>(w);
        QLineEdit *le = qobject_cast<QLineEdit*>(w);
        QString setting = QString("deviceSettings/%1").arg(w->objectName());
        if(cb) {
            settings.setValue(setting, cb->currentText());
            if(cb->property("cli").isValid() && cb->isVisible() && !cb->currentText().isEmpty()) {
                if(cb->currentText().compare("N/A"))
                    settingsString.append(" ").append(cb->property("cli").toString().append(" ").append(cb->currentText()));
            }
        }
        else if(sb) {
            settings.setValue(setting, sb->value());
            if(sb->property("cli").isValid() && sb->isVisible()) {
                settingsString.append(" ").append(sb->property("cli").toString().append(" ").append(QString::number(sb->value())));
            }
        }
        else if(dsb) {
            settings.setValue(setting, dsb->value());
            if(dsb->property("cli").isValid() && dsb->isVisible()) {
                settingsString.append(" ").append(dsb->property("cli").toString().append(" ").append(QString::number(dsb->value())));
            }
        }
        else if(chb) {
            settings.setValue(setting, chb->isChecked());
            if(chb->property("cli").isValid() && chb->isVisible() && chb->isChecked()) {
                settingsString.append(" ").append(chb->property("cli").toString());
            }
        }
        else if(le) {
            settings.setValue(setting, le->text());
            if(le->property("cli").isValid() && le->isVisible() && !le->text().isEmpty()) {
                settingsString.append(" ").append(chb->property("cli").toString());
            }
        }
    }
    if(ui->devSetIndividualGains->isChecked()) {
        settingsString.append(" ").append("-G");
        foreach (QWidget *w, availableAmplificationElements) {
            QDoubleSpinBox *d = qobject_cast<QDoubleSpinBox*>(w);
            if(d && d->property("cli").isValid()) {
                settingsString.append(" ").append(d->property("cli").toString()).append("=").append(QString::number(d->value()));
            }
        }
    }
    settings.setValue("deviceSettings/settingsString", settingsString);
    settings.setValue("deviceSettings/minFrequency", ui->devMinimumFreq->text());
    settings.setValue("deviceSettings/maxFrequency", ui->devMaximumFreq->text());
    QDialog::accept();
}

void DeviceSettings::on_devSetIndividualGains_clicked()
{
    bool state = ui->devSetIndividualGains->isChecked();
    ui->devTotalGain->setVisible(!state);
    ui->devTotalGainLabel->setVisible(!state);
    foreach (QWidget *w, availableAmplificationElements) {
        w->setVisible(state);
    }
}
