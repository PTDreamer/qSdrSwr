#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QCoreApplication::setOrganizationName("JBTec");
    QCoreApplication::setOrganizationDomain("jbtecnologia.com");
    QCoreApplication::setApplicationName("SDR spectrum & swr analyser");
    MainWindow w;
    w.show();

    return a.exec();
}
