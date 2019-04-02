#ifdef _WIN32

#include "driverhelper.h"
#include "zlog.h"

#include <QFile>
#include <QDir>
#include <QProcess>
#include <QCryptographicHash>
#include <QDomDocument>
#include <QDomElement>
#include <QUuid>
#include <QUrl>
#include <QVariantMap>
#include <QApplication>

#include "zsettings.h"
#include "ziphelper.h"
#include "zdm2/zdmhttp.h"
#include "zdm2/zdmhttpex.h"
#include "qjson/parser.h"
#include "unzip.h"
#include "winverifytrust.h"

#ifdef USE_DDK
#include "usbview/usbview.h"
#endif

QString g_tmpDriverPath;
static void rmdir_recursively(const QString& path);

DriverDeviceNode::DriverDeviceNode() {
    status = 0;
    problem = 0;
}

static QString getFileMd5(const QString& path) {
    QCryptographicHash md5(QCryptographicHash::Md5);
    QString ret;
    QFile f(path);
    char buf[4096];
    int n;
    if(f.open(QIODevice::ReadOnly)) {
        while((n = f.read(buf, sizeof(buf))) > 0) {
            md5.addData(buf, n);
        }
        f.close();
        ret = md5.result().toHex();
    } else {
        //DBG("cannot open '%s'!\n", path.toLocal8Bit().data());
    }
    return ret;
}

static BOOL IsWindows64() {
    typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
    LPFN_ISWOW64PROCESS fnIsWow64Process = (LPFN_ISWOW64PROCESS)::GetProcAddress(GetModuleHandle("kernel32"), "IsWow64Process");
    BOOL bIsWow64 = FALSE;
    if (fnIsWow64Process)
        if (!fnIsWow64Process(::GetCurrentProcess(), &bIsWow64))
            bIsWow64 = FALSE;
    return bIsWow64;
}

static QString getRandomName() {
    QString uuid = QUuid::createUuid().toString();
    uuid.remove(QRegExp("\\W"));
    //DBG("%s\n", uuid.toLocal8Bit().data());
    return uuid;
}

WinOSInfo::WinOSInfo() {
    isWow64 = false;
    platform = 0;
    majorVer = 0;
    minorVer = 0;
    buildNum = 0;
}

bool WinOSInfo::detect() {
    isWow64 = IsWindows64();

    OSVERSIONINFOEX OS;
    OS.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

    BOOL VerEx = GetVersionEx((OSVERSIONINFO*)&OS);
    if(!VerEx) {
        //DBG("getVersion failed\n");
        return false;
    }

    platform = OS.dwPlatformId;
    majorVer = OS.dwMajorVersion;
    minorVer = OS.dwMinorVersion;
    buildNum = OS.dwBuildNumber;

    // XP 下使用统一版本驱动程序
    if (platform == 2 && majorVer == 5) {
        minorVer = 1;
        buildNum = 2601;
    }

    //DBG("getVersion platform: %d ver: %d.%d-%d\n", platform, majorVer, minorVer, buildNum);
    return true;
}

QByteArray DriverInfo::toXml(WinOSInfo& osInfo) {
    QDomDocument doc;
    QDomElement root = doc.createElement("drv_config");
    doc.appendChild(root);

    QDomElement host = doc.createElement("host");
    host.setAttribute("isWow64", osInfo.isWow64 ? 1 : 0);
    host.setAttribute("platform", osInfo.platform);
    host.setAttribute("majorVer", osInfo.majorVer);
    host.setAttribute("minorVer", osInfo.minorVer);
    host.setAttribute("buildNum", osInfo.buildNum);
    root.appendChild(host);

    QDomElement desc = doc.createElement("desc");
    int n = infPath.lastIndexOf('/');
    QString inf = n == -1 ? infPath : infPath.mid(n+1);

    desc.setAttribute("inf", inf);
    desc.setAttribute("name", name);
    desc.setAttribute("id", device_id);
    desc.setAttribute("ver", drvVersion);
    desc.setAttribute("vid", vid);
    desc.setAttribute("pid", pid);
    desc.setAttribute("signed", bsigned);

    root.appendChild(desc);

    return doc.toByteArray(4);
}

bool DriverInfo::fromXml(const QByteArray &data, WinOSInfo &osInfo) {
    QDomDocument doc;
    if(!doc.setContent(data)) {
        //DBG("xml parse error\n");
        return false;
    }

    QDomNodeList nodes = doc.elementsByTagName("host");
    if(nodes.size() < 1) {
        //DBG("xml parse error\n");
        return false;
    }

    QDomElement node = nodes.at(0).toElement();


    osInfo.isWow64 = node.attribute("isWow64").toInt() == 1;
    osInfo.platform = node.attribute("platform").toInt();
    osInfo.majorVer = node.attribute("majorVer").toInt();
    osInfo.minorVer = node.attribute("minorVer").toInt();
    osInfo.buildNum = node.attribute("buildNum").toInt();

    nodes = doc.elementsByTagName("desc");
    if(nodes.size() < 1) {
        //DBG("xml parse error\n");
        return false;
    }

    node = nodes.at(0).toElement();
    infPath = node.attribute("inf");
    name = node.attribute("name");
    device_id = node.attribute("id");
    drvVersion = node.attribute("ver");
    vid = node.attribute("vid");
    pid = node.attribute("pid");
    bsigned = node.attribute("signed").toInt();

    //DBG("parse xml done.\n");
    return true;
}

typedef BOOLEAN (WINAPI* pWow64EnableWow64FsRedirection)( BOOLEAN Wow64FsEnableRedirection );
typedef BOOL (WINAPI* pWow64DisableWow64FsRedirection)( PVOID *OldValue );
typedef BOOL (WINAPI* pWow64RevertWow64FsRedirection)( PVOID OldValue );
static bool wow64QFileOpen(QFile& f, const QString& file, QIODevice::OpenModeFlag flag) {
    bool ret = false;

    pWow64EnableWow64FsRedirection funcWow64EnableWow64FsRedirection = (pWow64EnableWow64FsRedirection)
            GetProcAddress( GetModuleHandle(TEXT("kernel32")), "Wow64EnableWow64FsRedirection");
    if(funcWow64EnableWow64FsRedirection != NULL) {
        //DBG("wow64QFileOpen mode wow64\n");
        funcWow64EnableWow64FsRedirection(FALSE);
        f.setFileName(file);
        ret = f.open(flag);
        funcWow64EnableWow64FsRedirection(TRUE);
        return ret;
    }

    pWow64DisableWow64FsRedirection funcWow64DisableWow64FsRedirection = (pWow64DisableWow64FsRedirection)
            GetProcAddress( GetModuleHandle(TEXT("kernel32")), "Wow64DisableWow64FsRedirection" );
    pWow64RevertWow64FsRedirection funcWow64RevertWow64FsRedirection = (pWow64RevertWow64FsRedirection)
            GetProcAddress( GetModuleHandle(TEXT("kernel32")), "Wow64RevertWow64FsRedirection" );
    if(funcWow64DisableWow64FsRedirection != NULL && funcWow64RevertWow64FsRedirection != NULL) {
        //DBG("wow64QFileOpen mode vista\n");
        PVOID oldVal = NULL;
        funcWow64DisableWow64FsRedirection(&oldVal);
        f.setFileName(file);
        ret = f.open(flag);
        funcWow64RevertWow64FsRedirection(oldVal);
        return ret;
    }

    //DBG("wow64QFileOpen mode x86\n");
    f.setFileName(file);
    ret = f.open(flag);
    return ret;
}

bool DriverHelper::toZip(DriverInfo &info, const QString &path, QString& errmsg) {
    bool ret = true;
    ZipHelper h;

    int progress = 0;
    int total = info.files.size() + info.filesInWindir.size();

    foreach(const QString& file, info.files) {
        QFile f(file);
        if(!f.open(QIODevice::ReadOnly)) {
            errmsg = tr("cannot open %1").arg(file);
            ret = false;
            break;
        }

        QString entryName = file.mid(info.drvPath.length()+1);
        //DBG("entry '%s'\n", entryName.toLocal8Bit().data());

        ZipEntry *entry = new ZipEntry();
        entry->name = entryName;
        entry->data = f.readAll();
        f.close();

        h.entries.append(entry);
        emit signal_progress(++progress, total);
    }

    foreach(const QString& file, info.filesInWindir) {
        bool duplicate = false;
        QFile f;
        if(!wow64QFileOpen(f, file, QIODevice::ReadOnly)) {
            errmsg = tr("cannot open %1").arg(file);
            ret = false;
            break;
        }

        int n = file.lastIndexOf('\\');
        QString entryName = n != -1 ? file.mid(n+1) : file;

        QString dupName;
        foreach(ZipEntry *dupEntry, h.entries) {
            n = dupEntry->name.lastIndexOf('/');
            if(n != -1) {
                dupName = dupEntry->name.mid(n+1);
            } else {
                dupName = dupEntry->name;
            }

            if(dupName == entryName) {
                //DBG("found duplicate entry '%s'\n", dupEntry->name.toLocal8Bit().data());
                duplicate = true;
#if 0 //def CMD_DEBUG
                ZipEntry *entry = new ZipEntry();
                entry->name = dupEntry->name+"_dup";
                entry->data = f.readAll();
                h.entries.append(entry);
#endif
                break;
            }
        }

        //DBG("[%s] duplicate = %d\n", entryName.toLocal8Bit().data(), duplicate);
        if(!duplicate) {
            //DBG("add new entry from winDir '%s'\n", entryName.toLocal8Bit().data());
            ZipEntry *entry = new ZipEntry();
            entry->name = QString(osInfo.isWow64 ? "amd64/" : "i386/") + entryName;
#if 0 //def CMD_DEBUG
            entry->name.append("_new");
#endif
            entry->data = f.readAll();
            h.entries.append(entry);
        }
        f.close();
        emit signal_progress(++progress, total);
    }

    if(!ret) {
        return false;
    }

    ZipEntry *xmlEntry = new ZipEntry();
    xmlEntry->name = "dxdrv.xml";
    xmlEntry->data = info.toXml(osInfo);
    h.entries.append(xmlEntry);
    signal_progress(0, 0);
    return h.writeZipFile(path);
}

bool DriverHelper::uninstallDrv(DriverDeviceNode *node, DriverInfo &info, QString &errmsg, bool deleteInf) {
    QString basename = info.oemFile;
    int n = basename.lastIndexOf('\\');
    if(n != -1) {
        basename.remove(0, n+1);
    }

    //DBG("remove [%s]\n", qPrintable(node->device_id_long));
    QProcess p;
    QStringList args;
    int r = -1;
    QByteArray out;
    bool ret = false;

    args << "remove" << "@"+node->device_id_long;
    p.start(devcon, args);
    do {
        if(!p.waitForStarted(2000)) {
            errmsg = tr("start devcon failed!");
            break;
        }

        if(!p.waitForFinished()) {
            errmsg = tr("finish devcon failed!");
            p.kill();
            break;
        }

        r = p.exitCode();
        out = p.readAll();
        printf("\n\nexitCode=%d\n[%s]", r, out.data());
    } while(0);

    if(!deleteInf) {
        switch(r) {
        case 0:
            ret = true;
            break;
        case 1:
            ret = true;
            errmsg = tr("Reboot required");
            break;
        default:
            ret = false;
            errmsg = QString::fromLocal8Bit(out);
            break;
        }
        return ret;
    }

    if(info.oemFile.isEmpty() || info.oemFile.endsWith("usb.inf")) {
        errmsg = tr("no oem file found");
        return false;
    }

    //DBG("dp_delete [%s]\n", qPrintable(basename));
    args.clear();
    args << "-f" << "dp_delete" << basename;
    p.start(devcon, args);
    do {
        if(!p.waitForStarted(2000)) {
            errmsg = tr("start devcon failed!");
            break;
        }

        if(!p.waitForFinished()) {
            errmsg = tr("finish devcon failed!");
            p.kill();
            break;
        }

        r = p.exitCode();
        out = p.readAll();
        printf("\n\nexitCode=%d\n[%s]", r, out.data());
        switch(r) {
        case 0:
            ret = true;
            break;
        case 1:
            ret = true;
            errmsg = tr("Reboot required");
            break;
        default:
            ret = false;
            errmsg = QString::fromLocal8Bit(out);
            break;
        }
    } while(0);

    return ret;
}

QString DriverHelper::getZipFileSum(const QString& path) {
    unzFile file = unzOpen64(path.toLocal8Bit().data(), 0);
    if(file == NULL) {
        //DBG("unzOpen fail!\n");
        QString sum = QUuid::createUuid().toString();
		sum = "";
        return sum;
    }

    QCryptographicHash md5(QCryptographicHash::Md5);
    QMap<QString, uint> crcMap;

    bool ret = true;
    unz_global_info global_info;
    unz_file_info file_info;
    char entry[512];
    QString entryName;

    int r = unzGetGlobalInfo(file, &global_info);
    for(uLong i=0; i<global_info.number_entry; i++) {
        if((r = unzGetCurrentFileInfo(file, &file_info, entry, sizeof(entry), NULL, 0, NULL, 0)) != UNZ_OK) {
            //DBG("unzGetCurrentFileInfo fail!\n");
            ret = false;
            break;
        }

        entryName = QString::fromLocal8Bit(entry);
        if(!entryName.endsWith('/') && !entryName.endsWith('\\')
                && entryName != "dxdrv.xml") {
            crcMap.insert(entryName, file_info.crc);
        }

        if(i < global_info.number_entry - 1) {
            if((r = unzGoToNextFile(file)) != UNZ_OK) {
                //DBG("unzGoToNextFile fail!\n");
                ret = false;
                break;
            }
        }
    }
    unzClose(file);

    if(!ret) {
        QString sum = QUuid::createUuid().toString();
		sum = "";
        return sum;
    }

    QStringList entryList = crcMap.keys();
    entryList.sort();
    foreach(const QString& en, entryList) {
        uint crc = crcMap.value(en);
        md5.addData((const char *) &crc, sizeof(crc));
    }
    return md5.result().toHex();
}

bool DriverHelper::loadUsbDB(QTextStream &stream) {
    QString line;
    dbMutex.lock();
    while(!dbList.isEmpty()) {
        delete dbList.takeFirst();
    }

    QString vid;
    QString pid;
    QString brand;
    QString model;

    int i=0;
    for(;;) {
        line = stream.readLine(256);
        if(line.isNull()) {
            break;
        }
        ++i;

        if(line.isEmpty() || line.startsWith('#')) {
            continue;
        }

        // C_ AT_ HID_ R_ PHY_
        if(line[1] == ' ' || line[2] == ' ' || line[3] == ' ') {
            //DBG("break @ line %d\n", i);
            break;
        }

        if(line[4] == ' ' && line[5] == ' ') {
            vid = line.mid(0, 4);
            brand = line.mid(6);
        }

        if(line[5] == ' ' && line[6] == ' ') {
            pid = line.mid(1, 4);
            model = line.mid(7);

            UsbDbInfo *info = new UsbDbInfo();
            info->vid = vid;
            info->pid = pid;
            info->brand = brand;
            info->model = model;
            dbList.append(info);
        }
    }

    dbMutex.unlock();
    //DBG("got dbList.size = %d\n", dbList.size());
    return true;
}

bool DriverHelper::loadUsbDB(const QString &path) {
    bool ret = false;
    QFile f(path);
    if(f.open(QIODevice::ReadOnly)) {
        QTextStream s(&f);
        ret = loadUsbDB(s);
        f.close();
    }
    return ret;
}

bool DriverHelper::getNameInUsbDB(const QString &vid, const QString &pid, QString &brand, QString &model) {
    bool ret = false;
    dbMutex.lock();
    foreach(UsbDbInfo *info, dbList) {
        if(info->vid.toUpper() == vid.toUpper()) {
            brand = info->brand;
            if(info->pid.toUpper() == pid.toUpper()) {
                model = info->model;
                ret = true;
                break;
            }
        }
    }
    dbMutex.unlock();
    return ret;
}

bool DriverHelper::uninstallDriver(const QString &path, QString &errmsg) {
    QStringList infList;
    QStringList infDirList;

    // walk through
    QStringList dirList;
    dirList.append(path);
    for(int i=0; i<dirList.size(); i++) {
        QString dirPath = dirList[i];
        QDir dir(dirPath);
        QFileInfoList infoList = dir.entryInfoList(QDir::AllEntries|QDir::NoDotAndDotDot);
        foreach(const QFileInfo& info, infoList) {
            QString subPath = info.canonicalFilePath();
            if(info.isDir()) {
                dirList.append(subPath);
            } else if(subPath.endsWith(".inf", Qt::CaseInsensitive)) {
                infList.append(subPath);

                if(!infDirList.contains(dirPath)) {
                    infDirList.append(dirPath);
                }
            }
        }
    }
    //DBG("infList.size = %d, infDirList.size = %d\n", infList.size(), infDirList.size());

    QProcess p;
    QStringList args;
    unsigned int r;
    QByteArray out;

    // remove
    foreach(const QString& inf, infList) {
        QString infNative = inf;
        infNative.replace('/', '\\');

        args.clear();
        args << "/U" << infNative << "/SW" << "/D";
        p.start(dpinst, args);

        if(p.waitForStarted(2000) && p.waitForFinished()) {
            r = p.exitCode();
            out = p.readAllStandardError() + p.readAllStandardOutput();
            //DBG("[Uninstall '%s'] = 0x%08x, <%s>\n", qPrintable(infNative), r, out.data());
        } else {
            errmsg = tr("Uninstall for %1 failed").arg(infNative);
            //DBG("[Uninstall '%s'] exec failed\n", qPrintable(infNative));
            return false;
        }
    }

    //DBG("uninstallDriver done.\n");
    return true;
}

bool DriverHelper::installDriver(const QString &path, QString &errmsg) {
    QStringList infList;
    QStringList infDirList;

    // walk through
    QStringList dirList;
    dirList.append(path);
    for(int i=0; i<dirList.size(); i++) {
        QString dirPath = dirList[i];
        QDir dir(dirPath);
        QFileInfoList infoList = dir.entryInfoList(QDir::AllEntries|QDir::NoDotAndDotDot);
        foreach(const QFileInfo& info, infoList) {
            QString subPath = info.canonicalFilePath();
            if(info.isDir()) {
                dirList.append(subPath);
            } else if(subPath.endsWith(".inf", Qt::CaseInsensitive)) {
                infList.append(subPath);

                if(!infDirList.contains(dirPath)) {
                    infDirList.append(dirPath);
                }
            }
        }
    }
    //DBG("infList.size = %d, infDirList.size = %d\n", infList.size(), infDirList.size());

    QProcess p;
    QStringList args;
    unsigned int r;
    QByteArray out;

    // install
    foreach(const QString& infDir, infDirList) {
        QString infDirNative = infDir;
        infDirNative.replace('/', '\\');
        //DBG("[Install '%s']\n", qPrintable(infDirNative));

        args.clear();
        args << "/PATH" << infDirNative << "/F" << "/LM" << "/SW" << "/SA" << "/A";
        p.start(dpinst, args);

        if(p.waitForStarted(2000) && p.waitForFinished()) {
            r = p.exitCode();
            out = p.readAllStandardError() + p.readAllStandardOutput();
            //DBG("[Install '%s'] = 0x%08x, <%s>\n", qPrintable(infDirNative), r, out.data());
        } else {
            errmsg = tr("Install for %1 failed").arg(infDirNative);
            //DBG("[Install '%s'] exec failed\n", qPrintable(infDirNative));
            return false;
        }
    }

    //DBG("installDriver done.\n");
    return true;
}

QString DriverHelper::getDriverDirCustomName(const QString &dir) {
    ZSettings ini(dir+"/info.ini");
    QString name = ini.value("name", dir);
    return name;
}

QString DriverHelper::getDriverDirPlatformName(const QString& dir)
{
    ZSettings ini(dir+"/info.ini");
    QString name = ini.value("Platform", dir);
    return name;
}

QString DriverHelper::getDriverInfCustomName(const QString &inf) {
    QString dir = inf;
    int len = dir.length();
    int n = dir.lastIndexOf('/');
    if(n != -1) {
        dir.remove(n, len-n);
    }
    //DBG("dir=>'%s'\n", qPrintable(dir));
    return getDriverDirCustomName(dir);
}

bool DriverHelper::refreshDriver(const QStringList &infList, const QStringList& infDirList, QString& errmsg) {
#if 0
    QStringList infList;
    QStringList infDirList;

    // walk through
    QStringList dirList;
    dirList.append(path);
    for(int i=0; i<dirList.size(); i++) {
        QString dirPath = dirList[i];
        QDir dir(dirPath);
        QFileInfoList infoList = dir.entryInfoList(QDir::AllEntries|QDir::NoDotAndDotDot);
        foreach(const QFileInfo& info, infoList) {
            QString subPath = info.canonicalFilePath();
            if(info.isDir()) {
                dirList.append(subPath);
            } else if(subPath.endsWith(".inf", Qt::CaseInsensitive)) {
                infList.append(subPath);

                if(!infDirList.contains(dirPath)) {
                    infDirList.append(dirPath);
                }
            }
        }
    }
#endif
    //DBG("infList.size = %d, infDirList.size = %d\n", infList.size(), infDirList.size());

    QProcess p;
    QStringList args;
    unsigned int r;
    QByteArray out;

    // remove
    foreach(const QString& inf, infList) {
        QString infNative = inf;
        infNative.replace('/', '\\');

        args.clear();
        args << "/U" << infNative << "/SW" << "/D";
        p.start(dpinst, args);

        emit signal_addLog(tr("Removing drivers from [%1]").arg(getDriverInfCustomName(inf)));
        if(p.waitForStarted(2000) && p.waitForFinished()) {
            r = p.exitCode();
            out = p.readAllStandardError() + p.readAllStandardOutput();
            //DBG("[Uninstall '%s'] = 0x%08x, <%s>\n", qPrintable(infNative), r, out.data());
        } else {
            errmsg = tr("Uninstall for %1 failed").arg(infNative);
            //DBG("[Uninstall '%s'] exec failed\n", qPrintable(infNative));
            return false;
        }
    }

    // install	
    foreach(const QString& infDir, infDirList) {
        QString infDirNative = infDir;
        infDirNative.replace('/', '\\');
        //DBG("[Install '%s']\n", qPrintable(infDirNative));

        args.clear();
        args << "/PATH" << infDirNative << "/F" << "/LM" << "/SW" << "/SA" << "/A";
        p.start(dpinst, args);

        emit signal_addLog(tr("Installing drivers from [%1]").arg(getDriverDirCustomName(infDir)));
		
        if(p.waitForStarted(2000) && p.waitForFinished()) {
            r = p.exitCode();
            out = p.readAllStandardError() + p.readAllStandardOutput();
            //DBG("[Install '%s'] = 0x%08x, <%s>\n", qPrintable(infDirNative), r, out.data());
        } else {
            errmsg = tr("Install for %1 failed").arg(infDirNative);
            //DBG("[Install '%s'] exec failed\n", qPrintable(infDirNative));
            return false;
        }		
    }
	
    //DBG("refresh driver done.\n");
    return true;
}

bool DriverHelper::refreshDriverYun(const QStringList &infList, const QStringList &infDirList, QString &errmsg){ // 云驱动文件，不从本地安装，而是从服务器获取
	    //DBG("infList.size = %d, infDirList.size = %d\n", infList.size(), infDirList.size());
    QProcess p;
    QStringList args;
    unsigned int r;
    QByteArray out;
	QString strDeviceID = "";
	QString strDeviceName = "";
	int tmp = 0;

    // remove
    foreach(const QString& inf, infList) {
        QString infNative = inf;
        infNative.replace('/', '\\');
		// 这里改为从本地删除
        //args.clear();
        //args << "/U" << infNative << "/SW" << "/D";
        //p.start(dpinst, args);
        
		GetDeviceID_Name(infNative,strDeviceID,strDeviceName);
		//根据ID 和 Name 获取 驱动信息
		InstallYunDevice(infNative, strDeviceID, strDeviceName);
		tmp = 1;

		if((infNative.contains("drivers_both\\Qualcomm"))||(infNative.contains("drivers_both\\8085Q"))){
		  // 这里改为从本地删除
          args.clear();
          args << "/U" << infNative << "/SW" << "/D";
          p.start(dpinst, args);
 
          emit signal_addLog(tr("Removing drivers from [%1]").arg(getDriverInfCustomName(inf)));
          if(p.waitForStarted(2000) && p.waitForFinished()) {
            r = p.exitCode();
            out = p.readAllStandardError() + p.readAllStandardOutput();
            DBG("[Uninstall '%s'] = 0x%08x, <%s>\n", qPrintable(infNative), r, out.data());
          } else {
            errmsg = tr("Uninstall for %1 failed").arg(infNative);
            DBG("[Uninstall '%s'] exec failed\n", qPrintable(infNative));
            //return false;
          }
		}


//	}else if(infNative.contains("drivers_both\\8085Q")){


//	} 

		// 这里改为从本地删除
        //args.clear();
        //args << "/U" << infNative << "/SW" << "/D";
        //p.start(dpinst, args);
/*
        emit signal_addLog(tr("Removing drivers from [%1]").arg(getDriverInfCustomName(inf)));
        if(p.waitForStarted(2000) && p.waitForFinished()) {
            r = p.exitCode();
            out = p.readAllStandardError() + p.readAllStandardOutput();
            //DBG("[Uninstall '%s'] = 0x%08x, <%s>\n", qPrintable(infNative), r, out.data());
        } else {
            errmsg = tr("Uninstall for %1 failed").arg(infNative);
            //DBG("[Uninstall '%s'] exec failed\n", qPrintable(infNative));
            return false;
        }
*/
    }

    // install	
    foreach(const QString& infDir, infDirList) {
        QString infDirNative = infDir;
        infDirNative.replace('/', '\\');
        //DBG("[Install '%s']\n", qPrintable(infDirNative));
		//这里改为从本地安装
       // args.clear();
       // args << "/PATH" << infDirNative << "/F" << "/LM" << "/SW" << "/SA" << "/A";
       // p.start(dpinst, args);

		//GetDeviceID_Name(infDirNative, strDeviceID, strDeviceName);
 		//InstallYunDevice(infDirNative, strDeviceID, strDeviceName);

		if((infDirNative.contains("drivers_both\\Qualcomm"))||(infDirNative.contains("drivers_both\\8085Q"))){
           args.clear();
           args << "/PATH" << infDirNative << "/F" << "/LM" << "/SW" << "/SA" << "/A";
           p.start(dpinst, args);

		   emit signal_addLog(tr("Installing drivers from [%1]").arg(getDriverDirCustomName(infDir)));
		
           if(p.waitForStarted(2000) && p.waitForFinished()) {
            r = p.exitCode();
            out = p.readAllStandardError() + p.readAllStandardOutput();
            DBG("[Install '%s'] = 0x%08x, <%s>\n", qPrintable(infDirNative), r, out.data());
            } else {
            errmsg = tr("Install for %1 failed").arg(infDirNative);
            DBG("[Install '%s'] exec failed\n", qPrintable(infDirNative));
            // return false;
         }

		}
/*
        emit signal_addLog(tr("Installing drivers from [%1]").arg(getDriverDirCustomName(infDir)));
		
        if(p.waitForStarted(2000) && p.waitForFinished()) {
            r = p.exitCode();
            out = p.readAllStandardError() + p.readAllStandardOutput();
            //DBG("[Install '%s'] = 0x%08x, <%s>\n", qPrintable(infDirNative), r, out.data());
        } else {
            errmsg = tr("Install for %1 failed").arg(infDirNative);
            //DBG("[Install '%s'] exec failed\n", qPrintable(infDirNative));
            return false;
        }
*/
    }
	
    //DBG("refresh driver done.\n");
    return true;

}

bool DriverHelper::InstallYunDevice(QString infNative,QString &strDeviceID,QString &strDeviceName)
{
    DriverInfo info;
    RemoteDrvInfo remoteInfo;
	QString errmsg;
    QString str_info = "";
    infNative.replace('\\', '/');
    str_info = getDriverInfCustomName(infNative);
	bool ret = false;
	int tmp = 0;

    // 防止安装 machine.inf 驱动
    if (infNative.contains("machine.inf", Qt::CaseInsensitive)) {
        return false;
    }

    // 驱动补丁列表
    if (isPatchInf(infNative)) {
        char buf[512];
        strncpy(buf, infNative.toLocal8Bit().data(), MAX_PATH);
        char *p = strrchr(buf, '/');
        if (p) {
            p++;
            strDeviceID = p;
            strDeviceName = p;
        }
    }

    do{
        if(!DriverUploadThread::checkRemoteDriverExists(osInfo, strDeviceName, strDeviceID, remoteInfo, "", "", 1)) { // 根据osInfo name device_id 获取 remoteInfo
            if(!DriverUploadThread::checkRemoteDriverExists(osInfo, strDeviceName, strDeviceID, remoteInfo, "", "", 0)) {
                tmp = 5;
                continue;
            }
        }

        if(remoteInfo.driver_id == -1) {
            continue;
        }    

        QString tmpFile = g_tmpDriverPath + getRandomName();
        QFile f(tmpFile);
        if(!f.open(QIODevice::WriteOnly)) {
            continue;
        }

        QDataStream fs(&f);
        ZDMHttpEx http; 
        //connect(&http, SIGNAL(signal_progress(void*,int,int)), SLOT(slot_downloadProgress(void*,int,int)));
        if(0 != http.get(remoteInfo.url, fs)) {
            f.close();
            f.remove();
            continue;
        }
        f.close();
/*
        if(getDriverInfo(tmpFile, info, errmsg)
                && updateDrv(info, tmpFile, strDeviceID, errmsg)) {
            tmp = 1; 
        } else {
			tmp = -1;
        }
		ret = updateDrvYun(info, tmpFile, strDeviceID, errmsg);
*/
		getDriverInfo(tmpFile, info, errmsg);        
        updateDrvYun(info, tmpFile, strDeviceID, errmsg,str_info);

		f.remove();//删除临时文件

#ifndef CMD_DEBUG
        f.remove();
#endif


	}while(0);
	return false;
}

void DriverHelper::GetDeviceID_Name(QString infNative,QString& strDeviceID,QString& strDeviceName)
{
	int tmp = 5;
	if (infNative.contains("drivers_x64\\mtk")) {
	   if (infNative.contains("mdmcpq.inf")){
          strDeviceID = "usb\\vid_0e8d&pid_2000";// ok
          strDeviceName = "x64_mtk_mdmcpq";
		  tmp = 6;       
	   }else if(infNative.contains("Infs")){
		   if(infNative.contains("usbvcom.inf")){	
              strDeviceID = "USB\\Vid_0e8d&Pid_2000";// ok
              strDeviceName = "x64_mtk_infs_usbvcom";
			  tmp = 6;
		   } else if(infNative.contains("usbvcom_brom.inf")){ 
              strDeviceID = "USB\\Vid_0e8d&Pid_0003"; // 0k
              strDeviceName = "x64_mtk_infs_usbvcom_brom";
			  tmp = 6;
		   }
	   }else if(infNative.contains("Unsigned infs")){
		   if(infNative.contains("android_winusb.inf")){
              strDeviceID = "USB\\VID_0BB4&PID_0C01";//ok
              strDeviceName = "x64_mtk_Unsigned_infs_android_winusb";
			  tmp = 6;
		   } else if(infNative.contains("cdc-acm.inf")){ 
              strDeviceID = "USB\\VID_0BB4&PID_0005";//ok
              strDeviceName = "x64_mtk_Unsigned_infs_cdc-acm";
			  tmp = 6;
		   } else if(infNative.contains("tetherxp.inf")){ 
              strDeviceID = "USB\\VID_0e8d&PID_2004"; //驱动没找到
              strDeviceName = "x64_mtk_Unsigned_infs_tetherxp";
			  tmp = 6;
		   } else if(infNative.contains("wpdmtp.inf")){ 
              strDeviceID = "USB\\VID_0BB4&PID_2008";//驱动没找到
              strDeviceName = "x64_mtk_Unsigned_infs_wpdmtp";
			  tmp = 6;
		   }
	   }
       tmp = 6;
	}else if (infNative.contains("drivers_x64\\HuaweiSpreadtrum")) {
       if (infNative.contains("ET5321_usbeem10.inf")){
         strDeviceID = "ET5321MUX\\USBEEM10";//驱动没找到
         strDeviceName = "x64_zhanxun_ET5321_usbeem10";
		 tmp = 6; 
	   }else if(infNative.contains("ET5321_enum.inf")){
         strDeviceID = "DCENUMROOT";//驱动没找到
         strDeviceName = "x64_zhanxun_ET5321_usbeem10";
         tmp = 6; 
	   }else if(infNative.contains("ET5321_usbfunc.inf")){
         strDeviceID = "usb\\vid_12d1&pid_1d27&mi_02";// ok
         strDeviceName = "x64_zhanxun_ET5321_usbfunc";
         tmp = 6; 
	   }else if(infNative.contains("ET5321_mux.inf")){
         strDeviceID = "USB\\VID_12D1&PID_1D27&MI_01";//ok
         strDeviceName = "x64_zhanxun_ET5321_mux";
         tmp = 6; 
	   }else if(infNative.contains("ET5321_portcfg.inf")){
         strDeviceID = "USB\\VID_12D1&PID_1D27";// ok
         strDeviceName = "x64_zhanxun_ET5321_portcfg";
         tmp = 6; 
	   }else if(infNative.contains("ET5321_vport.inf")){
         strDeviceID = "ET5321MUX\\HW_MUXPORT_00"; //ok
         strDeviceName = "x64_zhanxun_ET5321_vport";
         tmp = 6; 
	   }else if(infNative.contains("ET5321_diag.inf")){
         strDeviceID = "USB\\VID_12D1&PID_1D28&MI_01";//没找到
         strDeviceName = "x64_zhanxun_ET5321_diag";
         tmp = 6; 
	   }
	}else if (infNative.contains("drivers_x86\\mtk")) {
	   if (infNative.contains("mdmcpq.inf")){
          strDeviceID = "usb\\vid_0e8d&pid_2000";
          strDeviceName = "x86_mtk_mdmcpq";
		  tmp = 6;       
	   }else if(infNative.contains("Infs")){
		   if(infNative.contains("usbvcom.inf")){	
              strDeviceID = "USB\\Vid_0e8d&Pid_2000";
              strDeviceName = "x86_mtk_infs_usbvcom";
			  tmp = 6;
		   } else if(infNative.contains("usbvcom_brom.inf")){
              strDeviceID = "USB\\Vid_0e8d&Pid_0003";
              strDeviceName = "x86_mtk_infs_usbvcom_brom";
			  tmp = 6;
		   }
	   }else if(infNative.contains("Unsigned infs")){
		   if(infNative.contains("android_winusb.inf")){
              strDeviceID = "USB\\VID_0BB4&PID_0C01";
              strDeviceName = "x86_mtk_Unsigned_infs_android_winusb";
			  tmp = 6;
		   } else if(infNative.contains("cdc-acm.inf")){ 
              strDeviceID = "USB\\VID_0BB4&PID_0005";
              strDeviceName = "x86_mtk_Unsigned_infs_cdc-acm";
			  tmp = 6;
		   } else if(infNative.contains("tetherxp.inf")){ 
              strDeviceID = "USB\\VID_0e8d&PID_2004";
              strDeviceName = "x86_mtk_Unsigned_infs_tetherxp";
			  tmp = 6;
		   } else if(infNative.contains("wpdmtp.inf")){ 
              strDeviceID = "USB\\VID_0BB4&PID_2008";
              strDeviceName = "x64_mtk_Unsigned_infs_wpdmtp";
			  tmp = 6;
		   }
	   }
	}else if (infNative.contains("drivers_x86\\HuaweiSpreadtrum")) {
       if (infNative.contains("ET5321_usbeem10.inf")){
         strDeviceID = "ET5321MUX\\USBEEM10";
         strDeviceName = "x86_zhanxun_ET5321_usbeem10";
		 tmp = 6; 
	   }else if(infNative.contains("ET5321_enum.inf")){
         strDeviceID = "DCENUMROOT";
         strDeviceName = "x86_zhanxun_ET5321_usbeem10";
         tmp = 6; 
	   }else if(infNative.contains("ET5321_usbfunc.inf")){
         strDeviceID = "usb\\vid_12d1&pid_1d27&mi_02";
         strDeviceName = "x86_zhanxun_ET5321_usbfunc";
         tmp = 6; 
	   }else if(infNative.contains("ET5321_mux.inf")){
         strDeviceID = "USB\\VID_12D1&PID_1D27&MI_01";
         strDeviceName = "x86_zhanxun_ET5321_mux";
         tmp = 6; 
	   }else if(infNative.contains("ET5321_portcfg.inf")){
         strDeviceID = "USB\\VID_12D1&PID_1D27";
         strDeviceName = "x86_zhanxun_ET5321_portcfg";
         tmp = 6; 
	   }else if(infNative.contains("ET5321_vport.inf")){
         strDeviceID = "ET5321MUX\\HW_MUXPORT_00";
         strDeviceName = "x86_zhanxun_ET5321_vport";
         tmp = 6; 
	   }else if(infNative.contains("ET5321_diag.inf")){
         strDeviceID = "USB\\VID_12D1&PID_1D28&MI_01";
         strDeviceName = "x86_zhanxun_ET5321_diag";
         tmp = 6; 
	   }
	}else if (infNative.contains("drivers_both\\mtk")) {
	   if (infNative.contains("usbvcom.inf")){
         strDeviceID = "USB\\Vid_0e8d&Pid_0003";// OK
         strDeviceName = "both_mtk_usbvcom";
		 tmp = 6;       
	   }
	}else if (infNative.contains("drivers_both\\google_usb_driver")) {
	   if (infNative.contains("android_winusb.inf")){
          strDeviceID = "USB\\VID_18D1&PID_0D02"; // ok
          strDeviceName = "both_google";  
	   }
	}
	else if (infNative.contains("drivers_both\\samsung")) {
	   if (infNative.contains("ss_conn_usb_driver.inf")){
          strDeviceID = "USB\\VID_04E8&PID_685D&CONN"; //驱动没找到
          strDeviceName = "both_samsung_ss_conn_usb_driver";    
	   }else if (infNative.contains("ssudadb.inf")){
          strDeviceID = "USB\\VID_04E8&PID_685D&ADB";// ok
          strDeviceName = "both_samsung_ssudadb";
	   }else if (infNative.contains("ssudbus.inf")){
          strDeviceID = "USB\\VID_04E8&PID_685D";// ok
          strDeviceName = "both_samsung_ssudbus";
	   }else if (infNative.contains("ssudcdf.inf")){
          strDeviceID = "USB\\VID_04E8&PID_6818";// ok
          strDeviceName = "both_samsung_ssudcdf"; 
	   }else if (infNative.contains("ssuddmgr.inf")){
          strDeviceID = "USB\\VID_04E8&PID_685D&DevMgrSerd"; //驱动没找到
          strDeviceName = "both_samsung_ssuddmgr";
	   }else if (infNative.contains("ssudeadb.inf")){ 
          strDeviceID = "USB\\VID_04E8&PID_685D&ADB_1_5"; //驱动没找到
          strDeviceName = "both_samsung_sssudeadb";
	   }else if (infNative.contains("ssudmarv.inf")){
          strDeviceID = "USB\\VID_04E8&PID_685E&MarvelDiag"; //驱动没找到
          strDeviceName = "both_samsung_ssudmarv";
	   }else if (infNative.contains("ssudmdm.inf")){
          strDeviceID = "USB\\VID_04E8&PID_685D&Modem";// ok
          strDeviceName = "both_samsung_ssudmdm";
	   }else if (infNative.contains("ssudmtp.inf")){
          strDeviceID = "USB\\VID_04E8&PID_6860&MI_00";//ok
          strDeviceName = "both_samsung_ssudmtp";    
	   }else if (infNative.contains("ssudnd5.inf")){
          strDeviceID = "USB\\VID_04E8&PID_685D&EEMNET"; //驱动没找到
          strDeviceName = "both_samsung_ssudnd5"; 
	   }else if (infNative.contains("ssudobex.inf")){
          strDeviceID = "USB\\VID_04E8&PID_685D&ObexSerd"; // 驱动没找到
          strDeviceName = "both_samsung_ssudobex"; 
	   }else if (infNative.contains("ssudrmnet.inf")){
          strDeviceID = "USB\\VID_04E8&PID_685D&RMNET";// ok
          strDeviceName = "both_samsung_ssudrmnet";
	   }else if (infNative.contains("ssudrmnetmp.inf")){
          strDeviceID = "USB\\VID_04E8&PID_685D&MP&RMNETMP";
          strDeviceName = "both_samsung_ssudrmnetmp";
	   }else if (infNative.contains("ssudrnds.inf")){
          strDeviceID = "USB\\VID_04E8&PID_6861&RNdis";
          strDeviceName = "both_samsung_ssudrnds";  
	   }else if (infNative.contains("ssudsdb.inf")){
          strDeviceID = "USB\\VID_04E8&PID_685D&SDB"; // ok
          strDeviceName = "both_samsung_ssudsdb"; 
	   }else if (infNative.contains("ssudserd.inf")){
          strDeviceID = "USB\\VID_04E8&PID_685D&DiagSerd"; //ok
          strDeviceName = "both_samsung_ssudserd";  
	   }
	}else if (infNative.contains("drivers_both\\xiaomi_fastboot")) {
	   if (infNative.contains("android_winusb.inf")){
          strDeviceID = "usb\\class_ff&subclass_43&prot_01";
          strDeviceName = "both_xiaomi_fastboot";    
	   }
	}
}

static void releaseAsset(const QString& filename) {
    if(!QFile::exists(filename)) {
        QFile f(":/"+filename);
        if(f.open(QIODevice::ReadOnly)){
            QFile w(filename);
            if(w.open(QIODevice::WriteOnly)) {
                w.write(f.readAll());
                w.close();
            }
            f.close();
        }
    }
}

DriverHelper::DriverHelper(QObject *parent)
    : QObject(parent)
{
    osInfo.detect();
    if(osInfo.isWow64) {
        devcon = "devcon64.exe";
        dpinst = "dpinst64.exe";
    } else {
        devcon = "devcon32.exe";
        dpinst = "dpinst32.exe";
    }

    releaseAsset(devcon);
    releaseAsset(dpinst);
    g_tmpDriverPath = qApp->applicationDirPath() + "/tmpDriver/";
    rmdir_recursively(g_tmpDriverPath);
    QDir dir(g_tmpDriverPath);
    if (!dir.exists()){
        dir.mkdir(g_tmpDriverPath);
    }
    int a = 5;

    // read patch list
    QString fn = QApplication::applicationDirPath() + "/drivers_both/patch.list";
    QFile pf(fn);
    if (pf.open(QFile::ReadOnly | QFile::Text)) {
        patchInfList.clear();
        while(pf.bytesAvailable()) {
            QString line = pf.readLine().trimmed();
            if (line.isEmpty() == false)
                patchInfList.push_back(line);
        }
    }
}

DriverHelper::~DriverHelper() {
    while(!deviceList.isEmpty()) {
        delete deviceList.takeFirst();
    }
    while(!dbList.isEmpty()) {
        delete dbList.takeFirst();
    }
}

QList<DriverDeviceNode *> DriverHelper::getDeviceList() {
    QList<DriverDeviceNode *> ret;
    mutex.lock();
    ret.append(deviceList);
    mutex.unlock();
    return ret;
}

QList<DriverDeviceNode *> DriverHelper::getDeviceList(const QStringList& ids) {
    QList<DriverDeviceNode *> ret;
    mutex.lock();
    foreach(const QString& id, ids) {
        foreach(DriverDeviceNode *node, deviceList) {
            if(node->device_id_long == id) {
                ret.append(node);
                break;
            }
        }
    }
    mutex.unlock();
    return ret;
}

DriverDeviceNode *DriverHelper::getDeviceById(const QString &id) {
    DriverDeviceNode *ret = NULL;
    mutex.lock();
    foreach(DriverDeviceNode *node, deviceList) {
        if(node->device_id_long == id) {
            ret = node;
            break;
        }
    }
    mutex.unlock();
    return ret;
}

QString DriverHelper::GetDeviceStringProperty(__in HDEVINFO Devs, __in PSP_DEVINFO_DATA DevInfo, __in DWORD Prop) {
    DWORD guess_size = 1024;
    DWORD real_size = 0;
    DWORD type = 0;
    int code = -1;
    char *buffer = new char[guess_size];
    buffer[0] = '\0';

    while(!SetupDiGetDeviceRegistryProperty(Devs, DevInfo, Prop, &type, (PBYTE)buffer, guess_size, &real_size)) {
        code = GetLastError();
        if(code != ERROR_INSUFFICIENT_BUFFER) {
            emit signal_error(code, tr("SetupDiGetDeviceRegistryProperty %1 failed").arg(Prop));
            goto GetDeviceStringProperty_quit;
        }

        if(type != REG_SZ) {
            emit signal_error(code, tr("SetupDiGetDeviceRegistryProperty %1 failed, type = %2").arg(Prop).arg(type));
            goto GetDeviceStringProperty_quit;
        }

        delete[] buffer;
        guess_size = real_size;
        buffer = new char[real_size+1];
        buffer[0] = '\0';
    }
GetDeviceStringProperty_quit:
    QString ret = QString::fromLocal8Bit(buffer, real_size);
    delete[] buffer;
    return ret;
}

void DriverHelper::loadDeviceList(GUID *guid, int flags) {
    int code = -1;
    SP_DEVINFO_DATA devInfo;
    SP_DEVINFO_LIST_DETAIL_DATA devInfoListDetail;
    char deviceId[MAX_DEVICE_ID_LEN];
    int n = -1;

    HDEVINFO devs = SetupDiGetClassDevsEx(guid,
                                          NULL,
                                          NULL,
                                          (guid != NULL ? 0 : DIGCF_ALLCLASSES) | flags,
                                          NULL,
                                          NULL, NULL);
    if(devs == INVALID_HANDLE_VALUE) {
        code = GetLastError();
        emit signal_error(code, tr("SetupDiGetClassDevsEx failed"));
        return;
    }

    devInfoListDetail.cbSize = sizeof(devInfoListDetail);
    if(!SetupDiGetDeviceInfoListDetail(devs, &devInfoListDetail)) {
        code = GetLastError();
        emit signal_error(code, tr("SetupDiGetDeviceInfoListDetail failed"));
        SetupDiDestroyDeviceInfoList(devs);
        return;
    }

    devInfo.cbSize = sizeof(devInfo);
    for(int i=0; SetupDiEnumDeviceInfo(devs, i, &devInfo); i++) {
        DriverDeviceNode *node = new DriverDeviceNode();
        deviceList.append(node);

        // name
        //node->name = GetDeviceStringProperty(devs, &devInfo, SPDRP_FRIENDLYNAME);
        //if(node->name.isEmpty()) {
        node->name = GetDeviceStringProperty(devs, &devInfo, SPDRP_DEVICEDESC);
        //}

        // device_id
        if(CM_Get_Device_ID_Ex(devInfo.DevInst, deviceId, MAX_DEVICE_ID_LEN,
                               0, devInfoListDetail.RemoteMachineHandle)
                != CR_SUCCESS) {
            code = GetLastError();
            emit signal_error(code, tr("CM_Get_Device_ID_Ex %1 failed").arg(node->name));
        }
        node->device_id_long = QString::fromLocal8Bit(deviceId);

        node->device_id = node->device_id_long;
        n = node->device_id.lastIndexOf('\\');
        if(n != -1) {
            node->device_id.truncate(n);
        }

        if((n = node->device_id.indexOf(QRegExp("(VEN_|VID_)[0-9A-F]{4}"))) != -1) {
            node->vid = node->device_id.mid(n+4, 4);
        }
        if((n = node->device_id.indexOf(QRegExp("(DEV_|PID_)[0-9A-F]{4}"))) != -1) {
            node->pid = node->device_id.mid(n+4, 4);
        }

        // status & problem
        if((flags & DIGCF_PRESENT) != 0) {
            if(CM_Get_DevNode_Status_Ex(&node->status, &node->problem, devInfo.DevInst,
                                        0, devInfoListDetail.RemoteMachineHandle)
                    != CR_SUCCESS) {
                code = GetLastError();
                //DBG("CM_Get_DevNode_Status_Ex %s failed %d\n", node->name.toLocal8Bit().data(), code);
            }
        }

        // parent
        DWORD parentInst;
        if(CM_Get_Parent_Ex(&parentInst, devInfo.DevInst, 0, devInfoListDetail.RemoteMachineHandle) != CR_SUCCESS) {
            code = GetLastError();
            //DBG("CM_Get_Parent_Ex %s failed %d\n", node->name.toLocal8Bit().data(), code);
        } else {
            if(CM_Get_Device_ID_Ex(parentInst, deviceId, MAX_DEVICE_ID_LEN,
                                   0, devInfoListDetail.RemoteMachineHandle)
                    == CR_SUCCESS) {
                node->parent_devId = QString::fromLocal8Bit(deviceId);
            }
        }

        //----------
        emit signal_gotDevice(node);
    }

    SetupDiDestroyDeviceInfoList(devs);
}

#ifdef USE_DDK
void DriverHelper::enumerateUsbControllers() {
    ULONG num;
    ZTreeItem item  = AddLeaf(0, 0, "My Computer", ComputerIcon);
    EnumerateHostControllers(item, &num);
    ZTreeItemPrint(item, 0);
    ZTreeItemFree(item);
}
#endif

//扫描注册表来获取设备
void DriverHelper::reload(bool phonesOnly, bool presentOnly) {
    GUID cls[] = {
        // serial
        { 0x4D36E978, 0xE325, 0x11CE, { 0xBF, 0xC1, 0x08, 0x00,
                                        0x2B, 0xE1, 0x03, 0x18 } },
        // usb
        { 0x36fc9e60, 0xc465, 0x11cf, { 0x80, 0x56, 0x44, 0x45,
                                        0x53, 0x54, 0x00, 0x00 } },
        // android
        { 0x3f966bd9, 0xfa04, 0x4ec5, { 0x99, 0x1c, 0xd3, 0x26,
                                        0x97, 0x3b, 0x51, 0x28 } },
        // fastboot
        { 0xf72fe0d4, 0xcbcb, 0x407d, { 0x88, 0x14, 0x9e, 0xd6,
                                        0x73, 0xd0, 0xdd, 0x6b } },
        // YunOS
        { 0x0c8e9016, 0x5b23, 0x4182, { 0xac, 0xfb, 0x7c, 0x8d,
                                        0xb6, 0xd0, 0x53, 0xc0} },
        // others
        { 0x9d7debbc, 0xc85d, 0x11d1, { 0x9e, 0xb4, 0x00, 0x60,
                                        0x08, 0xc3, 0xa1, 0x9a } },
    };
    int clsNum = sizeof(cls) / sizeof(GUID);

    mutex.lock();
    while(!deviceList.isEmpty()) {
        delete deviceList.takeFirst();
    }
    emit signal_deviceListClear();
#ifdef USE_DDK
    enumerateUsbControllers();
#endif
    if(phonesOnly) {
        for(int i=0; i<clsNum; i++) {
            loadDeviceList(&cls[i], presentOnly ? DIGCF_PRESENT : 0);
        }
        for(int i=0; i<deviceList.size(); i++) {
            DriverDeviceNode *device = deviceList[i];
            if (device->device_id.contains("ROOT")
                    || device->device_id.contains("ACPI")
                    || device->vid == "8086"
                    || device->vid == "8087"
                    || device->vid == "80EE"
                    || device->vid == "0E0F"
                    || device->vid == "0000"
                    || device->vid == "041E"
                    || device->vid == "046D") {
                if (!device->name.contains("HUAWEI")) {
                    //DBG("delete [%s]\n", device->name.toLocal8Bit().data());
                    delete deviceList.takeAt(i--);
                    continue;
                }
            }

            if(device->name.contains(QString::fromLocal8Bit("集线器"))
                    || device->name.contains(QString::fromLocal8Bit("大容量存储"))
                    || device->name.contains(QString::fromLocal8Bit("USB Mass Storage Device"), Qt::CaseInsensitive)
                    || device->name.contains(QString::fromLocal8Bit("USB HUB"), Qt::CaseInsensitive)
                    || device->name.contains(QString::fromLocal8Bit("主机控制器"))
                    || device->name.contains(QString::fromLocal8Bit("HOST CONTROLLER"), Qt::CaseInsensitive)) {
                //DBG("delete [%s]\n", device->name.toLocal8Bit().data());
                delete deviceList.takeAt(i--);
                continue;
            }
        }
    } else {
        loadDeviceList(NULL, presentOnly ? DIGCF_PRESENT : 0);
    }

    // 需使用inf直接从windows提取的驱动列表
    foreach (QString inf, patchInfList) {
        DriverDeviceNode *node = new DriverDeviceNode();
        node->name = inf;
        node->problem = 0;
        node->status = 0;
        deviceList.append(node);
    }

    mutex.unlock();
}

static void getAllFiles(const QString& path, QStringList &files) {
    QDir dir(path);
    QFileInfoList infList = dir.entryInfoList(QDir::AllEntries|QDir::NoDotAndDotDot);
    foreach(const QFileInfo& info, infList) {
        if(info.isDir()) {
            getAllFiles(info.filePath(), files);
        } else {
            files.append(info.filePath());
        }
    }
}

static bool contentsMatch(const QString& path1, const QString& path2) {
    QFile f1(path1);
    QFile f2(path2);
    QByteArray d1;
    QByteArray d2;

    if(f1.open(QIODevice::ReadOnly)) {
        d1 = f1.readAll();
        f1.close();
    } else {
        return false;
    }

    if(f2.open(QIODevice::ReadOnly)) {
        d2 = f2.readAll();
        f2.close();
    } else {
        return false;
    }

    return (d1 == d2);
}

bool DriverHelper::getDriverFilesByInf(const QString &inf, DriverInfo &drvInfo, QString& errmsg) {
    bool ret = false;
    //DBG("getDriverFilesByInf [%s]\n", qPrintable(inf));

    do {
        QString catFile;
        //QSettings ini(inf, QSettings::IniFormat);
        ZSettings ini(inf);
        ini.beginGroup("Version");

        QStringList keys = ini.childKeys();
        foreach (QString key, keys) {
            // Catalogfile 字段前后空格bug
            key = key.trimmed().toLocal8Bit().data();
            if (key.startsWith("CATALOGFILE", Qt::CaseInsensitive)) {
                if (key.contains(osInfo.isWow64 ? "64" : "86")) {
                    catFile = ini.value(key); //.toString();
                }
                else if (key.toUpper() == "CATALOGFILE") {
                    catFile = ini.value(key); //.toString();
                }
            }
            else if (key == "DriverVer") {
                // 有些驱动的版本号中间存在空格，在web调用时出错
                QString ver = ini.value(key);
                ver.replace(" ", "");
                drvInfo.drvVersion = ver;
                //drvInfo.drvVersion = ini.value(key); //.toString();
            }
        }

        if(catFile.isEmpty()) {
            errmsg = tr("cannot find cat file!");
            // 未签名驱动没有cat签名文件,继续打包inf路径
            char szinf[512];
            strncpy(szinf, inf.toLocal8Bit().data(), MAX_PATH);
            char *p = strrchr(szinf, '/');
            if (p == NULL) p = strrchr(szinf, '\\');
            if (p) {
                p++;
                catFile = p;
            }
            //break;
        }

        //DBG("catFile = [%s]\n", qPrintable(catFile));
        QString strDriverPath = QString::fromLocal8Bit(getenv("SYSTEMROOT")) + "\\System32\\DriverStore\\FileRepository";
        if (osInfo.platform == 2 && osInfo.majorVer == 5) {
            // XP 下路径不同
            strDriverPath = QString::fromLocal8Bit(getenv("SYSTEMROOT")) + "\\System32\\DRVSTORE";
        }
        //DBG("driver path: %s\n", strDriverPath.toLocal8Bit().data());
        QDir d(strDriverPath);
        QFileInfoList dirInfos = d.entryInfoList(QDir::Dirs|QDir::NoDotAndDotDot);
        foreach(const QFileInfo& dirInfo, dirInfos) {
            QString catFullName = dirInfo.canonicalFilePath()+"\\"+catFile;
            if(!QFile::exists(catFullName)) {
                continue;
            }
            //DBG("cat file find: %s\n", catFullName.toLocal8Bit().data());

            QDir subDir(dirInfo.canonicalFilePath());
            QFileInfoList subInfos = subDir.entryInfoList(QDir::Files);
            foreach(const QFileInfo& subInfo, subInfos) {
                if(subInfo.fileName().endsWith(".inf", Qt::CaseInsensitive)
                        && contentsMatch(subInfo.canonicalFilePath(), inf)) {
                    drvInfo.infPath = subInfo.canonicalFilePath();
                    drvInfo.drvPath = subDir.canonicalPath();
                    ret = true;
                    break;
                }
            }

            if(ret) {
                getAllFiles(dirInfo.canonicalFilePath(), drvInfo.files);
                break;
            }
        }
    } while(0);

    if(!ret && errmsg.isEmpty()) {
        errmsg = tr("cannot find exact inf file!");
    }
    return ret;
}

bool DriverHelper::getDriverInfo(const QString &zip, DriverInfo &info, QString &errmsg) {
    QByteArray xml;
    if (!ZipHelper::readFileInZip(zip, "dxdrv.xml", xml)) {
        errmsg = tr("invalid drv file");
        return false;
    }

    WinOSInfo tmpOsInfo;
    if (!info.fromXml(xml, tmpOsInfo)) {
        errmsg = tr("invalid xml in drv file");
        return false;
    }

    if(tmpOsInfo.isWow64 != osInfo.isWow64
            || tmpOsInfo.platform != osInfo.platform
            || tmpOsInfo.majorVer != osInfo.majorVer
            || tmpOsInfo.minorVer != osInfo.minorVer) {
        errmsg = tr("invalid target operating system");
        return false;
    }

    return true;
}

bool DriverHelper::updateDrv(DriverInfo &info, const QString &path, const QString &device_id, QString &errmsg) {
    bool ret = false;
    QString tmpDir = g_tmpDriverPath + getRandomName();
    if(!ZipHelper::extractZip(path, tmpDir)) {
        errmsg = tr("unzip failed");
        return false;
    }

    QProcess p;
    QStringList args;
    args << "update" << tmpDir+"/"+info.infPath << device_id;
    p.start(devcon, args);
    do {
        if(!p.waitForStarted(2000)) {
            errmsg = tr("start devcon failed!");
            break;
        }

        if(!p.waitForFinished()) {
            errmsg = tr("finish devcon failed!");
            p.kill();
            break;
        }

        int r = p.exitCode();
        QByteArray out = p.readAll();
        printf("\n\nexitCode=%d\n[%s]", r, out.data());
        switch(r) {
        case 0:
            ret = true;
            break;
        case 1:
            ret = true;
            errmsg = tr("Reboot required");
            break;
        default:
            //ret = false;//注释
			ret = true;
            errmsg = QString::fromLocal8Bit(out);
            break;
        }
    } while(0);

    //DBG("clean tmpdir '%s'\n", tmpDir.toLocal8Bit().data());
    args.clear();
    args << "/c" << "rd" << "/s" << "/q" << tmpDir;
    int r = p.execute("cmd", args);// 清除临时文件夹
    //DBG("r = %d\n", r);

    return ret;
}

bool DriverHelper::updateDrvYun(DriverInfo &info, const QString &path, const QString &device_id, QString &errmsg, QString &str_info) {
    bool ret = false;
    QString tmpDir = g_tmpDriverPath + getRandomName();
    if(!ZipHelper::extractZip(path, tmpDir)) {
        errmsg = tr("unzip failed");
        return false;
    }

    QProcess p;
    QStringList args;
	unsigned int r;
    QByteArray out;

#if 0
    args << "update" << tmpDir+"/"+info.infPath << device_id;
    p.start(devcon, args);
    do {
        if(!p.waitForStarted(2000)) {
            errmsg = tr("start devcon failed!");
            break;
        }

        if(!p.waitForFinished()) {
            errmsg = tr("finish devcon failed!");
            p.kill();
            break;
        }

        int r = p.exitCode();
        QByteArray out = p.readAll();
        printf("\n\nexitCode=%d\n[%s]", r, out.data());
        switch(r) {
        case 0:
            ret = true;
            break;
        case 1:
            ret = true;
            errmsg = tr("Reboot required");
            break;
        default:
            //ret = false;//注释
			ret = true;
            errmsg = QString::fromLocal8Bit(out);
            break;
        }
    } while(0);
#else
	QString inf;
	 //删除
    do{
	 args.clear();
	 QDir dir(tmpDir); // 查找inf文件
	 if (!dir.exists()) 
        break; 

	//设置过滤器(目录，文件或非上级目录) 
    dir.setFilter(QDir::Dirs|QDir::Files|QDir::NoDotAndDotDot); 
    dir.setSorting(QDir::DirsFirst); 
    //取得目录中文件列表(包含目录) 
    QFileInfoList list = dir.entryInfoList(); 
	int i=0; 
    do{ 
        QFileInfo fileInfo = list.at(i); 
        bool bisDir=fileInfo.isDir(); 
        //判断是否为目录，如果是目录则遍历，否则当前处理文件 
        if(bisDir) 
        { 
			i++;
            continue;//从服务器下载的驱动包，inf文件一定在第一层目录下
        }
		else 
		{
            QString temp=fileInfo.suffix().toLower(); 
			if(temp == "inf"){
              //inf =  fileInfo.fileName();
              inf =  fileInfo.path() + "/" + fileInfo.fileName();
			  break;
			}  
			int a = 5;
		}
	    i++; 
    }while(i<list.size());

	 QString infNative = inf;
     args << "/U" << infNative << "/SW" << "/D";
     p.start(dpinst, args);

      //emit signal_addLog(tr("Removing drivers from [%1]").arg(getDriverInfCustomName(inf)));
      emit signal_addLog(tr("Removing drivers from [%1]").arg(str_info));
      if(p.waitForStarted(2000) && p.waitForFinished())
	  {
        r = p.exitCode();
        out = p.readAllStandardError() + p.readAllStandardOutput();
        //DBG("[Uninstall '%s'] = 0x%08x, <%s>\n", qPrintable(infNative), r, out.data());
      } 
	  else 
	  {
          errmsg = tr("Uninstall for %1 failed").arg(infNative);
          //DBG("[Uninstall '%s'] exec failed\n", qPrintable(infNative));
            //return false;
      }
	  
	}while(0);

     //安装
	 args.clear();

	 tmpDir.replace('/', '\\');
     //DBG("[Install '%s']\n", qPrintable(tmpDir));

     args << "/PATH" << tmpDir << "/F" << "/LM" << "/SW" << "/SA" << "/A";
     p.start(dpinst, args);

     emit signal_addLog(tr("Installing drivers from [%1]").arg(str_info));

     DBG("Installing drivers from [%s]-[%s]",
         info.device_id.toLocal8Bit().data(),
         info.drvVersion.toLocal8Bit().data());

     do{
         if(p.waitForStarted(2000) && p.waitForFinished()) {
            r = p.exitCode();
            out = p.readAllStandardError() + p.readAllStandardOutput();
            //DBG("[Install '%s'] = 0x%08x, <%s>\n", qPrintable(tmpDir), r, out.data());        
         } else {
            errmsg = tr("Install for %1 failed").arg(tmpDir);
            //DBG("[Install '%s'] exec failed\n", qPrintable(tmpDir));

            // Win10 下有些驱动无法用p.start安装，UAC提权问题
            //QProcess::execute(dpinst, args);
            QString tmpDir = QDir::tempPath();
            QString bat = tmpDir + "/" + "u.bat";
            QString cmd = "echo ";
            cmd += QApplication::applicationDirPath();
            cmd += "/";
            cmd += dpinst;
            cmd += " " + args.join(" ");
            cmd += " > ";
            cmd += bat;
            system(cmd.toLocal8Bit().data());
            system(bat.toLocal8Bit().data());
         }
	   }while(0);
#endif

    // 检查补丁
    patchDrvYun(inf);

    //DBG("clean tmpdir '%s'\n", tmpDir.toLocal8Bit().data());
    args.clear();
    args << "/c" << "rd" << "/s" << "/q" << tmpDir;
    r = p.execute("cmd", args);// 清除临时文件夹
    //DBG("r = %d\n", r);

    return ret;
}

void DriverHelper::patchDrvYun(QString inf)
{
    QProcess p;
    QStringList args;

    // 展讯驱动补丁代码, 2015/9/21 yangzijiang
    if (inf.contains("ET5321_enum.inf", Qt::CaseInsensitive)) {
        args.clear();
        args << "remove" << "DCENUMROOT";
        p.start(devcon, args);
        p.waitForStarted(2000);
        p.waitForFinished();

        args.clear();
        args << "-r" << "install" << inf << "DCENUMROOT";
        p.start(devcon, args);
        p.waitForStarted(2000);
        p.waitForFinished();
    }
}

bool DriverHelper::getDriverInfo(DriverDeviceNode *node, DriverInfo &info, QString& errmsg) {
    bool foundDrivers = false;
    info.name = node->name;
    info.device_id = node->device_id;
    info.vid = node->vid;
    info.pid = node->pid;

    info.infPath.clear();
    info.drvPath.clear();
    info.files.clear();
    info.filesInWindir.clear();

    QProcess p;
    QStringList args;
    args << "driverfiles" << "@"+node->device_id_long;

    p.start(devcon, args);
    do {
        if(!p.waitForStarted(2000)) {
            errmsg = tr("start devcon failed!");
            break;
        }

        if(!p.waitForFinished(5000)) {
            errmsg = tr("finish devcon failed!");
            p.kill();
            break;
        }

        QByteArray out = p.readAll();

        QList<QByteArray> lines = out.split('\n');
        foreach(QByteArray line, lines) {
            line = line.simplified();
            if(line.isEmpty()) {
                continue;
            }

            if(foundDrivers && !line.contains("matching device(s) found")) {
                info.filesInWindir.append(QString::fromLocal8Bit(line));
            }

            int pos = -1;
            if((pos = line.indexOf("Driver installed from")) != -1) {
                pos += 22; // strlen

                int n = line.indexOf(" [", pos);
                QString inf = line.mid(pos, n-pos);

                // 防止上传 machine.inf 驱动
                if (inf.contains("machine.inf", Qt::CaseInsensitive)) {
                    break;
                }

                info.oemFile = inf;
                if(getDriverFilesByInf(inf, info, errmsg)) {
                    foundDrivers = true;
                } else {
                    break;
                }
            }
        }
    } while(0);

    if(!foundDrivers) {
        // 驱动获取补丁
        foundDrivers = patchGetDriverInfo(node, info, errmsg);

        if (errmsg.isEmpty()) {
            errmsg = tr("No driver files found");
        }
    }

    if (foundDrivers) {
        // 检查.cat文件签名
        foreach (QString str, info.files) {
            if (str.endsWith(".cat", Qt::CaseInsensitive)) {
                //DBG("Start verifyFile Sign '%s'\n", str.toLocal8Bit().data());
                if (false == WinVerifyTrust::verifyFile(str.toStdWString().data())) {
                    info.bsigned = 0;
                    break;
                }
            }
        }
    }

    return foundDrivers;
}

QString DriverHelper::findFileSysPath(QString &name)
{
    QString fullName;
    WCHAR szbuf[512];

    if (QFile::exists(name)) {
        return name;
    }

    GetSystemWindowsDirectoryW(szbuf, sizeof(szbuf));
    fullName = QString::fromStdWString(szbuf);

    fullName += QString::fromStdWString(L"/");
    QString n1 = fullName + name;
    if (QFile::exists(n1)) {
        return n1;
    }

    n1 = fullName + QString::fromStdWString(L"system32/") + name;
    if (QFile::exists(n1)) {
        return n1;
    }

    n1 = fullName + QString::fromStdWString(L"system32/drivers/") + name;
    if (QFile::exists(n1)) {
        return n1;
    }

    n1 = fullName + QString::fromStdWString(L"system/") + name;
    if (QFile::exists(n1)) {
        return n1;
    }

    return name;
}

bool DriverHelper::isPatchInf(QString name)
{
    bool rc = false;
    foreach (QString inf, patchInfList) {
        if (name.contains(inf, Qt::CaseInsensitive)) {
            rc = true;
            break;
        }
    }
    return rc;
}

bool DriverHelper::patchGetDriverInfo(DriverDeviceNode *node, DriverInfo& info, QString& errmsg)
{
    bool rc = false;

    if (isPatchInf(node->name)) {
        node->device_id = node->name;
        info.device_id = node->name;

        if (QFile::exists(node->name) == false) {
            QString strDriverPath = QString::fromLocal8Bit(getenv("SYSTEMROOT")) + "\\System32\\DriverStore\\FileRepository";
            if (osInfo.platform == 2 && osInfo.majorVer == 5) {
                // XP 下路径不同
                strDriverPath = QString::fromLocal8Bit(getenv("SYSTEMROOT")) + "\\System32\\DRVSTORE";
            }
            QDir d(strDriverPath);
            QFileInfoList dirInfos = d.entryInfoList(QDir::Dirs|QDir::NoDotAndDotDot);
            foreach(const QFileInfo& dirInfo, dirInfos) {
                QDir subDir(dirInfo.canonicalFilePath());
                QFileInfoList subInfos = subDir.entryInfoList(QDir::Files);
                foreach(const QFileInfo& subInfo, subInfos) {
                    if(subInfo.fileName().endsWith(node->name, Qt::CaseInsensitive)) {
                        node->name = subInfo.canonicalFilePath();
                        break;
                    }
                }
            }
        }

        rc = getDriverFilesByInf(node->name, info, errmsg);

        // fix windirfiles
        foreach (QString fn, info.files) {
            if (   fn.endsWith(".sys", Qt::CaseInsensitive)
                || fn.endsWith(".dll", Qt::CaseInsensitive)) {
#if 1
                char buf[512];
                fn.replace('\\', '/');
                strncpy(buf, fn.toLocal8Bit().data(), MAX_PATH);
                char *p = strrchr(buf, '/');
                if (p) {
                    p++;
                    QString sysrn = p;
                    sysrn = findFileSysPath(sysrn);
                    if (sysrn != QString(p)) {
                        info.filesInWindir.append(sysrn);
                    }
                }
#else
                info.filesInWindir.append(fn);
#endif
            }
        }
    }

    return rc;
}

DriverUploadThread::DriverUploadThread(DriverHelper *drv, QObject *parent)
    : QThread(parent) {
    this->drv = drv;
}

bool DriverUploadThread::checkRemoteDriverExists(WinOSInfo& os, const QString& deviceName, const QString& deviceId, RemoteDrvInfo &remoteInfo, const QString &filesum, const QString &version, int bsigned) {
	QUrl url("http://if.xianshuabao.com/driverquery.ashx");
    QByteArray out;
    QString devName;
    QString devId;

    ZDMHttp::url_escape(deviceName.toLocal8Bit(), devName);
    ZDMHttp::url_escape(deviceId.toLocal8Bit(), devId);
    QString params = QString("osIsWow64=%1"
                             "&osPlatform=%2"
                             "&osMajorVer=%3"
                             "&osMinorVer=%4"
                             "&osBuildNum=%5"
                             "&deviceId=")
            .arg(os.isWow64 ? 1 : 0)
            .arg(os.platform)
            .arg(os.majorVer)
            .arg(os.minorVer)
            .arg(os.buildNum);
    params.append(devId);

    if(!filesum.isEmpty()) {
        params.append(QString("&filesum=") + filesum);
        params.append(QString("&name=") + devName);
        params.append(QString("&version=") + version);
        params.append(QString("&signed=") + QString::number(bsigned));
    }

#if QT_VERSION < 0x050000
    url.setEncodedQuery(params.toLocal8Bit());
#else
    url.setQuery(params);
#endif

    char *uri = strdup(url.toEncoded().data());
    if(0 != ZDMHttp::get(uri, out)) {
        remoteInfo.driver_id = -1;
        remoteInfo.code = -1;
        remoteInfo.msg = tr("connect server failed");
        free(uri);
        return false;
    }

    //DBG("<%s>\nout = <%s>\n", uri, QString::fromUtf8(out).toLocal8Bit().data());
    free(uri);

    //DBG("%s\n", out.data());

    QJson::Parser p;
    bool ret = false;
    QVariantMap data_map = p.parse(out, &ret).toMap();
    if(!ret) {
        remoteInfo.driver_id = -1;
        remoteInfo.code = -2;
        remoteInfo.msg = tr("parse json failed");
        return false;
    }

    remoteInfo.driver_id = data_map["did"].toInt();
    remoteInfo.url = data_map["url"].toString();
    remoteInfo.version = data_map["version"].toString();
    remoteInfo.code = data_map["code"].toInt();
    remoteInfo.msg = data_map["msg"].toString();

    return true;
}

bool DriverUploadThread::uploadRemoteDriver(DriverHelper &drv, DriverInfo& info, const QString& fileName, const QString& filesum) {
	QUrl url("http://if.xianshuabao.com/driverupload.ashx");
    QByteArray out;
    QString devId;
    QString devName;

    ZDMHttp::url_escape(info.device_id.toUtf8(), devId);
    ZDMHttp::url_escape(info.name.toUtf8(), devName);

    QString params = QString("osIsWow64=%1"
                             "&osPlatform=%2"
                             "&osMajorVer=%3"
                             "&osMinorVer=%4"
                             "&osBuildNum=%5")
            .arg(drv.osInfo.isWow64 ? 1 : 0)
            .arg(drv.osInfo.platform)
            .arg(drv.osInfo.majorVer)
            .arg(drv.osInfo.minorVer)
            .arg(drv.osInfo.buildNum);
    params.append(QString("&deviceId=") + devId);
    params.append(QString("&name=") + devName);
    params.append(QString("&version=") + info.drvVersion);
    params.append(QString("&filesum=") + filesum);
    params.append(QString("&filename=") + devName + ".dxdrv");
    params.append(QString("&signed=") + QString::number(info.bsigned));

#if QT_VERSION < 0x050000
    url.setEncodedQuery(params.toLocal8Bit());
#else
    url.setQuery(params);
#endif

    char *uri = strdup(url.toEncoded().data());
    if(0 != ZDMHttp::postFile(uri, fileName, out)) {
        //DBG("connect server failed!\n");
        free(uri);
        return false;
    }

    //DBG("<%s>\nout = <%s>\n", uri, out.data());
    free(uri);
    return true;
}

void DriverUploadThread::run() {

    // yzj: removed 2016-1-22
    return;

#if 0
    //ZDMHttpEx zdm;
    //zdm.getFile("http://dl.wandoujia.com/files/phoenix/latest/wandoujia-wandoujia_web.apk"
    //            , "Z:/wandou_up.apk");
#else
    emit signal_addLog(NULL, tr("start driver upload thread..."));
    drv->reload(true, false);

    DriverInfo info;
    RemoteDrvInfo remoteInfo;

    QString errmsg;
    QList<DriverDeviceNode *> list = drv->getDeviceList();
    int count = 0;
    foreach(DriverDeviceNode *device, list) {
        info.bsigned = 1;
        if(device->device_id.contains("ROOT")
                || device->device_id.contains("ACPI")
                || device->vid == "8086"
                || device->vid == "8087"
                || device->vid == "80EE"
                || device->vid == "0E0F"
                || device->vid == "0000") {
            if (!device->name.contains("HUAWEI")) {
                continue;
            }
        }

        DBG("device->name [%s]\n", device->name.toLocal8Bit().data());
        if(device->name.contains(QString::fromLocal8Bit("集线器"))
                || device->name.contains(QString::fromLocal8Bit("大容量存储"))
                || device->name.contains(QString::fromLocal8Bit("USB Mass Storage Device"), Qt::CaseInsensitive)
                || device->name.contains(QString::fromLocal8Bit("USB HUB"), Qt::CaseInsensitive)
                || device->name.contains(QString::fromLocal8Bit("主机控制器"))
                || device->name.contains(QString::fromLocal8Bit("HOST CONTROLLER"), Qt::CaseInsensitive)) {
            continue;
        }

        if(!drv->getDriverInfo(device, info, errmsg)) {
//            DBG("get driverInfo failed for [%s], error:%s\n", device->device_id_long.toLocal8Bit().data(),
//                errmsg.toLocal8Bit().data());
            continue;
        }

        QString tmpName = g_tmpDriverPath + getRandomName();
        if(!drv->toZip(info, tmpName, errmsg)) {
            DBG("driver toZip failed for [%s], error:%s\n", device->device_id_long.toLocal8Bit().data(),
                errmsg.toLocal8Bit().data());
            QFile::remove(tmpName);
            continue;
        }

        QString filesum = DriverHelper::getZipFileSum(tmpName);
        if(!checkRemoteDriverExists(drv->osInfo, device->name, device->device_id, remoteInfo, filesum, info.drvVersion, info.bsigned)) {
            DBG("driver remote exists: %s\n", device->name.toLocal8Bit().data());
            emit signal_addLog(device, tr("check remote driver for [%1] failed due to network error").arg(device->name));
            QFile::remove(tmpName);
            continue;
        }

        if(remoteInfo.driver_id == -1) {
            emit signal_addLog(device, tr("remote driver for [%1] not exists, uploading...").arg(device->name));
            DBG("upload driver: %s\n", device->name.toLocal8Bit().data());
            if(!uploadRemoteDriver(*drv, info, tmpName, filesum)) {
                DBG("upload failed!");
            } else {
                DBG("upload succeed!");
                count ++;
            }
        }
        else {
            //DBG("driver_id != -1: %s\n", device->name.toLocal8Bit().data());
        }
        QFile::remove(tmpName);
    }

    emit signal_addLog(NULL, tr("successfully uploaded %1 drivers").arg(count));
#endif
}

DriverInstallThread::DriverInstallThread(DriverHelper *drv, QObject *parent)
    : QThread(parent) {
    this->drv = drv;
}

void DriverInstallThread::run() {
#if 0
    ZDMHttpEx zdm;
    connect(&zdm, SIGNAL(signal_progress(void*,int,int)), SLOT(slot_downloadProgress(void*,int,int)));

    zdm.getFile("http://dl.wandoujia.com/files/phoenix/latest/wandoujia-wandoujia_web.apk"
                , "Z:/wandou.apk");
#else
    DriverInfo info;
    RemoteDrvInfo remoteInfo;

    emit signal_addLog(NULL, tr("start driver install thread..."));
    //drv->reload(false, true); // phonesOnly should not be true to allow unknwon devices
	drv->reload(true, false); // phonesOnly should not be true to allow unknwon devices

    QString errmsg;

    QList<DriverDeviceNode *> list = drv->getDeviceList();
    int count = 0;
    foreach(DriverDeviceNode *device, list) {
        if(device->problem == 0) {
          continue; //测试时注释，发布时要取消注释
        }

        if(device->device_id.contains("ROOT")
                || device->device_id.contains("ACPI")
                || device->vid == "8086"
                || device->vid == "8087"
                || device->vid == "80EE"
                || device->vid == "0E0F"
                || device->vid == "0000"
                || device->vid == "041E"
                || device->vid == "046D") {
            continue;
        }

        //DBG("device->name [%s]\n", device->name.toLocal8Bit().data());
        if(device->name.contains(QString::fromLocal8Bit("集线器"))
                || device->name.contains(QString::fromLocal8Bit("大容量存储"))
                || device->name.contains(QString::fromLocal8Bit("USB Mass Storage Device"), Qt::CaseInsensitive)
                || device->name.contains(QString::fromLocal8Bit("USB HUB"), Qt::CaseInsensitive)
                || device->name.contains(QString::fromLocal8Bit("主机控制器"))
                || device->name.contains(QString::fromLocal8Bit("HOST CONTROLLER"), Qt::CaseInsensitive)) {
            continue;
        }

        //DBG("problem device [%s], problem=%d\n", device->device_id.toLocal8Bit().data(), device->problem);
        emit signal_addLog(device, tr("found problem device [%1], problem = %2").arg(device->name).arg(device->problem));
		//
		remoteInfo.code = 0;
		remoteInfo.driver_id = 0;
        remoteInfo.msg = "";
		remoteInfo.url = "";
		remoteInfo.version = "";
		//
         if(!DriverUploadThread::checkRemoteDriverExists(drv->osInfo, device->name, device->device_id, remoteInfo, "", "", 1)) { // 根据osInfo name device_id 获取 remoteInfo
             if(!DriverUploadThread::checkRemoteDriverExists(drv->osInfo, device->name, device->device_id, remoteInfo, "", "", 0)) {
                emit signal_addLog(device, tr("check remote driver for [%1] failed due to network error").arg(device->name));
                continue;
             }
        }

        if(remoteInfo.driver_id == -1) {
            emit signal_addLog(device, tr("remote driver for [%1] not exists").arg(device->name));
            emit signal_foundDriver(device, false);
            continue;
        }

        emit signal_addLog(device, tr("remote driver for [%1] exists, downloading...").arg(device->name));
        emit signal_foundDriver(device, true);
        QString tmpFile = g_tmpDriverPath + getRandomName();
        QFile f(tmpFile);
        if(!f.open(QIODevice::WriteOnly)) {
            emit signal_addLog(device, tr("download failed, create file error"));
            emit signal_downloadDriver(device, false, tr("Create file error"));
            continue;
        }

        QDataStream fs(&f);
        ZDMHttpEx http;
        http.setArg(device);
        connect(&http, SIGNAL(signal_progress(void*,int,int)), SLOT(slot_downloadProgress(void*,int,int)));
        if(0 != http.get(remoteInfo.url, fs)) {
            emit signal_addLog(device, tr("download failed, network error"));
            emit signal_downloadDriver(device, false, tr("Network error"));
            f.close();
            f.remove();
            continue;
        }
        f.close();

        emit signal_downloadDriver(device, true, tr("Start installation"));
        if(drv->getDriverInfo(tmpFile, info, errmsg)
                && drv->updateDrv(info, tmpFile, device->device_id, errmsg)) {
            emit signal_addLog(device, tr("install succeed"));
            emit signal_installDriver(device, true, tr("Installation succeed"));
            count++;
        } else {
            emit signal_addLog(device, tr("install failed, ")+errmsg);
            emit signal_installDriver(device, false, tr("Installation failed, ")+errmsg);
        }
#ifndef CMD_DEBUG
        f.remove();
#endif
    }
    emit signal_addLog(NULL, tr("successfully installed %1 drivers").arg(count));
#endif
}

void DriverInstallThread::slot_downloadProgress(void *p, int value, int total) {
    emit signal_downloadProgress((DriverDeviceNode*) p, value, total);
}

ProblemCollectThread::ProblemCollectThread(QObject *parent)
    : QThread(parent) {

}

void ProblemCollectThread::run() {
    DriverHelper *h = new DriverHelper(this);
    for(;;) {
        //msleep(3000);
		msleep(3000000);//临时调试时使用，发布时需要恢复为3000毫秒
        h->reload(false, true);
        QList<DriverDeviceNode *> list = h->getDeviceList();
        mutex.lock();
        foreach(DriverDeviceNode *node, list) {
            if(node->problem != 0) {
                //DBG("found problem device '%s'[%d]\n", qPrintable(node->device_id_long), node->problem);
                problemList.insert(node->device_id_long, node->problem);
            } else {
                problemList.remove(node->device_id_long);
            }
        }
        mutex.unlock();
    }
}

QString ProblemCollectThread::getProblemList() {
    QString ret;
    mutex.lock();
    QMapIterator<QString, unsigned long> it(problemList);
    while(it.hasNext()) {
        it.next();
        ret.append(QString("%1=>%2;").arg(it.key()).arg(it.value()));
    }
    mutex.unlock();
    return ret;
}

DriverManager::DriverManager(QWidget *parent)
    : QObject(parent) {
    _parent = parent;

    tInst = new DriverInstallThread(new DriverHelper(this), this);
    connect(tInst, SIGNAL(signal_addLog(DriverDeviceNode*,QString)), SLOT(slot_drvLog(DriverDeviceNode*,QString)));
    connect(tInst, SIGNAL(signal_downloadProgress(DriverDeviceNode*,int,int)), SLOT(slot_downloadProgress(DriverDeviceNode*,int,int)));

    tUpload = new DriverUploadThread(new DriverHelper(this), this);
    connect(tUpload, SIGNAL(signal_addLog(DriverDeviceNode*,QString)), SLOT(slot_drvLog(DriverDeviceNode*,QString)));
    connect(tUpload, SIGNAL(finished()), tInst, SLOT(start()));

    tCollect = new ProblemCollectThread(this);
}

DriverManager::~DriverManager() {

}

void DriverManager::slot_drvError(DriverDeviceNode *device, int code, const QString &hint) {
    //DBG("ERROR %d, %s\n", code, qPrintable(hint));
}

void DriverManager::slot_drvLog(DriverDeviceNode* device, const QString &log) {
    //DBG("LOG %s\n", qPrintable(log));
}

void DriverManager::slot_downloadProgress(DriverDeviceNode* device, int value, int max) {

}

void DriverManager::slot_startWork() {
    tUpload->start();
    tCollect->start();
}

void DriverManager::slot_usbToggled() {
    tInst->start();
}

static void rmdir_recursively(const QString& path) {
    QDir dir(path);
    if(dir.exists()) {
        QFileInfoList entries = dir.entryInfoList(QDir::NoDotAndDotDot|QDir::AllEntries);
        foreach(const QFileInfo& info, entries) {
            QString entry = info.canonicalFilePath();
            //DBG("entry = '%s'\n", entry.toLocal8Bit().data());

            if(info.isDir()) {
                rmdir_recursively(entry);
            } else {
                if (!info.isWritable()) {
                    QFile::setPermissions(entry, (QFile::Permission)0777);
                }
                QFile::remove(entry);
            }
        }

        //DBG("%s rmdir '%s'\n", path.toLocal8Bit().data(), path.toLocal8Bit().data());
        dir.rmdir(path);
    }
}

#endif
