#ifndef DEVICESETTINGS_H
#define DEVICESETTINGS_H

#include <QDialog>

namespace Ui {
class DeviceSettings;
}

class DeviceSettings : public QDialog
{
    Q_OBJECT

public:
    explicit DeviceSettings(QWidget *parent = 0);
    ~DeviceSettings();

private slots:
    void on_pushButton_clicked();

    void on_devSetIndividualGains_clicked();

private:
    Ui::DeviceSettings *ui;
    QStringList parseInfo(QStringList list, QString infoString, QChar separator = ',');
    QList<QWidget*> availableAmplificationElements;
    void loadSettings(bool savedValuesOnly);
protected:
    void accept();
};

#endif // DEVICESETTINGS_H
