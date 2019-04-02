#ifndef CADBDEVICENODE_H
#define CADBDEVICENODE_H
#include "adbdevicenode.h"

class CAdbDeviceNode : public AdbDeviceNode
{
public:
    CAdbDeviceNode(const QString &serial);
};

#endif // CADBDEVICENODE_H
