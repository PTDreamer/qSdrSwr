#include "mainwindow.h"
#include <QApplication>
//./linuxdeployqt-continuous-x86_64.AppImage /home/jose/code/qSdrSwr/build/Release/sdrSwr.desktop -verbose=3 -appimage -qmake=/home/jose/Qt/5.8/gcc_64/bin/qmake

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
