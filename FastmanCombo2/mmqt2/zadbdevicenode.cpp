#include "zadbdevicenode.h"
#include "globalcontroller.h"

#define AGENT_BASEPORT 18001
#define ZMASTER_BASEPORT 19001
#define JDWP_BASEPORT 19201

ZAdbDeviceNode::ZAdbDeviceNode(const QString& serial)
    : logFileName("") {
    controller = GlobalController::getInstance();
    adb_serial = serial;

    zm_port = -1;
    jdwp_port = -1;
    hasRoot = false;
    hasAgg = false;

    zm_info.euid = -1;
    zm_info.sdk_int = -1;
    zm_info.sys_free = 0;
    zm_info.sys_total = 0;
    zm_info.tmp_free = 0;
    zm_info.tmp_total = 0;
    zm_info.store_free = 0;
    zm_info.store_total = 0;
}

AdbDeviceNode* AdbDeviceNode::newDeviceNode(const QString& serial, int index) {
    ZAdbDeviceNode *node = new ZAdbDeviceNode(serial);
    node->node_index = index;
    node->zm_port = ZMASTER_BASEPORT + index;
    node->jdwp_port = JDWP_BASEPORT + index;
    return node;
}

void AdbDeviceNode::delDeviceNode(AdbDeviceNode* node) {
    ZAdbDeviceNode *n = (ZAdbDeviceNode *)node;
    delete n;
}

QString ZAdbDeviceNode::getName() {
    QString devname = QString("[%1] %2")
            .arg(node_index)
            .arg(brand.isEmpty() ? adb_serial : brand+"_"+model);
    if(!sales_code.isEmpty()) {
        devname.append(QString("[%1]").arg(sales_code));
    }
    //return devname.toUpper().simplified();// csy.
    return devname.simplified();
}

int ZAdbDeviceNode::getJobsCount() {
    return jobList.size();
}

int ZAdbDeviceNode::addJobs(const QStringList &ids) {
    int ret = 0;
    jobMutex.lock();
    foreach(const QString& id, ids) {
        if(!jobList.contains(id) && !jobHistory.contains(id)) {
            ret ++;
            jobList.append(id);
        }
    }
    jobMutex.unlock();
    return ret;
}

QStringList ZAdbDeviceNode::getNextJobs() {
    QStringList ret;
    jobMutex.lock();
    ret.append(jobList);
    jobMutex.unlock();
    return ret;
}

void ZAdbDeviceNode::finishJobs(const QStringList &ids) {
    jobMutex.lock();
    foreach(const QString& id, ids) {
        if(jobList.removeOne(id)) {
            jobHistory.append(id);
        }
    }
    jobMutex.unlock();
}

void ZAdbDeviceNode::clearJobHistory() {
    jobMutex.lock();
    jobHistory.clear();
    jobMutex.unlock();
}

void ZAdbDeviceNode::clearJobsAndHistory() {
    jobMutex.lock();
    jobHistory.clear();
    jobList.clear();
    jobMutex.unlock();
}
