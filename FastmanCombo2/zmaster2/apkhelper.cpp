#if !defined(_WIN32)
#include "apkhelper.h"
#include "cprocess.h"
#include "zlog.h"

#define SEGTYPE_STRINGS 0x1c0001
#define SEGTYPE_RESRCID 0x080180
#define SEGTYPE_NAMESPACE_START 0x100100
#define SEGTYPE_NAMESPACE_END   0x100101
#define SEGTYPE_TAG_START 0x100102
#define SEGTYPE_TAG_END   0x100103
#define VAL_TYPE_STRING 0x03

AXML_TAG::AXML_TAG() {
    name = -1;
}

AXML_TAG::~AXML_TAG() {
    clear(this);
}

AXML_TAG* AXML_TAG::getChildTag(AXML_TAG* p, int name) {
    AXML_TAG *q = NULL;
    if(p == NULL) {
        return NULL;
    }

    if(p->name == name) {
        return p;
    }

    for(int i=0; i<p->childTags.size(); i++) {
        if(p->childTags[i]->name == name) {
            return p->childTags[i];
        }

        q = getChildTag(p->childTags[i], name);
        if(q != NULL) {
            return q;
        }
    }
    return NULL;
}

AXML_TAG* AXML_TAG::getChildTag(int name) {
    return getChildTag(this, name);
}

void AXML_TAG::clear(AXML_TAG *p) {
    AXML_TAG *q;

    while(!p->attributes.isEmpty()) {
        delete p->attributes.takeAt(0);
    }

    while(!p->childTags.isEmpty()) {
        q = p->childTags.takeAt(0);
        clear(q);
        delete q;
    }
}

void AXML_TAG::clear() {
    clear(this);
}

#define getStr(x) (x >= 0 && x < stringList.size()) ? stringList[x] : ""

void ApkHelper::print(AXML_TAG *p, int depth) {
#if defined(CMD_DEBUG) && (!defined(__ANDROID__))
    int k = 0;
    while(k++ < depth) {
        printf("\t");
    }
    printf("<%s> ", getStr(p->name));
    for(int i=0; i<p->attributes.size(); i++) {
        AXML_ATTR *attr = p->attributes[i];
        if(attr->type == VAL_TYPE_STRING) {
            printf("%s=%s ", getStr(attr->name), getStr(attr->value));
        } else {
            printf("%s=0x%X ", getStr(attr->name), attr->value);
        }
    }
    printf("\n");
    for(int j=0; j<p->childTags.size(); j++) {
        print(p->childTags[j], depth+1);
    }
#endif
}

ApkHelper::ApkHelper() {

}

ApkHelper::~ApkHelper() {
    while (!stringList.isEmpty()) {
        free(stringList.takeAt(0));
    }
    resrcIdList.clear();
}

bool ApkHelper::parseManifest(unzFile file) {
    int n;
    char buf[4096];
    ZByteArray source(false);
    int i = 0;
    bool ret = false;
    int magic;
    int fileLen;

    rootTag.clear();
    AXML_TAG *curTag = &rootTag;
    AXML_TAG *lastTag = curTag;

    while (!stringList.isEmpty()) {
        free(stringList.takeAt(0));
    }
    resrcIdList.clear();

    if(unzOpenCurrentFilePassword(file, NULL) != UNZ_OK) {
        return false;
    }

    while((n = unzReadCurrentFile(file, buf, sizeof(buf))) > 0) {
        source.append(buf, n);
    }
    unzCloseCurrentFile(file);

    do {
        if((magic = source.getNextInt(i)) != 0x080003) {
            DBG("invalid xml magic!\n");
            break;
        }

        if((fileLen = source.getNextInt(i)) != source.length()) {
            DBG("invalid xml len!\n");
            break;
        }

        while(i < fileLen) {
            int segType = source.getNextInt(i);
            int segLen = source.getNextInt(i);
            int from = i - 8;

            if(segType == SEGTYPE_STRINGS) {
                int stringCount = source.getNextInt(i);
                int styleCount = source.getNextInt(i);
                i += 12;
                i += stringCount * 4;
                i += styleCount * 4;

                for(int j=0; j<stringCount; j++) {
                    short len = source.getNextShort(i);
                    char* str = source.getNextUtf16(i, len);
                    stringList.append(str);
                    i+=2;
                }
            } else if(segType == SEGTYPE_RESRCID) {
                int count = segLen/4 - 2;
                int id;
                while(count-- > 0) {
                    id = source.getNextInt(i);
                    resrcIdList.append(id);
                }
            } else if(segType == SEGTYPE_TAG_START) {
                i += 12;
                int name = source.getNextInt(i);
                i += 4;
                int attrCount = source.getNextInt(i);
                i += 4;
                int valueType;
                int value;

                AXML_TAG *tag = new AXML_TAG();
                tag->name = name;
                curTag->childTags.append(tag);
                lastTag = curTag;
                curTag = tag;

                while(attrCount-- > 0) {
                    i += 4;
                    name = source.getNextInt(i);
                    i += 4;
                    valueType = source.getNextInt(i);
                    value = source.getNextInt(i);
                    valueType = valueType >> 24;

                    AXML_ATTR *attr = new AXML_ATTR();
                    attr->name = name;
                    attr->value = value;
                    attr->type = valueType;
                    tag->attributes.append(attr);
                }
            } else if(segType == SEGTYPE_TAG_END) {
                curTag = lastTag;
            }

            i = from + segLen;
        }
        ret = true;
    } while(0);

    print(&rootTag, 0);
    return ret;
}

bool ApkHelper::parseManifest(unzFile file, APK_INFO &info) {
    int n;
    char buf[4096];
    ZByteArray source(false);
    int i = 0;
    bool ret = false;
    int magic;
    int fileLen;
    QList<char *> stringList;

    if(unzOpenCurrentFilePassword(file, NULL) != UNZ_OK) {
        return false;
    }

    while((n = unzReadCurrentFile(file, buf, sizeof(buf))) > 0) {
        source.append(buf, n);
    }
    unzCloseCurrentFile(file);

    do {
        if((magic = source.getNextInt(i)) != 0x080003) {
            DBG("invalid xml magic!\n");
            break;
        }

        if((fileLen = source.getNextInt(i)) != source.length()) {
            DBG("invalid xml len!\n");
            break;
        }

        while(i < fileLen) {
            int segType = source.getNextInt(i);
            int segLen = source.getNextInt(i);
            int from = i - 8;

            if(segType == SEGTYPE_STRINGS) {
                int stringCount = source.getNextInt(i);
                int styleCount = source.getNextInt(i);
                i += 12;
                i += stringCount * 4;
                i += styleCount * 4;

                for(int j=0; j<stringCount; j++) {
                    short len = source.getNextShort(i);
                    char* str = source.getNextUtf16(i, len);
                    stringList.append(str);
                    i+=2;
                }
            } else if(segType == SEGTYPE_TAG_START) {
                i += 12;
                int name = source.getNextInt(i);
                i += 4;
                int attrCount = source.getNextInt(i);
                i += 4;
                int valueType;
                int value;

                if(strcmp(getStr(name), "manifest") == 0) {
                    while(attrCount-- > 0) {
                        i += 4;
                        name = source.getNextInt(i);
                        i += 4;
                        valueType = source.getNextInt(i);
                        value = source.getNextInt(i);
                        valueType = valueType >> 24;

                        const char *tmpName = getStr(name);
                        const char *tmpValue = valueType == VAL_TYPE_STRING ? getStr(value) : "";

                        if(strcmp(tmpName, "package") == 0) {
                            strncpy(info.pkg, tmpValue, sizeof(info.pkg));
                        } else if(strcmp(tmpName, "versionCode") == 0) {
                            info.versionCode = value;
                        } else if(strcmp(tmpName, "versionName") == 0) {
                            strncpy(info.versionName, tmpValue, sizeof(info.versionName));
                        } else if(strcmp(tmpName, "installLocation") == 0) {
                            info.installLocation = value;
                        } else if(strcmp(tmpName, "sharedUserId") == 0) {
                            strncpy(info.sharedUid, tmpValue, sizeof(info.sharedUid));
                        }
                    }
                    break;
                }
            }

            i = from + segLen;
        }
        ret = true;
    } while(0);

    while(!stringList.size() > 0) {
        free(stringList.takeAt(0));
    }

    return ret;
}

bool ApkHelper::getApkInfo(char *path, APK_INFO &info) {
    DBG("getApkInfo '%s'\n", path);
    unzFile file = unzOpen(path);
    if(file == NULL) {
        DBG("cannot open as zip file!\n");
        return false;
    }

    unz_global_info global_info;
    unz_file_info file_info;
    char entry[512];
    bool hasError = false;
    bool hasArsc = false;

    info.manifestSize = 0;
    info.hasDex = false;
    info.md5[0] = 0;
    info.size = 0;

    int r = unzGetGlobalInfo(file, &global_info);
    for(int i = 0; i<global_info.number_entry; i++) {
        if((r = unzGetCurrentFileInfo(file, &file_info, entry, sizeof(entry),
                                      NULL, 0, NULL, 0)) != UNZ_OK) {
            hasError = true;
            break;
        }

        if(strcmp(entry, "AndroidManifest.xml") == 0) {
            info.manifestSize = file_info.uncompressed_size;
            if(!ApkHelper::parseManifest(file, info)) {
                hasError = true;
                break;
            }
        } else if(strcmp(entry, "resources.arsc") == 0) {
            hasArsc = true;
        } else if(strcmp(entry, "classes.dex") == 0) {
            info.hasDex = true;
        }

        if(i < global_info.number_entry - 1) {
            if((r = unzGoToNextFile(file)) != UNZ_OK) {
                hasError = true;
                break;
            }
        }
    }
    unzClose(file);

    if(info.path != path) {
        strncpy(info.path, path, sizeof(info.path));
    }
    getFileMd5(path, info.md5);
    info.size = getFileSize(path);

    if(!hasArsc || info.manifestSize == 0) {
        hasError = true;
    }

    DBG("path=%s\nmd5=%s\nhasDex=%d\nshareUid=%s\n\n",
        info.path,info.md5,info.hasDex,info.sharedUid);
    return !hasError;
}

static char *getNextAppPath(ZByteArray &out) {
    int l = out.indexOf("package:", 8);
    int r;
    char *ret;

    if(l == -1) {
        return NULL;
    }

    l+=8;
    r = out.indexOf(l, ".apk=", 5);
    if(r == -1) {
        return NULL;
    }

    r += 4;

    ret = out.getNextUtf8(l, r-l);
    out.remove(0, r);
    return ret;
}

// resp [path1][md51][path2][md52]
void ApkHelper::getApkSample(ZByteArray &resp, int minXmlSize) {
    QList<APK_INFO *> apkList;
    char *apkPath;
    int j;
    int stage = 0;
    ZByteArray out(false);
    CProcess::exec("CLASSPATH=/system/framework/pm.jar "
                   "app_process /system/bin com.android.commands.pm.Pm "
                   "list packages -f", out);

    while((apkPath = getNextAppPath(out)) != NULL) {
        APK_INFO *apk = new APK_INFO();
        strncpy(apk->path, apkPath, sizeof(apk->path));
        apk->size = getFileSize(apkPath);
        free(apkPath);

        for(j=0; j<apkList.size(); j++) {
            if(apkList[j]->size > apk->size) {
                break;
            }
        }
        apkList.insert(j, apk);
    }

    for(j=0; j<apkList.size(); j++) {
        APK_INFO *apk = apkList[j];
        if(apk->size > 5*1024*1024) {
            break;
        }

        if(!getApkInfo(apk->path, *apk)) {
            continue;
        }

        if(apk->manifestSize < minXmlSize) {
            continue;
        }

        if(strcmp(apk->sharedUid, "android.uid.system") != 0) {
            continue;
        }

        if(stage == 0) {
            DBG("stage0 sample '%s'\n", apk->path);
            resp.putUtf8(apk->path);
            resp.putUtf8(apk->md5);
            stage = 1;
        }

        if(stage == 1 && apk->hasDex) {
            DBG("stage1 sample '%s'\n", apk->path);
            resp.putUtf8(apk->path);
            resp.putUtf8(apk->md5);
            stage = 2;
            break;
        }
    }

    if(stage < 1) {
        resp.putUtf8("error");
        resp.putUtf8("error");
        resp.putUtf8("error");
        resp.putUtf8("error");
    } else if(stage < 2) {
        resp.putUtf8("error");
        resp.putUtf8("error");
    }

    while(!apkList.isEmpty()) {
        delete apkList.takeAt(0);
    }
}
#endif
