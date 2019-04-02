#include "zsingleprocesshelper.h"
#include "zlocalserver.h"
#include "zlocalclient.h"
#include "zlog.h"
#ifndef NO_GUI
#include <QApplication>
#include <QWidget>
#else
#include <QCoreApplication>
#endif
#include <QByteArray>
#include <QThread>

#ifdef _WIN32
#include <cstdlib>
#include <iostream>
#include <windows.h>
#include <TlHelp32.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#endif

class SleeperThread : public QThread
{
public:
    static void msleep(unsigned long msecs)
    {
        QThread::msleep(msecs);
    }
};

ZSingleProcessHelper::ZSingleProcessHelper(const QString &serverName, const QString &holder, QObject *parent) :
    QObject(parent)
  , m_server(0)
  , m_serverName(serverName)
  , m_holder(holder)
  , m_actWin(0)
{
    m_server = new ZLocalServer(m_serverName, this);
    connect(m_server, SIGNAL(newConnection()), this, SLOT(slot_newConnection()));

#ifdef _WIN32
    m_processId = QString::number(GetCurrentProcessId());
#else
    m_processId = QString::number(getpid());
#endif
    DBG("Get current process id=<%s>, holder=<%s>\n", qPrintable(m_processId), m_holder.toLocal8Bit().data());
}

bool ZSingleProcessHelper::takeLock() {
    if (0 == m_server->listen())
        return true;
    else
        return false;
}

#ifndef NO_GUI
void ZSingleProcessHelper::setActivationWindow(QWidget *aw) {
    m_actWin = aw;
}
#endif

bool ZSingleProcessHelper::hasRemote() {
    QString holder;
    return hasRemote(m_serverName, holder);
}

bool ZSingleProcessHelper::hasRemote(QString &holder) {
    return hasRemote(m_serverName, holder);
}

bool ZSingleProcessHelper::hasRemote(const QString &name, QString &holder) {
    ZLocalClient client(name);
    do {
        if (0 != client.connect())
            break;
        if (!client.write(QString("CHECK").toLocal8Bit()))
            break;
        QByteArray ba;
        if (client.read(ba) && QString(ba).startsWith("CHECK_OK")) {
            QStringList lst = QString::fromLocal8Bit(ba).split(',');
            if (lst.size() == 2) {
                holder = lst[1];
            }
            return true;
        }
    } while(0);

    return false;
}

bool ZSingleProcessHelper::activateRemote() {
    return activateRemote(m_serverName);
}

bool ZSingleProcessHelper::activateRemote(const QString &name) {
    ZLocalClient client(name);
    do {
        if (0 != client.connect())
            break;
        if (!client.write(QString("ACTIVATE").toLocal8Bit()))
            break;
        QByteArray ba;
        if (client.read(ba) && QString(ba) == "ACTIVATE_OK")
            return true;
    } while(0);

    return false;
}

bool ZSingleProcessHelper::killRemote() {
    return killRemote(m_serverName);
}

bool ZSingleProcessHelper::killRemote(const QString &name) {
    ZLocalClient client(name);
    do {
        if (0 != client.connect())
            break;
        if (!client.write(QString("QUIT").toLocal8Bit()))
            break;
        QByteArray ba;
        QString holder;
        if (client.read(ba) && QString(ba) == "QUIT_OK") {
            for (int i = 0; i < 15; ++i) {
                SleeperThread::msleep(1000);
                if (!hasRemote(name, holder)) {
                    DBG("Kill %s success\n", name.toLocal8Bit().data());
                    return true;
                }
            }
        }
    } while(0);

    // force quit
    do {
        if (0 != client.connect())
            break;
        if (!client.write(QString("FORCEQUIT").toLocal8Bit()))
            break;
        QByteArray ba;
        QString holder;
        if (client.read(ba) && QString(ba) == "FORCEQUIT_OK") {
            for (int i = 0; i < 3; ++i) {
                SleeperThread::msleep(1000);
                if (!hasRemote(name, holder)) {
                    DBG("Kill %s success\n", name.toLocal8Bit().data());
                    return true;
                }
            }
        }
    } while(0);

    DBG("Kill %s failure\n", name.toLocal8Bit().data());
    return false;
}

void ZSingleProcessHelper::sendArguments(const QString &name) {
    ZLocalClient client(name);
    do {
        if (0 != client.connect())
            break;
        QStringList argList = qApp->arguments();
        QString dataStr = "ARGUMENTS";
        foreach (QString var, argList) {
            dataStr.append(",").append(var);
        }
        DBG("Send arguments=<%s>\n", qPrintable(dataStr));
        if (!client.write(dataStr.toLocal8Bit()))
            break;
    } while(0);
}

void ZSingleProcessHelper::slot_newConnection() {
    ZLocalServer *server = qobject_cast<ZLocalServer *>(sender());
    if (NULL == server)
        return;

    QByteArray out;
    if (server->read(out)) {
        DBG("Server read: %s\n", out.data());
        if (QString::fromLocal8Bit(out) == "CHECK") {
            QByteArray resp = "CHECK_OK,";
            if(!m_holder.isEmpty()) {
                resp.append(m_holder.toLocal8Bit());
            } else {
                resp.append(m_processId.toLocal8Bit());
            }
            server->write(resp);
        }
        else if (QString::fromLocal8Bit(out) == "ACTIVATE") {
            server->write(QString("ACTIVATE_OK").toLocal8Bit());
            activateWinodw();
        }
        else if (QString::fromLocal8Bit(out) == "QUIT") {
            server->write(QString("QUIT_OK").toLocal8Bit());
            handleQuitRequest();
        }
        else if (QString::fromLocal8Bit(out) == "FORCEQUIT") {
            server->write(QString("FORCEQUIT_OK").toLocal8Bit());
            forceQuit();
        }
        else if (QString::fromLocal8Bit(out).startsWith("ARGUMENTS")) {
            QStringList list = QString::fromLocal8Bit(out).remove(0, QString("ARGUMENTS,").size()).split(",");
            emit signal_arguments(list);
        }
    }
}

void ZSingleProcessHelper::forceQuit() {
    DBG("Force quit the program.\n");
    ulong currentPid;
#ifdef _WIN32
    currentPid = GetCurrentProcessId();
    HANDLE currentHandle = OpenProcess(PROCESS_TERMINATE, FALSE, ulong(currentPid));
    if(currentHandle != NULL) {
        TerminateProcess(currentHandle, 0);
    }
#else
    currentPid = getpid();
    kill(currentPid, SIGKILL);
#endif
}

#ifndef NO_GUI
void ZSingleProcessHelper::activateWinodw() {
    DBG("There is already a instance running, raising it up.\n");
    if (m_actWin) {
        m_actWin->setWindowState(m_actWin->windowState() & ~Qt::WindowMinimized);
        m_actWin->raise();
        m_actWin->activateWindow();
    }
}
#endif

void ZSingleProcessHelper::handleQuitRequest() {
    qApp->quit();
}
