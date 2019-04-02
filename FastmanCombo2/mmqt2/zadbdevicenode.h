#ifndef ZADBDEVICENODE_H
#define ZADBDEVICENODE_H

#include <QStringList>
#include <QMutex>

#include "adbdevicenode.h"
#include "fastman2.h"

class GlobalController;
class ZAdbDeviceNode : public AdbDeviceNode
{
    GlobalController *controller;
    QMutex jobMutex;
    QStringList jobList;
    QStringList jobHistory;
public:
    QString platform;  // Android/YunOS
    QString brand;
    QString model;
    QString sales_code;
    QString lastImei;
    QString logFileName;

    ZMINFO zm_info;
    AGENT_BASIC_INFO ag_info;
    int zm_port;
    int jdwp_port;
    bool hasRoot;
    bool hasAgg;
	QString sBundleID;

    ZAdbDeviceNode(const QString& serial);
    QString getName();

    int getJobsCount();
    int addJobs(const QStringList& ids);
    QStringList getNextJobs();
    void finishJobs(const QStringList& ids);
    void clearJobHistory();
    void clearJobsAndHistory();
};

#endif // ZADBDEVICENODE_H
