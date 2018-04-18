#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QtMultimedia/QAudioDeviceInfo>
#include <QtMultimedia/QAudioInput>
#include <QLocalServer>
#include <QLocalSocket>
#include <QtConcurrent>
#include <QSplineSeries>
#include <devicesettings.h>
#include <QLineEdit>
#include <QFileDialog>
#include <QMessageBox>
#include "persistence1d/persistence1d.hpp"
#include <QMenu>
#include <QColorDialog>
#include <QtMath>
#include <QLogValueAxis>
#include <QProgressDialog>

using namespace std;
using namespace p1d;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow), m_tooltip(0), selectedSeries(NULL), m_soapy(NULL), currentState(STATE_IDLE)
{
    QString settingsPath = QSettings().fileName();
    if(!QFile(settingsPath).exists()) {
        QFileInfo(settingsPath).absolutePath();
        QDir().mkdir(QFileInfo(settingsPath).absolutePath());
        QFile::copy(":/settings/settings.conf", settingsPath);
        QFile::setPermissions(settingsPath, QFileDevice::WriteOwner | QFileDevice::ReadOwner | QFileDevice::ReadGroup);
    }
    qRegisterMetaType<SoapyPower::power_data_t>("SoapyPower::power_data_t");
    ui->setupUi(this);
    m_chart = new QChart;
    MyChart *chartView = new MyChart(m_chart, this);
    chartView->setRubberBand(QChartView::RectangleRubberBand);
    chartView->setMinimumSize(800, 600);
    ui->horizontalLayout->addWidget(chartView);
    m_scan_series = new QLineSeries;
    m_scan_series->setColor(Qt::blue);
    m_chart->addSeries(m_scan_series);
    m_baseline_series = new QLineSeries;
    m_baseline_series->setColor(Qt::red);
    m_chart->addSeries(m_baseline_series);
    QValueAxis *axisX = new QValueAxis;
    axisX->setRange(70000000, 98000000);
    axisX->setLabelFormat("%4.2f");
    axisX->setTitleText("Frequency");
    QValueAxis *axisY = new QValueAxis;
    axisY->setRange(0, 0);
    axisY->setTitleText("db");
    m_chart->setAxisX(axisX, m_scan_series);
    m_chart->setAxisY(axisY, m_scan_series);
    m_chart->setAxisX(axisX, m_baseline_series);
    m_chart->setAxisY(axisY, m_baseline_series);

    m_vswr_series = new QLineSeries;
    m_vswr_series->setColor(Qt::yellow);
    m_chart->addSeries(m_vswr_series);
    QLogValueAxis *laxisY = new QLogValueAxis;
    laxisY->setRange(1, 200000);
    laxisY->setTitleText("");
    m_chart->setAxisX(m_chart->axisX(m_scan_series), m_vswr_series);
    m_chart->setAxisY(laxisY, m_vswr_series);

    m_chart->legend()->hide();

    m_settings = new Settings(ui, this);
    m_settings->begin();

    ui->actionAction->setChecked(ui->DW_actions->isVisible());
    ui->actionAvereging->setChecked(ui->DW_averaging->isVisible());
    ui->actionCrop->setChecked(ui->DW_crop->isVisible());
    ui->actionFFT->setChecked(ui->DW_fft->isVisible());
    ui->actionPerformance->setChecked(ui->DW_performance->isVisible());
    ui->actionScan_Config->setChecked(ui->DW_scanConfig->isVisible());
    ui->actionView->setChecked(ui->DW_view->isVisible());
    ui->plotNameT->blockSignals(true);
    m_settings->restoreParameters();

    handleViewSettingsChange();
    connect(ui->viewFreqAuto, SIGNAL(toggled(bool)), this, SLOT(handleViewSettingsChange()));
    connect(ui->viewPowerAuto, SIGNAL(toggled(bool)), this, SLOT(handleViewSettingsChange()));
    connect(ui->viewFreqFrom, SIGNAL(valueChanged(double)), this, SLOT(handleViewSettingsChange()));
    connect(ui->viewFreqTo, SIGNAL(valueChanged(double)), this, SLOT(handleViewSettingsChange()));
    connect(ui->viewPowerFrom, SIGNAL(valueChanged(double)), this, SLOT(handleViewSettingsChange()));
    connect(ui->viewPowerTo, SIGNAL(valueChanged(double)), this, SLOT(handleViewSettingsChange()));

    double maxFrequency;
    double minFrequency;
    m_settings->getMaxMinFrequency(maxFrequency, minFrequency);
    QSettings settings;
    deviceSettingsString = m_settings->getDeviceSettingsString();
    setFrequencyMaxMin(convertFrequencyValue(maxFrequency, MHZ, (frequencyUnits)ui->scanConfigScanUnits_CB->currentIndex()), convertFrequencyValue(minFrequency, MHZ, (frequencyUnits)ui->scanConfigScanUnits_CB->currentIndex()));
    ui->scanConfigScanUnits_CB->setProperty("previousIndex", ui->scanConfigScanUnits_CB->currentIndex());
    handleFFT_WindowChange();
    connect(ui->scanConfigScanUnits_CB, SIGNAL(currentIndexChanged(int)), this, SLOT(handleFrequencyUnitsChange(int)));
    connect(ui->fftWindow_CB, SIGNAL(currentIndexChanged(int)), this, SLOT(handleFFT_WindowChange()));
    handleFFT_UnitsChange();
    connect(ui->fftUnits_CB, SIGNAL(currentIndexChanged(int)), this, SLOT(handleFFT_UnitsChange()));
    handleAveragingUnitsChange();
    connect(ui->averagingUnits_CB, SIGNAL(currentIndexChanged(int)), this, SLOT(handleAveragingUnitsChange()));
    handleCropUnitsChange();
    connect(ui->cropUnits_CB, SIGNAL(currentIndexChanged(int)), this, SLOT(handleCropUnitsChange()));
    setupWidgetUpdatedConnections();

    m_soapy = new SoapyPower(settings.value("soapy_power").toString(), this);
    connect(m_soapy, SIGNAL(dataReceived(SoapyPower::power_data_t)), this, SLOT(onDataReceived(SoapyPower::power_data_t)));
    connect(m_soapy, SIGNAL(finished(int)), this, SLOT(soapyFinished(int)));
    connect(ui->actionClear_all_settings, SIGNAL(triggered(bool)), m_settings, SLOT(clearAllSettings()));
    connect(ui->actionRestore_Workspace, SIGNAL(triggered(bool)), m_settings, SLOT(clearAllGuiSettings()));

    connect(chartView, SIGNAL(keyEvent(QKeyEvent*)), this, SLOT(handleChartKey(QKeyEvent*)));
    connect(m_scan_series, SIGNAL(hovered(QPointF, bool)), this, SLOT(tooltip(QPointF,bool)));
    connect(m_scan_series, SIGNAL(clicked(QPointF)), this, SLOT(onPlotClicked(QPointF)));
    connect(m_vswr_series, SIGNAL(hovered(QPointF, bool)), this, SLOT(tooltip_vswr(QPointF,bool)));
    connect(m_vswr_series, SIGNAL(clicked(QPointF)), this, SLOT(onPlotClicked(QPointF)));
    connect(m_baseline_series, SIGNAL(hovered(QPointF, bool)), this, SLOT(tooltip(QPointF,bool)));
    connect(m_baseline_series, SIGNAL(clicked(QPointF)), this, SLOT(onPlotClicked(QPointF)));
    chartView->setMouseTracking(true);
    m_chart->setAcceptHoverEvents(true);
    connect(chartView, SIGNAL(sizeChanged()), this, SLOT(updateCalloutGeometry()));

    foreach (QDockWidget *dock, this->findChildren<QDockWidget *>()) {
        ui->menuView->addAction(dock->toggleViewAction());
    }

    graph_t g;
    g.searchMaxMin = false;
    g.maxMinPersistence = 0.9;
    g.name = "Current scan";
    ui->plotList->clear();
    ui->plotList->addItem(g.name);
    seriesList.clear();
    seriesList.insert(m_scan_series, g);

    g.searchMaxMin = false;
    g.maxMinPersistence = 0.9;
    g.name = "Baseline";
    ui->plotList->addItem(g.name);
    seriesList.insert(m_baseline_series, g);
    ui->plotNameT->setText(g.name);

    g.searchMaxMin = true;
    g.maxMinPersistence = 0.9;
    g.name = "VSWR";
    ui->plotList->addItem(g.name);
    seriesList.insert(m_vswr_series, g);

    handleSeriesClick(m_scan_series);
    connect(m_soapy, SIGNAL(currentHopFinished(quint16,quint16)), this, SLOT(onCurrentHopFinished(quint16,quint16)));
    connect(m_soapy, SIGNAL(currentScanFinished()), this, SLOT(onCurrentScanFinished()));
    ui->plotNameT->blockSignals(true);
    if(ui->viewFreqAuto->isChecked()) {
        m_chart->axisX()->setMax(ui->scanConfigStopFrequency_SB->value());
        m_chart->axisX()->setMin(ui->scanConfigStartFrequency_DS->value());
    }
    QList<Settings::graph_settings_t> gs = m_settings->restoreGraphs();
    foreach (QLineSeries *s, seriesList.keys()) {
        foreach (Settings::graph_settings_t g, gs) {
            if(seriesList[s].name == g.name) {
                seriesList[s].maxMinPersistence = g.maxMinPersistence;
                seriesList[s].hide = g.hide;
                if(g.hide) {
                    s->setVisible(false);
                }
                seriesList[s].normalize = g.normalize;
                seriesList[s].searchMaxMin = g.searchMaxMin;
            }
        }
    }
    handleSeriesClick(m_scan_series);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onDataReceived(SoapyPower::power_data_t array)
{
    QVector<QPointF> oldPoints = m_scan_series->pointsVector();
    quint64 index = array.index;
    bool found;
    for(quint64 x = 0; x < array.meta.array_size / sizeof(float); ++ x) {
        QPointF f = QPointF(convertFrequencyValue((array.meta.start + (x * array.meta.step)), HZ, (frequencyUnits)ui->scanConfigScanUnits_CB->currentIndex()), array.data[x]);
        if(oldPoints.length() > index) { // oldpoints 140 originaldata 92
            if(originalData.length() > index)
                originalData.replace(index, f);
            else
                originalData.append(f);
            if(seriesList[m_scan_series].normalize && (currentState == STATE_SCANNING || currentState == STATE_SINGLE_SCAN)) {
                qreal y = getValueAprox(m_baseline_series, f.x(), found);
                if(found)
                    f.setY(f.y() - y);
            }
            oldPoints.replace(index, f);
        }
        else {
            originalData.append(f);
            if(seriesList[m_scan_series].normalize && (currentState == STATE_SCANNING || currentState == STATE_SINGLE_SCAN)) {
                qreal y = getValueAprox(m_baseline_series, f.x(), found);
                if(found)
                    f.setY(f.y() - y);
            }
            oldPoints.append(f);
        }
        ++index;
    }
    autoRange();
    m_scan_series->replace(oldPoints);
}


void MainWindow::autoRange()
{
    if(!ui->viewPowerAuto->isChecked())
        return;
//    qreal maxy = -1000;
//    qreal miny = 1000;
    qreal maxydb = -1000;
    qreal minydb = 1000;

    foreach (QLineSeries *series, seriesList.keys()) {
        if(!series)
            continue;
        if(!series->isVisible())
            continue;
        foreach (QPointF p, series->points()) {
            if(m_chart->axisY(series) == m_chart->axisY(m_scan_series)) {
                if(p.y() < minydb)
                    minydb = p.y();
                if(p.y() > maxydb)
                    maxydb = p.y();
            }
//            else if(m_chart->axisY(series) == m_chart->axisY(m_vswr_series)) {
//                if(p.y() < miny)
//                    miny = p.y();
//                if(p.y() > maxy)
//                    maxy = p.y();
//            }
        }
    }
//    if(maxy != -1000)
//        m_chart->axisY(m_vswr_series)->setMax(maxy);
//    if(miny != 1000)
//        m_chart->axisY(m_vswr_series)->setMin(miny);
    if(maxydb != -1000)
        m_chart->axisY(m_scan_series)->setMax(maxydb);
    if(minydb != 1000)
        m_chart->axisY(m_scan_series)->setMin(minydb);
}

QStringList MainWindow::parseParameters()
{
    QStringList parameters;
    QRegExp re;
    re.setPattern("*dockWidgetContents*");
    re.setPatternSyntax(QRegExp::Wildcard);
    QDockWidget *dock = NULL;
    foreach (QObject *o, this->children()) {
        dock = qobject_cast<QDockWidget*>(o);
        if(!dock)
            continue;
        QObject *cont = dock->findChildren<QWidget*>(re,Qt::FindDirectChildrenOnly).at(0);
        foreach (QObject *w, cont->children()) {
            QComboBox *cb = qobject_cast<QComboBox*>(w);
            QSpinBox *sb = qobject_cast<QSpinBox*>(w);
            QDoubleSpinBox *dsb = qobject_cast<QDoubleSpinBox*>(w);
            QCheckBox *chb = qobject_cast<QCheckBox*>(w);
            QLineEdit *le = qobject_cast<QLineEdit*>(w);
            QWidget *ww = qobject_cast<QWidget*>(w);
            if(ww && !ww->isVisible())
                continue;
            if(!w->property("cli").isValid())
                continue;
            if(cb)
                parameters << cb->property("cli").toString() << cb->currentText();
            else if(sb)
                parameters << sb->property("cli").toString() << QString::number(sb->value());
            else if(dsb)
                parameters << dsb->property("cli").toString() << QString::number(dsb->value());
            else if(chb && chb->isChecked())
                parameters << chb->property("cli").toString();
            else if(le)
                parameters << le->property("cli").toString() << le->text();
        }
    }
    switch (ui->perfNumberOfFFTBins_CB->currentIndex()) {
    case 0:

        break;
    case 1:
        parameters << "--even";
        break;
    case 2:
        parameters << "--pow2";
        break;
    default:
        break;
    }
    return parameters;
}

double MainWindow::convertFrequencyValue(double value, MainWindow::frequencyUnits before, MainWindow::frequencyUnits after)
{
    int exp = before - after;
    return value * pow(10, 3 * exp);
}

void MainWindow::setFrequencyMaxMin(double max, double min)
{
    ui->scanConfigStartFrequency_DS->setMaximum(max);
    ui->scanConfigStopFrequency_SB->setMaximum(max);
    ui->scanConfigStartFrequency_DS->setMinimum(min);
    ui->scanConfigStopFrequency_SB->setMinimum(min);
}

void MainWindow::setupWidgetUpdatedConnections()
{
    QRegExp re;
    re.setPattern("*dockWidgetContents*");
    re.setPatternSyntax(QRegExp::Wildcard);
    QDockWidget *dock = NULL;
    foreach (QObject *o, this->children()) {
        dock = qobject_cast<QDockWidget*>(o);
        if(!dock)
            continue;
        QObject *cont = dock->findChildren<QWidget*>(re,Qt::FindDirectChildrenOnly).at(0);
        foreach (QObject *w, cont->children()) {
            QComboBox *cb = qobject_cast<QComboBox*>(w);
            QSpinBox *sb = qobject_cast<QSpinBox*>(w);
            QDoubleSpinBox *dsb = qobject_cast<QDoubleSpinBox*>(w);
            QCheckBox *chb = qobject_cast<QCheckBox*>(w);
            if(cb == ui->fftUnits_CB || cb == ui->scanConfigScanUnits_CB || cb == ui->averagingUnits_CB || cb == ui->cropUnits_CB)
                continue;
            if(cb)
                connect(cb, SIGNAL(currentIndexChanged(int)), this, SLOT(onParametersChanged()));
            else if(sb)
                connect(sb, SIGNAL(valueChanged(int)), this, SLOT(onParametersChanged()));
            else if(dsb)
                connect(dsb, SIGNAL(valueChanged(double)), this, SLOT(onParametersChanged()));
            else if(chb)
                connect(chb, SIGNAL(toggled(bool)), this, SLOT(onParametersChanged()));
        }
    }
}

qreal MainWindow::getValueAprox(QLineSeries *series, qreal xval, bool &found)
{
    QVector<QPointF> values = series->pointsVector();
    for(int x = 0; x < values.count() - 1; ++x) {
        if(values[x].x() == xval) {
            found = true;
            return values[x].y();
        }
        else if(values[x + 1].x() == xval) {
            found = true;
            return values[x + 1].y();
        }
        if(values[x].x() < xval && values[x+1].x() >= xval) {
            found = true;
            qreal m = (values[x+1].y() - values[x].y()) / (values[x+1].x() - values[x].x());
            qreal b = m * values[x].x() - values[x].y();
            b = -b;
            return m * xval + b;
        }
    }
    found = false;
    return 0;
}

void MainWindow::normalize(bool checked)
{
    QVector<QPointF> backup;
    bool found;
    if(checked) {
        originalData.clear();
        originalData.append(m_scan_series->pointsVector());
        backup.clear();
        backup.append(originalData);
        for(int x = 0; x < backup.length(); ++x) {
            QPointF f = backup.at(x);
            qreal y = getValueAprox(m_baseline_series, f.x(), found);
            if(!found)
                continue;
            f.setY(f.y() - y);
            backup.replace(x, f);
        }
        m_scan_series->replace(backup);
    }
    else {
        m_scan_series->replace(originalData);
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QList<Settings::graph_settings_t> graphs;
    foreach (graph_t t, seriesList.values()) {
        Settings::graph_settings_t st;
        st.hide = t.hide;
        st.maxMinPersistence = t.maxMinPersistence;
        st.name = t.name;
        st.normalize = t.normalize;
        st.searchMaxMin = t.searchMaxMin;
        graphs.append(st);
    }
    m_settings->end(graphs);
    m_soapy->stop();
    QMainWindow::closeEvent(event);
}

void MainWindow::on_actionCapture_Device_triggered()
{
    DeviceSettings settings(this);
    int res = settings.exec();
    if(res == QDialog::Accepted) {
        double max, min;
        m_settings->getMaxMinFrequency(max, min);
        max = convertFrequencyValue(max, MHZ, (frequencyUnits)ui->scanConfigScanUnits_CB->currentIndex());
        min = convertFrequencyValue(min, MHZ, (frequencyUnits)ui->scanConfigScanUnits_CB->currentIndex());
        if(max) {
            ui->scanConfigStartFrequency_DS->setMaximum(max);
            ui->scanConfigStopFrequency_SB->setMaximum(max);
        }
        if(min) {
            ui->scanConfigStartFrequency_DS->setMinimum(min);
            ui->scanConfigStopFrequency_SB->setMinimum(min);
        }
    }
}

void MainWindow::handleFrequencyUnitsChange(int index)
{
    double newStart = convertFrequencyValue(ui->scanConfigStartFrequency_DS->value(), (frequencyUnits)ui->scanConfigScanUnits_CB->property("previousIndex").toInt(), (frequencyUnits)index);
    double newStop = convertFrequencyValue(ui->scanConfigStopFrequency_SB->value(), (frequencyUnits)ui->scanConfigScanUnits_CB->property("previousIndex").toInt(), (frequencyUnits)index);
    double newMax =convertFrequencyValue(ui->scanConfigStopFrequency_SB->maximum(), (frequencyUnits)ui->scanConfigScanUnits_CB->property("previousIndex").toInt(), (frequencyUnits)index);
    double newMin =convertFrequencyValue(ui->scanConfigStopFrequency_SB->minimum(), (frequencyUnits)ui->scanConfigScanUnits_CB->property("previousIndex").toInt(), (frequencyUnits)index);
    setFrequencyMaxMin(newMax, newMin);
    ui->scanConfigStartFrequency_DS->setValue(newStart);
    ui->scanConfigStopFrequency_SB->setValue(newStop);
    ui->scanConfigScanUnits_CB->setProperty("previousIndex", index);
}

void MainWindow::handleFFT_WindowChange()
{
    if(ui->fftWindow_CB->currentText().contains("tukey") || ui->fftWindow_CB->currentText().contains("kaiser")) {
        ui->fftWindowShape_FT->setVisible(true);
        ui->fftWindowShapeLabel->setVisible(true);
    }
    else {
        ui->fftWindowShape_FT->setVisible(false);
        ui->fftWindowShapeLabel->setVisible(false);
    }
}

void MainWindow::handleFFT_UnitsChange()
{
    if(ui->fftUnits_CB->currentText().contains("Bins")) {
        ui->fftValue_SB->setProperty("cli", "-b");
    }
    else if(ui->fftUnits_CB->currentText().contains("Hz")) {
        ui->fftValue_SB->setProperty("cli", "-B");

    }
}

void MainWindow::handleAveragingUnitsChange()
{
    switch (ui->averagingUnits_CB->currentIndex()) {
    case 0:
        ui->averagingValue_SB->setProperty("cli", "-n");
        break;
    case 1:
        ui->averagingValue_SB->setProperty("cli", "-t");
        break;
    case 2:
        ui->averagingValue_SB->setProperty("cli", "-T");
        break;
    default:
        break;
    }
}

void MainWindow::handleCropUnitsChange()
{
    if(ui->cropUnits_CB->currentText().contains("Crop")) {
        ui->cropValue_SB->setProperty("cli", "--crop");
    }
    else if(ui->cropUnits_CB->currentText().contains("Overlap")) {
        ui->cropValue_SB->setProperty("cli", "--overlap");

    }
}

void MainWindow::handleViewSettingsChange()
{
    ui->viewFreqFrom->setEnabled(!ui->viewFreqAuto->isChecked());
    ui->viewFreqFrom_label->setEnabled(!ui->viewFreqAuto->isChecked());
    ui->viewFreqTo->setEnabled(!ui->viewFreqAuto->isChecked());
    ui->viewFreqTo_label->setEnabled(!ui->viewFreqAuto->isChecked());

    ui->viewPowerFrom->setEnabled(!ui->viewPowerAuto->isChecked());
    ui->viewPowerFrom_label->setEnabled(!ui->viewPowerAuto->isChecked());
    ui->viewPowerTo->setEnabled(!ui->viewPowerAuto->isChecked());
    ui->viewPowerTo_label->setEnabled(!ui->viewPowerAuto->isChecked());

    if(!ui->viewFreqAuto->isChecked()) {
        m_chart->axisX()->setMin(ui->viewFreqFrom->value());
        m_chart->axisX()->setMax(ui->viewFreqTo->value());
    }
    if(!ui->viewPowerAuto->isChecked()) {
        m_chart->axisY()->setMin(ui->viewPowerFrom->value());
        m_chart->axisY()->setMax(ui->viewPowerTo->value());
    }
    else
        autoRange();
}

void MainWindow::actionsShowVSWR_toggled(bool checked)
{
    m_vswr_series->setVisible(checked);
    m_chart->axisY(m_vswr_series)->setVisible(checked);
    if(!m_soapy)
        return;
}

void MainWindow::on_actionsBeginScan_BT_toggled(bool checked)
{
    if(checked) {
        double start = convertFrequencyValue(ui->scanConfigStartFrequency_DS->value(), (frequencyUnits)ui->scanConfigScanUnits_CB->currentIndex(), MHZ);
        double stop = convertFrequencyValue(ui->scanConfigStopFrequency_SB->value(), (frequencyUnits)ui->scanConfigScanUnits_CB->currentIndex(), MHZ);
        bool res = m_soapy->start(parseParameters(), deviceSettingsString, start, stop, true);
        if(!res) {
            QMessageBox::critical(this, "Soapy Power executable not set", "Please set Soapy Power executable location");
            ui->actionsBeginScan_BT->setChecked(false);
            return;
        }
        ui->actionsBeginScan_BT->setText("Stop Scan");
        m_scan_series->clear();
        originalData.clear();
        qDeleteAll(seriesList[m_scan_series].m_callouts);
        seriesList[m_scan_series].m_callouts.clear();
        m_chart->axisX()->setMax(ui->scanConfigStopFrequency_SB->value());
        m_chart->axisX()->setMin(ui->scanConfigStartFrequency_DS->value());
        currentState = STATE_SCANNING;
        ui->actionsSingleScan_BT->setEnabled(false);
        ui->actionsBaselineScan_BT->setEnabled(false);
        ui->actionsBeginScan_BT->setEnabled(true);
    }
    else {
        ui->actionsBeginScan_BT->setText("Begin Scan");
        m_soapy->stop();
    }
}

void MainWindow::onParametersChanged()
{

}

void MainWindow::on_actionsoapy_power_triggered()
{
    QString soapyPower = QFileDialog::getOpenFileName(this, "Select Soapy Power executable");
    QSettings settings;
    settings.setValue("soapy_power", soapyPower);
    m_soapy->setSoapyExec(soapyPower);
}

void MainWindow::on_actionsSingleScan_BT_clicked()
{
    if(currentState == STATE_SINGLE_SCAN) {
        ui->actionsSingleScan_BT->setText("Single Scan");
        currentState = STATE_ABORTING;
        m_soapy->stop();
        return;
    }
    double start = convertFrequencyValue(ui->scanConfigStartFrequency_DS->value(), (frequencyUnits)ui->scanConfigScanUnits_CB->currentIndex(), MHZ);
    double stop = convertFrequencyValue(ui->scanConfigStopFrequency_SB->value(), (frequencyUnits)ui->scanConfigScanUnits_CB->currentIndex(), MHZ);
    bool res = m_soapy->start(parseParameters(), deviceSettingsString, start, stop, false);
    if(!res) {
        QMessageBox::critical(this, "Soapy Power executable not set", "Please set Soapy Power executable location");
        ui->actionsBeginScan_BT->setChecked(false);
        currentState = STATE_IDLE;
        ui->actionsSingleScan_BT->setText("Baseline Scan");
        ui->actionsSingleScan_BT->setText("Single Scan");
        return;
    }
    m_scan_series->clear();
    originalData.clear();
    qDeleteAll(seriesList[m_scan_series].m_callouts);
    seriesList[m_scan_series].m_callouts.clear();
    if(ui->viewFreqAuto->isChecked()) {
        m_chart->axisX()->setMax(ui->scanConfigStopFrequency_SB->value());
        m_chart->axisX()->setMin(ui->scanConfigStartFrequency_DS->value());
    }
    if(currentState == STATE_BASELINE) {
        ui->actionsBaselineScan_BT->setText("Stop");
        ui->actionsSingleScan_BT->setEnabled(false);
        ui->actionsBaselineScan_BT->setEnabled(true);
        ui->actionsBeginScan_BT->setEnabled(false);
    }
    else {
        ui->actionsSingleScan_BT->setText("Stop");
        currentState = STATE_SINGLE_SCAN;
        ui->actionsSingleScan_BT->setEnabled(true);
        ui->actionsBaselineScan_BT->setEnabled(false);
        ui->actionsBeginScan_BT->setEnabled(false);
    }
}

void MainWindow::on_actionsBaselineScan_BT_clicked()
{
    if(currentState == STATE_BASELINE) {
        ui->actionsBaselineScan_BT->setText("Baseline Scan");
        currentState = STATE_ABORTING;
        m_scan_series = m_scan_series_backup;
        m_soapy->stop();
        return;
    }
    m_scan_series_backup = m_scan_series;
    m_scan_series = m_baseline_series;
    currentState = STATE_BASELINE;
    on_actionsSingleScan_BT_clicked();
}

void MainWindow::keepCallout()
{
    seriesList[selectedSeries].m_callouts.append(m_tooltip);
    m_tooltip = new Callout(m_chart);
}

void MainWindow::tooltip(QPointF point, bool state, bool is_db)
{
    QAbstractSeries *s = qobject_cast<QAbstractSeries*>(sender());
    if(!s)
        return;
    QString yUnit;
    if(is_db)
        yUnit = "dB";
    if (m_tooltip == 0)
        m_tooltip = new Callout(m_chart);

    if (state) {
        m_tooltip->setText(QString("%1%2 \n%3%4 ").arg(point.x()).arg(ui->scanConfigScanUnits_CB->currentText()).arg(point.y()).arg(yUnit));
        m_tooltip->setAnchor(point, s);
        m_tooltip->setZValue(11);
        m_tooltip->updateGeometry();
        m_tooltip->show();
    } else {
        m_tooltip->hide();
    }
}

void MainWindow::tooltip_vswr(QPointF point, bool state)
{
    tooltip(point, state, false);
}

void MainWindow::updateCalloutGeometry()
{
    foreach (graph_t g, seriesList) {
        foreach (Callout *callout, g.m_callouts)
            callout->updateGeometry();
    }

}

void MainWindow::handleChartKey(QKeyEvent *e)
{
    if(e->key() == Qt::Key_Insert)
        keepCallout();
}

void MainWindow::onPlotClicked(QPointF)
{
    QLineSeries *s = qobject_cast<QLineSeries *>(sender());
    handleSeriesClick(s);
}

void MainWindow::on_plotColorBt_clicked()
{
    QColor c = QColorDialog::getColor(ui->plotColorBt->palette().color(ui->plotColorBt->backgroundRole()), this);
    QPen p = selectedSeries->pen();
    p.setColor(c);
    selectedSeries->setPen(p);
    QPalette palette = ui->plotColorBt->palette();
    palette.setColor(ui->plotColorBt->backgroundRole(), p.color());
    palette.setColor(ui->plotColorBt->foregroundRole(), p.color());
    ui->plotColorBt->setAutoFillBackground(true);
    ui->plotColorBt->setPalette(palette);

}

void MainWindow::on_plotDetach_clicked()
{
    static quint8 newplots = 0;
    static QList<QString> colors = QList<QString>() << QString("red") << QString("green")
    << QString("blueviolet")
    << QString("brown")
    << QString("burlywood")
    << QString("cadetblue")
    << QString("chartreuse")
    << QString("chocolate")
    << QString("coral")
    << QString("cornflowerblue");
    QAbstractAxis *x = m_chart->axisX(selectedSeries);
    QAbstractAxis *y = m_chart->axisY(selectedSeries);
    QLineSeries *series = new QLineSeries;
    series->replace(selectedSeries->pointsVector());
    graph_t g = seriesList[selectedSeries];
    seriesList.insert(series, g);
    seriesList[series].name = QString("%0 #%1").arg(seriesList[selectedSeries].name).arg(newplots);
    ui->plotList->addItem(seriesList[series].name);
    ++newplots;
    foreach (QString color, colors) {
        bool colorTaken = false;
        foreach (QLineSeries *s, seriesList.keys()) {
            if(s->color().name() == QColor(color).name())
                colorTaken = true;
        }
        if(!colorTaken && color != "aliceblue") {
            series->setColor(QColor(color));
            break;
        }
    }

    m_chart->addSeries(series);
    m_chart->setAxisX(x, series);
    m_chart->setAxisY(y, series);
    connect(series, SIGNAL(hovered(QPointF, bool)), this, SLOT(tooltip(QPointF,bool)));
    connect(series, SIGNAL(clicked(QPointF)), this, SLOT(onPlotClicked(QPointF)));

}

void MainWindow::on_plotDelete_clicked()
{
    m_chart->removeSeries(selectedSeries);
    foreach (Callout *c, seriesList[selectedSeries].m_callouts) {
        delete c;
    }
    ui->plotList->blockSignals(true);
    for(int x = 0; x < ui->plotList->count(); ++x) {
        if(seriesList[selectedSeries].name == ui->plotList->itemText(x))
            ui->plotList->removeItem(x);
    }
    ui->plotList->blockSignals(false);
    seriesList.remove(selectedSeries);
    delete selectedSeries;
    handleSeriesClick(m_scan_series);
}

void MainWindow::handleSeriesClick(QLineSeries *s)
{
    if(s) {
        foreach (QLineSeries *series, seriesList.keys()) {
            QPen p = series->pen();
            p.setWidth(1);
            series->setPen(p);
        }
        selectedSeries = s;
        QPen p = s->pen();
        p.setWidth(3);
        s->setPen(p);
        QPalette palette = ui->plotColorBt->palette();
        palette.setColor(ui->plotColorBt->backgroundRole(), p.color());
        palette.setColor(ui->plotColorBt->foregroundRole(), p.color());
        ui->plotColorBt->setAutoFillBackground(true);
        ui->plotColorBt->setPalette(palette);
        ui->plotList->blockSignals(true);
        ui->plotNameT->blockSignals(true);
        ui->plotList->setCurrentText(seriesList[selectedSeries].name);
        ui->plotNameT->setText(seriesList[selectedSeries].name);
        ui->plotList->blockSignals(false);
        ui->plotNameT->blockSignals(false);
        ui->plotSearchMaxMin->setChecked(seriesList[selectedSeries].searchMaxMin);
        ui->plotPersistence->setValue(seriesList[selectedSeries].maxMinPersistence);
        ui->plotHide->setChecked(seriesList[selectedSeries].hide);
        ui->plotnormalize->setChecked(seriesList[selectedSeries].normalize);
        bool val = (s == m_scan_series || s == m_baseline_series || s == m_vswr_series);
        ui->plotDelete->setEnabled(!val);
        ui->plotDetach->setEnabled(val);
        ui->plotNameT->setEnabled(!val);
        ui->plotnormalize->setVisible(s == m_scan_series);
    }
}

void MainWindow::handleMaxMinSearch(QLineSeries *s, float thresh)
{
    if(!s)
        return;
    if(!seriesList[selectedSeries].searchMaxMin)
        return;
    if(selectedSeries->points().count() == 0)
        return;
    QString yUnit;
    if(s != m_vswr_series)
        yUnit = "dB";
    qDeleteAll(seriesList[s].m_callouts);
    seriesList[s].m_callouts.clear();
    Persistence1D p;
    vector< float > data;
    foreach (QPointF point, s->points()) {
        data.push_back(point.y());
    }
    p.RunPersistence(data);
    vector< TPairedExtrema > Extrema;
    p.GetPairedExtrema(Extrema, thresh);

    //Print all found pairs - pairs are sorted ascending wrt. persistence.
      for(vector< TPairedExtrema >::iterator it = Extrema.begin(); it != Extrema.end(); it++)
      {
          Callout *c = new Callout(m_chart);
          c->setText(QString("%1%2 \n%3%4 ").arg(s->points().at((*it).MaxIndex).x()).arg(ui->scanConfigScanUnits_CB->currentText()).arg(s->points().at((*it).MaxIndex).y()).arg(yUnit));
          c->setAnchor(s->points().at((*it).MaxIndex), s);
          c->setColor(Qt::green);
          c->setZValue(11);
          c->updateGeometry();
          c->show();
          seriesList[s].m_callouts.append(c);
          c = new Callout(m_chart);
          c->setText(QString("%1%2 \n%3%4 ").arg(s->points().at((*it).MaxIndex).x()).arg(ui->scanConfigScanUnits_CB->currentText()).arg(s->points().at((*it).MaxIndex).y()).arg(yUnit));
          c->setAnchor(s->points().at((*it).MinIndex), s);
          c->setColor(Qt::red);
          c->setZValue(11);
          c->updateGeometry();
          c->show();
          seriesList[s].m_callouts.append(c);
      }

      if(s != m_vswr_series)
           return;
       Callout *c = new Callout(m_chart);
       c->setText(QString("%1%2 \n%3%4 ").arg(s->points().at(p.GetGlobalMinimumIndex()).x()).arg(ui->scanConfigScanUnits_CB->currentText()).arg(s->points().at(p.GetGlobalMinimumIndex()).y()).arg(yUnit));
       c->setAnchor(s->points().at(p.GetGlobalMinimumIndex()), s);
       c->setColor(Qt::green);
       c->setZValue(11);
       c->updateGeometry();
       c->show();
       seriesList[s].m_callouts.append(c);
}

void MainWindow::onCurrentHopFinished(quint16 hop, quint16 total)
{
   // qDebug() << "HOP " << hop << " OF " << total << "finished";
}

void MainWindow::onCurrentScanFinished()
{
   // qDebug() << "Current scan finished";
    if(seriesList[m_scan_series].searchMaxMin)
        handleMaxMinSearch(m_scan_series, seriesList[m_scan_series].maxMinPersistence);
    handleVSWRUpdate();
    emit scanUpdated();
}

void MainWindow::on_plotSearchMaxMin_clicked(bool checked)
{
    seriesList[selectedSeries].searchMaxMin = checked;
    qDeleteAll(seriesList[selectedSeries].m_callouts);
    seriesList[selectedSeries].m_callouts.clear();
    if(checked) {
        seriesList[selectedSeries].maxMinPersistence = ui->plotPersistence->value();
        handleMaxMinSearch(selectedSeries, ui->plotPersistence->value());
    }
}

void MainWindow::on_plotPersistence_valueChanged(double arg1)
{
    seriesList[selectedSeries].maxMinPersistence = arg1;
    if(ui->plotSearchMaxMin->isChecked()) {
        handleMaxMinSearch(selectedSeries, arg1);
    }
}

void MainWindow::on_plotNameT_textChanged(const QString &arg1)
{
    ui->plotList->blockSignals(true);
    seriesList[selectedSeries].name = arg1;
    ui->plotList->clear();
    foreach (graph_t g, seriesList.values()) {
        ui->plotList->addItem(g.name);
    }
    ui->plotList->setCurrentText(arg1);
    ui->plotList->blockSignals(false);
}

void MainWindow::on_plotHide_clicked(bool checked) {
    seriesList[selectedSeries].hide = checked;
    selectedSeries->setVisible(!checked);
    if(selectedSeries == m_vswr_series) {
        actionsShowVSWR_toggled(!checked);
    }
    autoRange();
}

void MainWindow::on_plotList_currentTextChanged(const QString &arg1)
{
    foreach (QLineSeries *s, seriesList.keys()) {
        if(seriesList[s].name == arg1) {
            handleSeriesClick(s);
        }
    }
}
void MainWindow::on_plotnormalize_toggled(bool checked)
{
    seriesList[selectedSeries].normalize = checked;
    if(selectedSeries != m_scan_series)
        return;
    normalize(checked);
}

void MainWindow::handleVSWRUpdate()
{
    QVector<QPointF> vector;
    bool found;
    for(int x = 0; x < originalData.length(); ++x) {
        QPointF p = originalData.at(x);
        qreal rl = getValueAprox(m_baseline_series , p.x(), found) - p.y();
        if(!found)
            continue;
        rl = qMax(0.0001, rl);
        qreal y =(qPow(10,(rl/20)) + 1) / (qPow(10,(rl/20)) - 1);
        p.setY(y);
        vector.append(p);
    }
    m_vswr_series->replace(vector);
    handleMaxMinSearch(m_vswr_series, ui->plotPersistence->value());
    qDebug() << "VSWR calculated";
}

void MainWindow::soapyFinished(int code)
{
    qDebug() << "soapyfinished" << code;
    switch (currentState) {
    case STATE_ABORTING:
        currentState = STATE_IDLE;
        break;
    case STATE_SINGLE_SCAN:
        ui->actionsSingleScan_BT->setText("Single Scan");
        currentState = STATE_IDLE;
        break;
    case STATE_BASELINE:
        m_scan_series = m_scan_series_backup;
        emit baseLineUpdated();
        ui->actionsBaselineScan_BT->setText("Baseline Scan");
        currentState = STATE_IDLE;
        if(seriesList[m_scan_series].normalize) {
            normalize(true);
            autoRange();
        }
        handleVSWRUpdate();
        break;
    case STATE_SCANNING:
        ui->actionsBeginScan_BT->setText("Begin Scan");
        ui->actionsBeginScan_BT->setEnabled(true);
        currentState = STATE_IDLE;
        break;
    default:
        break;
    }
    if(currentState == STATE_IDLE) {
        ui->actionsSingleScan_BT->setText("Single Scan");
        ui->actionsBaselineScan_BT->setText("Baseline Scan");
        ui->actionsBeginScan_BT->setText("Begin Scan");
        ui->actionsSingleScan_BT->setEnabled(true);
        ui->actionsBaselineScan_BT->setEnabled(true);
        ui->actionsBeginScan_BT->setEnabled(true);
    }
}

void MainWindow::on_plotExport_clicked()
{
    QString filter = "Comma-separated values (*.csv)";
    QString filename = QFileDialog::getSaveFileName(this, "Export file name","",filter, &filter);
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
              QMessageBox::information(this, tr("Unable to open file"),
                  file.errorString());
              return;
    }
    QTextStream stream( &file );
    foreach (QPointF p, selectedSeries->pointsVector()) {
        stream << p.x() << "," << p.y() << "\n";
    }
}

void MainWindow::on_plotImport_clicked()
{
    QString filter = "Comma-separated values (*.csv);; All (*.*)";
    QString filename = QFileDialog::getOpenFileName(this, "Export file name","",filter, &filter);
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
              QMessageBox::information(this, tr("Unable to open file"),
                  file.errorString());
              return;
    }
    QTextStream stream( &file );
    QString line;
    QPointF p;
    selectedSeries->clear();
    bool error = false;
    QVector<QPointF> points;
    do {
        line = stream.readLine();
        QStringList sl = line.split(",");
        if(sl.count() != 2) {
            error = true;
            continue;
        }
        QString xs = sl.at(0);
        QString ys = sl.at(1);
        qreal x = xs.toDouble();
        qreal y = ys.toDouble();
        p.setX(x);
        p.setY(y);
        points.append(p);
    } while(!line.isNull());
    selectedSeries->replace(points);
    autoRange();
    if(selectedSeries == m_scan_series || selectedSeries == m_baseline_series)
        handleVSWRUpdate();
        if(seriesList[m_scan_series].normalize) {
            normalize(true);
            autoRange();
        }
}
