#include "soapypower.h"
#include <unistd.h>
#include <fcntl.h>
#include <QProcess>
#include <QtConcurrent>
#include <QMessageBox>

SoapyPower::SoapyPower(QString soapyExecutable, QObject *parent) : QObject(parent), soapyExec(soapyExecutable)
{

    connect(&soapyProcess, SIGNAL(readyReadStandardOutput()), this, SLOT(processDebug()));
    connect(&soapyProcess, SIGNAL(readyReadStandardError()), this, SLOT(processError()));
    connect(&soapyProcess, SIGNAL(errorOccurred(QProcess::ProcessError)), this, SLOT(processExitError(QProcess::ProcessError)));
    connect(&soapyProcess, SIGNAL(finished(int)), this, SLOT(soapy_finished(int)));
    soapyProcess.setProgram(soapyExecutable);
}

bool SoapyPower::start(QStringList parameters, QString deviceSettingsString, double starFreqMhz, double stopFreqMhz, bool continuous)
{
        stop();
        if(soapyExec.isEmpty()) {
            return false;
        }
        QStringList arguments;
        arguments.append(deviceSettingsString.trimmed().split(" "));
        arguments.append(parameters);
        closePipeRead = false;
        ::pipe(fd);
        arguments << "-F" << "soapy_power_bin";
        arguments << "--output-fd" << QString::number(fd[1]);
        //arguments << "-u" << "10";
        arguments << "-f" << QString("%0M:%1M").arg(starFreqMhz).arg(stopFreqMhz);
        if(continuous)
            arguments << "-c";
        soapyProcess.setArguments(arguments);
        QtConcurrent::run(this, &SoapyPower::asyncRead);
        QString debug;
        foreach (QString s, soapyProcess.arguments()) {
            debug.append(s).append(" ");
        }
        soapyProcess.start();
        return true;
}

void SoapyPower::stop()
{
    if(soapyProcess.state() == QProcess::Running) {
        closePipeRead = true;
        soapyProcess.terminate();
    }
}

void SoapyPower::asyncRead()
{
    bool myFirstRun = true;
    double startFreq = 0;
    static char magic[] = "SDRFF";
    QByteArray bytes;
    QByteArray temp(20480,0);
    soapy_bin_t struc;
    QDateTime timeStart;
    QDateTime timeEnd;
    power_data_t d;
    static uint currentHop = 0;
    while(!closePipeRead) {
        ssize_t length = ::read(fd[0], temp.data(), temp.size());
        bytes.append(temp.left(length));
        if(bytes.indexOf(magic) > 0) {
            bytes.remove(0, bytes.indexOf(magic));
        }
        if(length < 0) {
            return;
        }
        if(length > 0) {

            if(true) {
                if(bytes.length() < (int)sizeof(soapy_bin_t))
                    continue;
                memcpy(&struc, bytes.constData(), sizeof(soapy_bin_t));
                if(memcmp(magic, struc.magic, sizeof(magic) - 1)) {
                    qCritical() << "Wrong magic received from soapy_power";
                    continue;
                }
            }
//            qDebug() << "version" << struc.version;
//            timeStart.setTime_t(struc.time_start);
//            timeEnd.setTime_t(struc.time_stop);
//            qDebug() << "time start" << timeStart.toString() << timeEnd.toString();
//            qDebug() << "samples" << struc.samples << "arraySize" << struc.array_size;
//            qDebug() << "start" << QString::number(struc.start, 'f', 5) << "stop" << QString::number(struc.stop, 'f', 5) << "step" << QString::number(struc.step, 'f', 5);
//            qDebug() << "lenght" << struc.array_size / sizeof(float);

            if((bytes.length() - sizeof(soapy_bin_t)) >= struc.array_size)
            {
                d.meta = struc;
                d.data.resize(struc.array_size / sizeof(float));
                memcpy(d.data.data(), bytes.constData() + sizeof(soapy_bin_t), struc.array_size);
                if(startFreq == 0) {
                    startFreq = struc.start;
                    d.firstRun = true;
                }
                else if(struc.start == startFreq) {
                    d.firstRun = false;
                    myFirstRun = false;
                }
                else if(!myFirstRun) {
                    d.firstRun = false;
                }

                d.index = (struc.start - startFreq) / struc.step;
                emit dataReceived(d);
                emit currentHopFinished(currentHop, hops - 1);
                if(currentHop == hops -1) {
                    emit currentScanFinished();
                    currentHop = 0;
                }
                else
                    ++currentHop;
                if((bytes.length() - sizeof(soapy_bin_t)) > struc.array_size) {
                    bytes.remove(0, sizeof(soapy_bin_t) + struc.array_size);
                }
                else {
                    bytes.clear();
                }

            }
            else {
            }
        }
    }
}

QString SoapyPower::getSoapyExec() const
{
    return soapyExec;
}

void SoapyPower::setSoapyExec(const QString &value)
{
    soapyProcess.setProgram(value);
    soapyExec = value;
}

void SoapyPower::processDebug()
{
    QByteArray array = soapyProcess.readAllStandardOutput();
    QString arrayStr(array);
    QStringList lines = arrayStr.split("\n");
    foreach (QString s, lines) {
        if(s.contains("hops")) {
            QString ss = s.split(":").at(2);
            hops = ss.toInt();
        }
    }
}

void SoapyPower::processError()
{
    QByteArray array = soapyProcess.readAllStandardError();
    QString arrayStr(array);
    QStringList lines = arrayStr.split("\n");
    foreach (QString s, lines) {
        if(s.contains("hops")) {
            QString ss = s.split(":").at(2);
            hops = ss.toInt();
        }
    }
}

void SoapyPower::processExitError(QProcess::ProcessError error)
{
    qCritical() << "soapy_power failed to start with code" << error;
    closePipeRead = true;
    ::close(fd[0]);
    ::close(fd[1]);
    emit finished(error);
}

void SoapyPower::soapy_finished(int code)
{
    closePipeRead = true;
    ::close(fd[0]);
    ::close(fd[1]);
    qInfo() << "soapy_power exited with code" << code;
    emit finished(code);
}

