#include "trackermain.h"
#include "adbtracker.h"
#include "adbdevicenode.h"
#include "zsingleprocesshelper.h"
#include "zlog.h"
#include <QProcess>
#include <QApplication>

extern const QString SERVERNAME_MMQT2;

TrackerMain::TrackerMain(QObject *parent)
    : QObject(parent)
    , m_adbTracker(NULL) {
    iniTracker();
}

TrackerMain::~TrackerMain() {
    m_adbTracker->stop();
    m_adbTracker->freeMem();
}

void TrackerMain::iniTracker() {
    m_adbTracker = new AdbTracker("zxlycon", this);
    connect(m_adbTracker, SIGNAL(sigDevListChanged()),
            this, SLOT(slot_onDevListChanged()));
    connect(m_adbTracker, SIGNAL(sigAdbError(QString)),
            this, SLOT(slot_onAdbError(QString)));
    m_adbTracker->start();
}

void TrackerMain::slot_onDevListChanged() {
    DBG("Devices changed.\n");
    m_devices = m_adbTracker->getDeviceList();
    QString holder;
    foreach (AdbDeviceNode *node, m_devices) {
        if (node->connect_statname == "device") {
            if (ZSingleProcessHelper::hasRemote(SERVERNAME_MMQT2, holder)) {
                if (ZSingleProcessHelper::activateRemote(SERVERNAME_MMQT2)) {
                    DBG("Active window.\n");
                }
            } else {
                DBG("Launch mmqt2...\n");
                QProcess *ps = new QProcess(this);
                QString program;

#ifdef _WIN32
                program = qApp->applicationDirPath() + "/mmqt2.exe";
#else
                program = qApp->applicationDirPath() + "/mmqt2";
#endif
                ps->startDetached(program);
                if (!ps->waitForStarted(3000)) {
                    DBG("Start mmqt2 failed.\n");
                }
            }
            break;
        }
    }
}

void TrackerMain::slot_onAdbError(const QString &hint) {
    DBG("%s", hint.toLocal8Bit().data());
}

