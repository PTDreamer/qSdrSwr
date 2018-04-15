#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include <QMainWindow>
#include <QtCharts/QChartGlobal>

QT_CHARTS_BEGIN_NAMESPACE
class QLineSeries;
class QChart;
QT_CHARTS_END_NAMESPACE

QT_CHARTS_USE_NAMESPACE
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QChart>
#include <QtCharts/QValueAxis>
#include <QProcess>
#include "settings.h"
#include "soapypower.h"
#include "mychart.h"
#include "callout.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
private:
    typedef enum {STATE_SCANNING, STATE_BASELINE, STATE_SINGLE_SCAN, STATE_IDLE, STATE_ABORTING} state_t;

private slots:
    void onDataReceived(SoapyPower::power_data_t);
    void autoRange();
    void on_actionCapture_Device_triggered();

    void handleFrequencyUnitsChange(int);
    void handleFFT_WindowChange();
    void handleFFT_UnitsChange();
    void handleAveragingUnitsChange();
    void handleCropUnitsChange();
    void handleViewSettingsChange();
    void actionsShowVSWR_toggled(bool checked);
    void on_actionsBeginScan_BT_toggled(bool checked);
    void onParametersChanged();
    void on_actionsoapy_power_triggered();


    void on_actionsSingleScan_BT_clicked();

    void on_actionsBaselineScan_BT_clicked();
    void keepCallout();
    void tooltip(QPointF point, bool state, bool is_db = true);
    void tooltip_vswr(QPointF point, bool state);
    void updateCalloutGeometry();
    void handleChartKey(QKeyEvent*);
    void onPlotClicked(QPointF);
    void on_plotColorBt_clicked();

    void on_plotDetach_clicked();
    void on_plotDelete_clicked();
    void on_plotSearchMaxMin_clicked(bool checked);
    void on_plotNameT_textChanged(const QString &arg1);
    void on_plotHide_clicked(bool checked);
    void on_plotList_currentTextChanged(const QString &arg1);
    void handleSeriesClick(QLineSeries *);
    void handleMaxMinSearch(QLineSeries*,float thresh);
    void onCurrentHopFinished(quint16 hop, quint16 total);
    void onCurrentScanFinished();
    void on_plotPersistence_valueChanged(double arg1);
    void on_plotnormalize_toggled(bool checked);
    void handleVSWRUpdate();
    void soapyFinished(int code);
    void on_plotExport_clicked();

    void on_plotImport_clicked();

signals:
    void baseLineUpdated();
    void scanUpdated();
private:
    state_t currentState;
    typedef struct graph {
        QString name;
        bool searchMaxMin;
        float maxMinPersistence;
        bool isSWR;
        bool hide;
        bool normalize;
        QList<Callout *> m_callouts;
    } graph_t;

    Ui::MainWindow *ui;
    QChart *m_chart;

    QLineSeries *m_scan_series;
    QLineSeries *m_baseline_series;
    qreal yMin = - 300;
    qreal yMax = - 200;
    qreal xMin;
    qreal xMax;
    QString parseDeviceSettings();
    QStringList parseParameters();
    QString deviceSettingsString;
    typedef enum {HZ, KHZ, MHZ} frequencyUnits;
    double convertFrequencyValue(double value, frequencyUnits before, frequencyUnits after);
    void setFrequencyMaxMin(double max, double min);
    void setupWidgetUpdatedConnections();
    Settings *m_settings;
    SoapyPower *m_soapy;
    Callout *m_tooltip;
    QLineSeries *selectedSeries;
    QLineSeries *m_vswr_series;
    QHash<QLineSeries *,graph_t> seriesList;
    QLineSeries *m_scan_series_backup;
    QVector<QPointF> originalData;
    qreal getValueAprox(QLineSeries *series, qreal x, bool &found);
    void normalize(bool norm);
protected:
    void closeEvent(QCloseEvent *event);
};
#endif // MAINWINDOW_H
