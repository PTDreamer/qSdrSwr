#include "mychart.h"
#include <QDebug>

MyChart::MyChart(QChart *chart, QWidget *parent): QChartView(chart, parent), isDragging(false)
{

}

void MyChart::mouseMoveEvent(QMouseEvent *event)
{
    if((event->modifiers() & Qt::ControlModifier) && isDragging) {
        chart()->scroll(lastX - event->x(), event->y() - lastY);
        lastX = event->x();
        lastY = event->y();
        emit sizeChanged();
        event->accept();
    }
    else if(event->modifiers() & Qt::ShiftModifier)
        QChartView::mouseMoveEvent(event);
    else
        QGraphicsView::mouseMoveEvent(event);
}

void MyChart::mousePressEvent(QMouseEvent *event)
{
    if((event->modifiers() & Qt::ControlModifier) &&(event->button() == Qt::LeftButton)) {
       isDragging = true;
       lastX = event->x();
       lastY = event->y();
       event->accept();
    }
    else if(event->modifiers() & Qt::ShiftModifier)
        QChartView::mousePressEvent(event);
    else
        QGraphicsView::mousePressEvent(event);
}

void MyChart::mouseReleaseEvent(QMouseEvent *event)
{
    isDragging = false;
    if(event->modifiers() & Qt::ShiftModifier)
        QChartView::mouseReleaseEvent(event);
    else
        QGraphicsView::mouseReleaseEvent(event);
    emit sizeChanged();
}

void MyChart::resizeEvent(QResizeEvent *event)
{
    QChartView::resizeEvent(event);
    emit sizeChanged();
}

void MyChart::keyPressEvent(QKeyEvent *event)
{
    emit keyEvent(event);
    QChartView::keyPressEvent(event);
}
