#ifndef SOAPYPOWER_H
#define SOAPYPOWER_H

#include <QObject>
#include <QProcess>
#include <QVector>

class SoapyPower : public QObject
{
    Q_OBJECT
public:
    explicit SoapyPower(QString soapyExecutable, QObject *parent = 0);
    bool start(QStringList parameters, QString deviceSettingsString, double starFreqMhz, double stopFreqMhz, bool continuous);
    void stop();
    QString getSoapyExec() const;
    void setSoapyExec(const QString &value);
    typedef struct  __attribute__((packed, aligned(2))) soapy_bin {
        char magic[5];
        unsigned char version;
        double time_start;
        double time_stop;
        double start;
        double stop;
        double step;
        unsigned long long samples;
        unsigned long long array_size;
        char pad[2];
    } soapy_bin_t;

    typedef struct power_data {
        soapy_bin_t meta;
        QVector<float> data;
        quint64 index;
        bool firstRun;
    } power_data_t;
signals:
    void dataReceived(SoapyPower::power_data_t);
    void currentHopFinished(quint16 hopNumber, quint16 hopTotal);
    void currentScanFinished();
    void finished(int);

public slots:

    void processDebug();
    void processError();
    void processExitError(QProcess::ProcessError error);
    void soapy_finished(int code);
private:
    void asyncRead();
    QString soapyExec;// TODO SET WHEN CHANGED SETTINGS
    QProcess soapyProcess;
    int fd[2];
    bool closePipeRead;
    quint16 hops;
};

#endif // SOAPYPOWER_H
