#include "ztimer.h"
#include "zlog.h"
#include "zapplication.h"

ZTimer::ZTimer(QObject *parent) :
    QTimer(parent)
{
    QTimer *tm = new QTimer(this);
    connect(tm, SIGNAL(timeout()), SLOT(slot_timeout()));
#ifdef ENABLE_MULTI_FLASH
    //
#else
    tm->start(500);
#endif
}

void ZTimer::slot_timeout() {
    quint64 now = ZApplication::currentMSecsSinceEpoch();
    if(((ZApplication *)qApp)->m_ticket == 0
            || now <= ((ZApplication *)qApp)->m_ticket) {
        //DBG("ignored\n");
        return;
    }

    DBG("Ztimer real work now\n");
    ((ZApplication *)qApp)->m_ticket = 0;
    emit signal_timeout();
}
