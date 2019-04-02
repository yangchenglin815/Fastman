#ifndef ZTIMER_H
#define ZTIMER_H

#include <QTimer>

class ZTimer : public QTimer
{
    Q_OBJECT

public:
    explicit ZTimer(QObject *parent = 0);

private slots:
    void slot_timeout();

signals:
    void signal_timeout();
};

#endif // ZTIMER_H
