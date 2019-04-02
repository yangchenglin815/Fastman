#include "zapplication.h"

#ifdef _WIN32
#include <Windows.h>
#endif

#include "zlog.h"
#include "ztimer.h"

#include <QUuid>
#include <QDateTime>

quint64 ZApplication::currentMSecsSinceEpoch() {
    return QDateTime::currentDateTime().currentMSecsSinceEpoch();
}

ZApplication::ZApplication(int argc, char *argv[]) : QApplication(argc, argv)
{
    m_ticket = 0;
}

#ifdef _WIN32
bool ZApplication::winEventFilter(MSG *msg, long *result) {
    int msgType = msg->message;
    if (msgType == WM_DEVICECHANGE) {
        m_ticket = currentMSecsSinceEpoch() + 1200;
        //DBG("m_ticket = %lld\n", m_ticket);
    }
    return QApplication::winEventFilter(msg, result);
}
#endif
