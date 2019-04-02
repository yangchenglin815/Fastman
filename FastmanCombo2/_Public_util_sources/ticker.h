#ifndef TICKER_H
#define TICKER_H

#include <QWidget>

class Ticker : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText)

public:
    Ticker(QWidget *parent = 0);

    void setText(const QString &newText);
    QString text() const;
    QSize sizeHint() const;

protected:
    void paintEvent(QPaintEvent *event);
    void timerEvent(QTimerEvent *evnet);
    void showEvent(QShowEvent *event);
    void hideEvent(QHideEvent *event);

private:
    QString tickerText;
    int offset;
    int timerID;
};

#endif // TICKER_H
