#include "settings.h"
#include "ui_mainwindow.h"
#include <QLineEdit>
#include <QDebug>
#include <QScrollArea>

Settings::Settings(Ui::MainWindow *mui, QMainWindow *mainWindow): QObject(mainWindow), ui(mui), mWindow(mainWindow)
{
}

void Settings::begin()
{
    if(settings.allKeys().contains("gui/State"))
        mWindow->restoreState(settings.value("gui/State").toByteArray());
    else
        settings.setValue("guiDefault/State",mWindow->saveState());
    if(settings.allKeys().contains("gui/Geometry"))
        mWindow->restoreGeometry(settings.value("gui/Geometry").toByteArray());
    else
        settings.setValue("guiDefault/Geometry",mWindow->saveGeometry());

}

void Settings::end(QList<graph_settings_t> graphs)
{    if(settings.allKeys().length()) {
        settings.setValue("gui/State", mWindow->saveState());
        settings.setValue("gui/Geometry", mWindow->saveGeometry());
        saveParameters();
        saveGraphs(graphs);
    }
}

void Settings::getMaxMinFrequency(double &max, double &min)
{
    max = settings.value("deviceSettings/maxFrequency", (double) 9999999999).toDouble();
    min = settings.value("deviceSettings/minFrequency", 0).toDouble();
}

QString Settings::getDeviceSettingsString()
{
     return settings.value("deviceSettings/settingsString").toString();
}

void Settings::restoreParameters()
{
    QRegExp re;
    re.setPattern("*dockWidgetContents*");
    re.setPatternSyntax(QRegExp::Wildcard);
    QDockWidget *dock = NULL;
    foreach (QObject *o, mWindow->children()) {
        dock = qobject_cast<QDockWidget*>(o);
        if(!dock)
            continue;
        QObject *cont = dock->findChildren<QWidget*>(re,Qt::FindDirectChildrenOnly).at(0);
        parseRestoreObjectList(cont->children(), dock);
    }
}

void Settings::saveParameters()
{
    QRegExp re;
    re.setPattern("*dockWidgetContents*");
    re.setPatternSyntax(QRegExp::Wildcard);
    QDockWidget *dock = NULL;
    foreach (QObject *o, mWindow->children()) {
        dock = qobject_cast<QDockWidget*>(o);
        if(!dock)
            continue;
        QObject *cont = dock->findChildren<QWidget*>(re,Qt::FindDirectChildrenOnly).at(0);
        if(cont)
            parseSaveObjectList(cont->children(), dock);
    }
}

void Settings::saveGraphs(QList<Settings::graph_settings_t> graphs)
{
    settings.beginGroup("parameters/graphs");
    foreach (graph_settings_t g, graphs) {
        settings.beginGroup(g.name);
        settings.setValue("hide", g.hide);
        settings.setValue("persistence", g.maxMinPersistence);
        settings.setValue("normalize", g.normalize);
        settings.setValue("searchmaxmin", g.searchMaxMin);
        settings.endGroup();
    }
    settings.endGroup();
}

QList<Settings::graph_settings_t> Settings::restoreGraphs()
{
    QList<Settings::graph_settings_t> list;
    settings.beginGroup("parameters/graphs");
    foreach (QString name, settings.childGroups()) {
        settings.beginGroup(name);
        graph_settings_t t;
        t.name = name;
        t.hide = settings.value("hide").toBool();
        t.maxMinPersistence = settings.value("persistence").toFloat();
        t.normalize = settings.value("normalize").toBool();
        t.searchMaxMin = settings.value("searchmaxmin").toBool();
        settings.endGroup();
        list.append(t);
    }
    settings.endGroup();
    return list;
}
void Settings::clearAllSettings()
{
    settings.clear();
}

void Settings::clearAllGuiSettings()
{
    settings.remove("gui");
    mWindow->restoreState(settings.value("guiDefault/State").toByteArray());
}

void Settings::parseSaveObjectList(QObjectList list, QDockWidget *dock)
{
    foreach (QObject *w, list) {
        QComboBox *cb = qobject_cast<QComboBox*>(w);
        QSpinBox *sb = qobject_cast<QSpinBox*>(w);
        QDoubleSpinBox *dsb = qobject_cast<QDoubleSpinBox*>(w);
        QCheckBox *chb = qobject_cast<QCheckBox*>(w);
        QLineEdit *le = qobject_cast<QLineEdit*>(w);
        if(!(cb || sb || dsb || chb || le)) {
            parseSaveObjectList(w->children(), dock);
        }
        QString setting = QString("parameters/%1/%2").arg(dock->objectName()).arg(w->objectName());
        if(cb)
            settings.setValue(setting, cb->currentText());
        else if(sb)
            settings.setValue(setting, sb->value());
        else if(dsb)
            settings.setValue(setting, dsb->value());
        else if(chb)
            settings.setValue(setting, chb->isChecked());
        else if(le)
            settings.setValue(setting, le->text());
    }
}

void Settings::parseRestoreObjectList(QObjectList list, QDockWidget *dock)
{
    foreach (QObject *w, list) {
        QComboBox *cb = qobject_cast<QComboBox*>(w);
        QSpinBox *sb = qobject_cast<QSpinBox*>(w);
        QDoubleSpinBox *dsb = qobject_cast<QDoubleSpinBox*>(w);
        QCheckBox *chb = qobject_cast<QCheckBox*>(w);
        QLineEdit *le = qobject_cast<QLineEdit*>(w);
        if(!(cb || sb || dsb || chb || le)) {
            parseRestoreObjectList(w->children(), dock);
        }
        QString setting = QString("parameters/%1/%2").arg(dock->objectName()).arg(w->objectName());
        if(!settings.allKeys().contains(setting))
            continue;
        if(cb)
            cb->setCurrentText(settings.value(setting).toString());
        else if(sb)
            sb->setValue(settings.value(setting).toInt());
        else if(dsb)
            dsb->setValue(settings.value(setting).toDouble());
        else if(chb)
            chb->setChecked(settings.value(setting).toBool());
        else if(le)
            le->setText(settings.value(setting).toString());
    }
}


