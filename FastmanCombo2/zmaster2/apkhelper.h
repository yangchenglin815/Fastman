#ifndef APKHELPER_H
#define APKHELPER_H

#include "zbytearray.h"
#include "zctools.h"
#include "unzip.h"

enum {
    AUTO = 0,
    INTERNAL_ONLY = 1,
    PREFER_EXTERNAL = 2
};

typedef struct APK_INFO {
    char name[128];
    char path[128];
    char md5[64];

    char pkg[128];
    unsigned versionCode;
    char versionName[128];
    unsigned installLocation;
    char sharedUid[128];

    bool hasDex;
    u64 size;
    unsigned manifestSize;
} APK_INFO;

class AXML_ATTR {
public:
    int name;
    int value;
    int type;
};

class AXML_TAG {
    static AXML_TAG *getChildTag(AXML_TAG *p, int name);
    static void clear(AXML_TAG *p);
public:
    int name;
    QList<AXML_ATTR *> attributes;
    QList<AXML_TAG *> childTags;

    AXML_TAG();
    ~AXML_TAG();

    AXML_TAG *getChildTag(int name);
    void clear();
};

class ApkHelper
{
    QList<char *> stringList;
    QList<int> resrcIdList;
    AXML_TAG rootTag;
    void print(AXML_TAG *p, int depth);

    bool parseManifest(unzFile file);
    static bool parseManifest(unzFile file, APK_INFO& info);
public:
    ApkHelper();
    ~ApkHelper();

    static bool getApkInfo(char *path, APK_INFO& info);
    static void getApkSample(ZByteArray &resp, int minXmlSize);
};

#endif // APKHELPER_H
