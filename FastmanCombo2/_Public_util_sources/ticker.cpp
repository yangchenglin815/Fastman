#include <QtGui>

#include "ticker.h"

Ticker::Ticker(QWidget *parent)
    : QWidget(parent)
{
    offset = 0;
    timerID = 0;
}

void Ticker::setText(const QString &newText)
{
    tickerText = newText;
    update();
    updateGeometry();
}

QString Ticker::text() const
{
    return tickerText;
}

QSize Ticker::sizeHint() const
{
    return fontMetrics().size(0,text());   //fontMetrics(): Returns the font metrics for the widget's current font
}

void Ticker::paintEvent(QPaintEvent *)
{
    QPainter painter(this);

    int textWidth = fontMetrics().width(text());
    if (textWidth < 1)
        return ;
    int x = -offset;
    while (x < width()) {
        painter.drawText(x, 0, textWidth, height(),
                         Qt::AlignLeft | Qt::AlignVCenter, text());
        x += textWidth;
    }
}

void Ticker::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == timerID) {
        ++offset;
        if (offset >= fontMetrics().width(text()))
            offset = 0;
        scroll(-1, 0);
    } else {
        QWidget::timerEvent(event);
    }
}

void Ticker::showEvent(QShowEvent *)
{
    timerID = startTimer(30);   //Starts a timer and returns a timer identifier, or returns zero if it could not start a timer.
}

void Ticker::hideEvent(QHideEvent *)
{
    killTimer(timerID);
    timerID = 0;
}

