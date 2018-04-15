#ifndef MYCHART_H
#define MYCHART_H
#include <QChartView>
#include <QChart>
#include <QObject>

QT_CHARTS_USE_NAMESPACE

class MyChart: public QChartView
{
    Q_OBJECT
public:
    MyChart(QChart *chart, QWidget *parent = 0);
protected:
    void mouseMoveEvent(QMouseEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void resizeEvent(QResizeEvent *event);
    void keyPressEvent(QKeyEvent *event);
private:
    int lastX;
    int lastY;
    bool isDragging;
signals:
    void sizeChanged(void);
    void keyEvent(QKeyEvent *event);
};

#endif // MYCHART_H
