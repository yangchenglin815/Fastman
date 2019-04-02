#include "cadbdevicenode.h"
#include "adbdevicenode.h"

CAdbDeviceNode::CAdbDeviceNode(const QString &serial)
{
    adb_serial = serial;
}

AdbDeviceNode* AdbDeviceNode::newDeviceNode(const QString& serial, int index) {
    CAdbDeviceNode *node = new CAdbDeviceNode(serial);
    node->node_index = index;

    return node;
}

void AdbDeviceNode::delDeviceNode(AdbDeviceNode* node) {
    CAdbDeviceNode *n = (CAdbDeviceNode *)node;
    delete n;
}
