#include "zmsg2.h"
#include "zlog.h"

ZMsg2::ZMsg2() :
    data(false)
{
    ver = ZMSG2_VERSION;
    cmd = 0;
}

// [int magic][int len]{[short ver][short cmd][var data ... ]}[short crc]
bool ZMsg2::parse(ZByteArray &source) {
    if (source.length() < 14) {
        DBG("len %d too short!\n", source.length());
        return false;
    }

    int i = 0;
    int magic = source.getNextInt(i);
    int len = source.getNextInt(i);
    if (magic != ZMSG2_MAGIC) {
        DBG("invalid magic!\n");

        magic = ZMSG2_MAGIC;
        i = source.indexOf(&magic, sizeof(magic));
        if(i != -1) {
            source.remove(0, i);
        } else {
            source.clear();
        }
        return false;
    }
    if(source.length() < len + 10) {
        DBG("invalid length!\n");
        return false;
    }

    ver = source.getNextShort(i);
    cmd = source.getNextShort(i);

    data.clear();
    data.append(source.data()+12, len-4);

    i = len + 8;
    u16 crc1 = source.getNextShort(i);
    u16 crc2 = source.checksum(8, len);
    if (crc1 != crc2) {
        DBG("invalid crc32!\n");
        return false;
    }

    //DBG("recv:\n");
    //DBG_HEX(source.data(), len + 10);
    source.remove(0, len + 10);
    return true;
}

ZByteArray ZMsg2::getPacket() {
    ZByteArray p(false);
    int len = 4 + data.length();
    p.putInt(ZMSG2_MAGIC);
    p.putInt(len);
    p.putShort(ver);
    p.putShort(cmd);
    p.append(data);
    short crc = p.checksum(8, len);
    p.putShort(crc);
    //DBG("send:\n");
    //DBG_HEX(p.data(), p.size());
    return p;
}
