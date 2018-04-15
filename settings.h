#ifndef SETTINGS_H
#define SETTINGS_H
#include <QMainWindow>
#include <QPointF>
#include <QSettings>
#include <QObject>

namespace Ui {
class MainWindow;
}

class Settings : public QObject
{
    Q_OBJECT
public:
    typedef struct graph_settings {
        QString name;
        bool searchMaxMin;
        float maxMinPersistence;
        bool isSWR;
        bool hide;
        bool normalize;
    } graph_settings_t;

    Settings(Ui::MainWindow *mui, QMainWindow *mainWindow);
    void begin();
    void end(QList<graph_settings_t> graphs);
    void getMaxMinFrequency(double &max, double &min);
    QString getDeviceSettingsString();
    void restoreParameters();
    void saveParameters();
    void saveGraphs(QList<graph_settings_t> graphs);
    QList<graph_settings_t> restoreGraphs();
public slots:
    void clearAllSettings();
    void clearAllGuiSettings();
private slots:



private:
    void parseSaveObjectList(QObjectList list, QDockWidget *dock);
    void parseRestoreObjectList(QObjectList list, QDockWidget *dock);
    Ui::MainWindow *ui;
    QMainWindow *mWindow;
    QSettings settings;
};

#endif // SETTINGS_H
