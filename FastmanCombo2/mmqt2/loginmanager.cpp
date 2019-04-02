#include "loginmanager.h"

#include "globalcontroller.h"
#include "appconfig.h"
#include "bundle.h"
#include "zdmhttp.h"
#include "zstringutil.h"
#include "zadbdevicenode.h"
#include "zlog.h"

#include <QTextCodec>
#include <QCryptographicHash>
#include <QUrl>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNode>
#include <QDomNodeList>
#include <QNetworkInterface>
#include <QDateTime>
#include <QTcpSocket>
#include <QFile>
#include <QUuid>
#include <QSettings>
#include <QDir>
#include "QJson/parser.h"
#include "QJson/serializer.h"
#include <QApplication>

static QString calcMd5(const QByteArray& in) {
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(in);
    QByteArray d = hash.result().toHex().toUpper();
    return QString(d);
}

static QString calcMd5(const QByteArray& in, int times) {
    int r = times;
    QByteArray d = in;
    while(r-- > 0) {
        QCryptographicHash hash(QCryptographicHash::Md5);
        hash.addData(d);
        d = hash.result().toHex().toUpper();
    }
    return QString(d);
}

static QString getMacAddr() {
    QList<QNetworkInterface> list = QNetworkInterface::allInterfaces();
    QString ret;
    foreach(const QNetworkInterface& i, list) {
        QString mac = i.hardwareAddress();
        if(mac.isEmpty()) {
            continue;
        } else if(mac.startsWith("00:50:56")) { // vmware
            DBG("skipped vmware adapter %s\n", i.humanReadableName().toLocal8Bit().data());
            continue;
        } else if(mac.startsWith("08:00:27")) { // virtualbox
            DBG("skipped vbox adapter %s\n", i.humanReadableName().toLocal8Bit().data());
            continue;
        } else if(mac.startsWith("00:00:00")) { // emulated
            DBG("skipped emulated adapter\n", i.humanReadableName().toLocal8Bit().data());
            continue;
        }

        ret += mac;
        ret += ",";
    }

    if(ret.isEmpty()) {
        ret = "NULL";
    } else {
        ret.chop(1);
        ret = ret.replace(':', '-');
    }
    return ret;
}

static QString getHWCfgPath() {
    QString path;
#if defined(Q_OS_WIN)
    QSettings regedit("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders"
                      , QSettings::NativeFormat);
    path = regedit.value("Common AppData").toString();
    if(!path.isEmpty()) {
        path += "\\Microsoft\\Network\\hwid.cfg";
    }
#elif defined(Q_OS_LINUX)
    QString path_win7 = "/mnt/live/memory/data/ProgramData/Microsoft/Network";
    QString path_xp = "/mnt/live/memory/data/Documents and Settings/All Users/Application Data/Microsoft/Network";
    if(QFile::exists(path_win7)) {
        path = path_win7 + "/hwid.cfg";
    } else if(QFile::exists(path_xp)) {
        path = path_xp + "/hwid.cfg";
    }
#endif
    DBG("getHWCfgPath <%s>\n", TR_C(path));
    return path;
}

static QString getHWid() {
    QString ret;
    QString path = getHWCfgPath();
    if(path.isEmpty()) {
        return "";
    }

    QFile f(path);
    if(f.open(QIODevice::ReadOnly)) {
        ret = f.readAll();
        f.close();
    } else {
        DBG("getHWid error: cannot read cfg\n");
    }
    return ret;
}

static void setHWid(const QString& hwid) {
    QString path = getHWCfgPath();
    if(path.isEmpty()) {
        return;
    }

    QFile f(path);
    if(f.open(QIODevice::WriteOnly)) {
        f.write(hwid.toUtf8());
        f.close();
    } else {
        DBG("setHWid error: cannot write cfg\n");
    }
}

UrlEntry::UrlEntry() {
    port = -1;
}

UrlEntry::UrlEntry(const QString &str) {
    fromString(str);
}

void UrlEntry::fromString(const QString &str) {
    int n = str.lastIndexOf(':');
    if(n == -1) {
        host = str;
        port = 80;
    } else {
        host = str.left(n);
        port = str.mid(n+1).toInt();
    }
}

QString UrlEntry::toString() {
    return QString("%1:%2").arg(host).arg(port);
}

bool UrlEntry::isEmpty() {
    return port == -1;
}

void UrlEntry::clear() {
    host.clear();
    port = -1;
}

void UrlEntry::copyFrom(const UrlEntry &other) {
    host = other.host;
    port = other.port;
}

bool UrlEntry::operator == (const UrlEntry& other) {
    return host == other.host && port == other.port;
}

bool UrlEntry::operator == (const QUrl& other) {
    return host == other.host() && port == other.port(80);
}

UrlConf::~UrlConf() {
    while(!alternatives.isEmpty()) {
        delete alternatives.takeFirst();
    }
}

UrlConfDb::UrlConfDb()
    : ZDatabaseUtil<UrlConf*>("urlconfNew") {

}

QByteArray UrlConfDb::toByteArray(UrlConf *obj) {
    ZByteArray d(false);
    QStringList list;
    foreach(UrlEntry *entry, obj->alternatives) {
        list.append(entry->toString());
    }
    d.putUtf8(obj->prefered.toString());
    d.putUtf8(list.join(","));
    return d;
}

UrlConf *UrlConfDb::fromByteArray(const QByteArray &data) {
    ZByteArray d(false);
    UrlConf *obj = new UrlConf();

    int i = 0;
    d.append(data);
    obj->prefered.fromString(d.getNextUtf8(i));
    QStringList list = d.getNextUtf8(i).split(",", QString::SkipEmptyParts);
    foreach(const QString& url, list) {
        obj->alternatives.append(new UrlEntry(url));
    }
    return obj;
}

QString UrlConfDb::getId(UrlConf *obj) {
    return obj->key.toString();
}

void UrlConfDb::setId(UrlConf *obj, const QString &id) {
    obj->key.fromString(id);
}

void LoginManager::addUrlConf(const QString &domain, const QString &ip)
{
    DBG("check add url confg: domain=%s, ip=%s\n", qPrintable(domain), qPrintable(ip));

    UrlEntry key(domain);  // auto add port: 80 by this construction
    UrlEntry value(ip);
    UrlConf *target = NULL;

    // check domain
    bool found = false;
    foreach (UrlConf *conf, urlList) {
        if (conf->key == key) {
            target = conf;
            found = true;
            break;
        }
    }
    if (!found) {
        DBG("not found domain, add a new one\n");
        target = new UrlConf();
        target->key.copyFrom(key);
        urlList.append(target);
    }

    // check IP
    found = false;
    foreach (UrlEntry *entry, target->alternatives) {
        if (*entry == value) {
            found = true;
            break;
        }
    }
    if (!found) {
        DBG("not found ip, add a new one\n");
        target->alternatives.append(new UrlEntry(ip));
    }

    urlDb->setData(target);
}

LoginManager::LoginManager() {
    DBG("+LoginManager %p\n", this);
    urlDb = NULL;

    disableBundleDiy = false;
    disableBundleLocalFile = false;
    disableNavIndex = false;
    disableNavApps = false;
    disableNavAccount = false;
    disableAsync = false;
    disableRoot = false;

    _mac = getMacAddr();
    _hwid = getHWid();
#if defined(Q_OS_WIN)
    _systype = "win";
#elif defined(Q_OS_LINUX)
    _systype = "linux";
#elif defined(Q_OS_OSX)
    _systype = "osx";
#endif
    _ver = MMQT2_VER;
}

LoginManager::~LoginManager() {
    DBG("~LoginManager %p\n", this);
#ifdef CMD_DEBUG
    QSettings ini("host.ini", QSettings::IniFormat);
    ini.setValue("mac", _mac);
    ini.setValue("hwid", _hwid);
    ini.setValue("sys", _systype);
    ini.setValue("ver", _ver);
    ini.sync();
#endif
    if(urlDb != NULL) {
        delete urlDb;
    }
    while(!urlList.isEmpty()) {
        delete urlList.takeFirst();
    }
}

LoginManager* LoginManager::getInstance() {
    static LoginManager instance;
    return &instance;
}

void LoginManager::initDb() {
    urlDb = new UrlConfDb();
    urlList = urlDb->getAllData();
    addUrlConf("sjapk.dengxian.net", "ydsjapk.dengxian.net");
    addUrlConf("zjadmin.dengxian.net", "ydzjadmin.dengxian.net");
}

bool LoginManager::getFastestUrl(UrlConf *conf, bool CMCCFirst) {
    DBG("get fastest url\n");

    if (CMCCFirst && ("sjapk.dengxian.net:80" == conf->key.toString())) {
        DBG("CMCC server first\n");
        conf->prefered.copyFrom(UrlEntry("ydsjapk.dengxian.net:80") );
        return true;
    }
    if (CMCCFirst && ("zjadmin.dengxian.net:80" == conf->key.toString())) {
        DBG("CMCC server first\n");
        conf->prefered.copyFrom(UrlEntry("ydzjadmin.dengxian:80") );
        return true;
    }

    int timeout = -1;
    QList<UrlEntry*> urlList;
    urlList.append(&conf->key);
    urlList.append(conf->alternatives);

    foreach(UrlEntry *entry, urlList) {
        QTcpSocket socket;
        QTime timer;
        int time_passed;

        DBG("try %s @ port %d\n", entry->host.toLocal8Bit().data(), entry->port);
        timer.start();
        socket.connectToHost(entry->host, entry->port);
        if(socket.waitForConnected(10000)) {
            time_passed = timer.elapsed();
            DBG("connected @ %d\n", time_passed);
            if(time_passed < 1000) {
                conf->prefered.copyFrom(*entry);
                break;
            }
            if(timeout == -1 || time_passed < timeout) {
                timeout = time_passed;
                conf->prefered.copyFrom(*entry);
            }
        } else {
            DBG("error: %d\n", socket.error());
        }
        socket.abort();
    }

    return !conf->prefered.isEmpty();
}

bool LoginManager::initUrlRevoke(QString& hint, bool CMCCFirst) {
    bool ret = true;
    foreach(UrlConf *conf, urlList) {
        DBG("url [%s] prefered: <%s>\n", conf->key.toString().toLocal8Bit().data(),
            conf->prefered.toString().toLocal8Bit().data());
        if(!getFastestUrl(conf, CMCCFirst)) {
            hint = QObject::tr("get fastest host for %1 failed").arg(conf->key.toString());
            ret = false;
            break;
        }
        DBG("==> result %s\n\n", conf->prefered.toString().toLocal8Bit().data());
        urlDb->setData(conf);
    }
    return ret;
}

void LoginManager::resetUrlRevoke() {
    foreach(UrlConf *conf, urlList) {
        conf->prefered.clear();
    }
    urlDb->setAllData(urlList);
}

void LoginManager::revokeUrl(QUrl &url) {
    if (url.port() == -1) {
        url.setPort(80);
    }
    QString sUrl = url.host().toLocal8Bit().data();
    DBG("look up for key <%s>\n", sUrl.toLocal8Bit().data());
    foreach(UrlConf *conf, urlList) {
        if(conf->key == url && !conf->prefered.isEmpty()) {
            DBG("conf->key == url and url is <%s>\n", sUrl.toLocal8Bit().data());
            url.setHost(conf->prefered.host);
            url.setPort(conf->prefered.port);
            sUrl = url.toString().toLocal8Bit().data();
            DBG("revoke as <%s>\n", sUrl.toLocal8Bit().data());
            break;
        }
    }
}

void LoginManager::revokeUrlString(QString &url_str) {
    QUrl url(url_str);
    revokeUrl(url);
    url_str = url.toString();
}

void LoginManager::fillUrlParams(QUrl& url, QString param, bool nouid) {
    uint now = QDateTime::currentDateTime().toTime_t();
    _client_type = "speed";
    QString ret = QString("ver=%1&mac=%2&hwid=%3&random=%4&sys=%5&client_type=%6")
            .arg(_ver).arg(_mac).arg(_hwid).arg(now).arg(_systype).arg(_client_type);
    if(!nouid && !_encuser.isEmpty()) {
        ret += QString("&userid=%1&pwd=%2").arg(_encuser, _encpass);
    }

    if(!param.isEmpty()) {
        ret += param;
    }
#if 0 //def NEW_INTERFACE
    ret += "&type=0";
#endif

    DBG("getUrlParams=<%s>\n", ret.toLocal8Bit().data());
    revokeUrl(url);
#ifdef USE_QT5
    url.setQuery(ret);
#else
    url.setEncodedQuery(ret.toLocal8Bit());
#endif
}

void LoginManager::getLoginInfo(QString &userid, QString &encpasswd) {
    userid = _oriuser;
    encpasswd = _encpass;
}

bool LoginManager::core_login(const QString &user, const QString &pass, QString &hint) {
    QString encPasswd = calcMd5(pass.toUtf8(), 3);
    return core_login2(user, encPasswd, hint);
}

bool LoginManager::core_login_offline(const QString &user, const QString &pass, QString &hint) {
    AppConfig *config = GlobalController::getInstance()->appconfig;
    QString encUserid;
    QString encPasswd = pass;
    ZDMHttp::url_escape(ZStringUtil::toGBK(user), encUserid);

    if(config->username != user) {
        hint = QObject::tr("stored username mismatch");
        return false;
    }

    if(config->encpasswd.isEmpty()) {
        hint = QObject::tr("stored password not exist, please login online first");
        return false;
    }

    if(config->encpasswd != encPasswd) {
        hint = QObject::tr("stored password mismatch!");
        return false;
    }

    QFile xmlcache("user.dat");
    if(!xmlcache.open(QIODevice::ReadOnly)) {
        hint = QObject::tr("user.dat not found!");
        return false;
    }
    QByteArray out = xmlcache.readAll();
    ZByteArray::decode_bytearray(out);
    xmlcache.close();

    QDomDocument dom;
    QDomNodeList nodes;
    if (!dom.setContent(out, false)
            || (nodes = dom.elementsByTagName("user")).count() < 1) {
        hint = QObject::tr("user.dat corrupt!");
        return false;
    }

    loadConfigFromXml(nodes.at(0));
    _oriuser = user;
    _encuser = encUserid;
    _encpass = encPasswd;
    return true;
}

void LoginManager::loadConfigFromXml(const QDomNode &node) {
    disableBundleDiy = node.firstChildElement("disable_bundle_diy").text().toInt() == 1;
    disableBundleLocalFile = node.firstChildElement("disable_add_local_file").text().toInt() == 1;
    disableNavIndex = node.firstChildElement("disable_index").text().toInt() == 1;
    disableNavApps = node.firstChildElement("disable_apps").text().toInt() == 1;
    disableNavAccount = node.firstChildElement("disable_account").text().toInt() == 1;
    disableAsync = node.firstChildElement("issync").text().toInt() == 0;
    disableRoot = node.firstChildElement("isroot").text().toInt() == 0;
}

bool LoginManager::core_login2(const QString &user, const QString &encPasswd, QString &hint) {
    AppConfig *config = GlobalController::getInstance()->appconfig;

    QDir dir;
    dir.remove(qApp->applicationDirPath() + "/DesktopConfiguration.ini");

    QUrl url("http://sjapk.dengxian.net/login.aspx");
    revokeUrl(url);

    QString encUserid;
    ZDMHttp::url_escape(ZStringUtil::toGBK(user), encUserid);
    fillUrlParams(url, QString("&userid=%1&pwd=%2").arg(encUserid, encPasswd), true);

    QByteArray out;
    char *uri = strdup(url.toEncoded().data());
    if(0 != ZDMHttp::get(uri, out)) {
        hint = QObject::tr("connect server fail");
        free(uri);
        return false;
    }
    DBG("core_login2 url=<%s>", uri);
    free(uri);

    QDomDocument dom;
    QDomNodeList nodes;
    if (!dom.setContent(out, false)
            || (nodes = dom.elementsByTagName("user")).count() < 1) {
        hint = QObject::tr("invalid xml response");
        return false;
    }

    QDomNode node = nodes.at(0);
    QString str = node.firstChildElement("comment").text();
    if(str != "success") {
        hint = str;
        return false;
    }

    QString hiddenTCShowflag = node.firstChildElement("showHiddenTC").text();
    if(hiddenTCShowflag == "true"){
        config->hiddenTCShowFlag = true;
        config->showHiddenTCTime = node.firstChildElement("showHiddenTCTime").text();
    }else{
        config->hiddenTCShowFlag = false;
    }

    QString allowYunOSReplaceIconStr = node.firstChildElement("AllowYunOSReplaceIcon").text();
    if ("false" == allowYunOSReplaceIconStr) {
        config->bAllowYunOSReplaceIcon = false;
    } else {
        config->bAllowYunOSReplaceIcon = true;
    }

    QString allowPCUploadInstallResultWhenAsyncStr = node.firstChildElement("SyncReport").text();
    if ("false" == allowPCUploadInstallResultWhenAsyncStr) {
        config->bAllowPCUploadInstallResultWhenAsync = false;
    } else {
        config->bAllowPCUploadInstallResultWhenAsync = true;
    }

    QString strDefaultDesktop = node.firstChildElement("IsMZofficialDesktop").text();
    if(strDefaultDesktop == "1") {
        config->MZDesktop = true;
    } else{
        config->MZDesktop = false;
    }

    config->YAMUser = (node.firstChildElement("isyam").text().toInt() == 1);
    config->CustomDesktop = (node.firstChildElement("customdesktop").text().toInt() == 1);

    hint = node.firstChildElement("login_hint").text();
    _oriuser = user;
    _encuser = encUserid;
    _encpass = encPasswd;

    str = node.firstChildElement("hwid").text();
    if(_hwid != str) {
        _hwid = str;
        setHWid(_hwid);
    }

    QDomNodeList urlNodes = dom.elementsByTagName("url");
    for(int i=0; i<urlNodes.count(); i++) {
        QDomElement urlNode = urlNodes.at(i).toElement();
        QString domain = urlNode.attribute("domain");
        QStringList ipList = urlNode.attribute("ip").split(",");

        UrlEntry key(domain);  // auto add port: 80 by this construction
        UrlConf *target = NULL;
        foreach(UrlConf *conf, urlList) {
            if(conf->key == key) {
                target = conf;
                break;
            }
        }

        if(target == NULL) {
            target = new UrlConf();
            target->key.copyFrom(key);
            urlList.append(target);
        } else {
            while(!target->alternatives.isEmpty()) {
                delete target->alternatives.takeFirst();
            }
        }
        foreach(const QString& ip, ipList) {
            target->alternatives.append(new UrlEntry(ip));
        }
    }
#ifdef CMD_DEBUG
    DBG("==== url map ====\n");
    foreach(UrlConf *conf, urlList) {
        DBG("%s <-> %s\n", conf->key.toString().toLocal8Bit().data(),
            conf->prefered.toString().toLocal8Bit().data());
        foreach(UrlEntry *entry, conf->alternatives) {
            DBG("\t%s\n", entry->toString().toLocal8Bit().data());
        }
    }
    DBG("==== url map ====\n");
#endif
    urlDb->clearData();
    xsleep(1000);
    urlDb->setAllData(urlList);
    loadConfigFromXml(node);

    ZByteArray::encode_bytearray(out);
    QFile xmlcache("user.dat");
    if(xmlcache.open(QIODevice::WriteOnly)) {
        xmlcache.write(out);
        xmlcache.close();
    }

    DBG("hint = <%s>\n", hint.toLocal8Bit().data());
    return true;
}

bool LoginManager::core_getRemoteApp(const QString &pkgname, const QString &filename, const QString& coId) {
    QString ver = "1.0.0";
    if(QFile::exists(filename)) {
        BundleItem item;
        if(item.parseApk(filename) && item.pkg == pkgname) {
            ver = item.version;

        } else {
            DBG("invalid local file %s, remove\n", qPrintable(filename));
            QFile::remove(filename);
        }
    }

    QString now = QString::number(QDateTime::currentDateTime().toTime_t());
    QString md5 = calcMd5((pkgname+ver+coId+now).toLocal8Bit(), 1).toLower();

    QUrl url("http://sign.92.net/upgradeApp.aspx");
    revokeUrl(url);

    QString mustrefresh = "1";
    QString query = QString("packname=%1&auth=%2&t=%3&coid=%4&ver=%5&mustrefresh=%6").arg(pkgname, md5, now, coId, ver,mustrefresh);
#ifdef USE_QT5
    url.setQuery(query);
#else
    url.setEncodedQuery(query.toLocal8Bit());
#endif
    QByteArray out;
    char *uri = strdup(url.toEncoded().data());
    if(0 != ZDMHttp::get(uri, out)) {
        DBG("connect server fail\n");
        free(uri);
        return false;
    }
    free(uri);
    DBG("out==>\n%s\n", QString::fromUtf8(out).toLocal8Bit().data());

    QDomDocument dom;
    QDomNodeList nodes;
    if (!dom.setContent(out, false)
            || (nodes = dom.elementsByTagName("update")).count() < 1) {
        DBG("invalid xml response\n");
        return false;
    }
    QDomNode node = nodes.at(0);
    QString newVer = node.firstChildElement("ver").text();
    QString downUrl = node.firstChildElement("down_url").text();
    if(newVer.isEmpty()) {
        DBG("invalid xml response 2\n");
        return false;
    }

    if(newVer != ver) {
        DBG("found new version of %s (%s)\n", qPrintable(pkgname), qPrintable(newVer));
        DBG("url => [%s]\n", qPrintable(downUrl));
    } else {
        DBG("already updated, return\n");
        return true;
    }

    // download
    QFile tmpF(filename+".dm!");
    if(!tmpF.open(QIODevice::WriteOnly)) {
        DBG("open file for read fail!\n");
        return false;
    }

    uri = strdup(downUrl.toLocal8Bit().data());
    QDataStream stream(&tmpF);
    if(0 != ZDMHttp::get(uri, stream)) {
        DBG("download fail!\n");
        free(uri);
        tmpF.close();
        tmpF.remove();
        return false;
    }
    free(uri);
    tmpF.close();

    QFile::remove(filename);
    tmpF.rename(filename);
    DBG("download success!\n");
    return true;
}

InstLog *LoginManager::prepareInstLog(ZAdbDeviceNode *node,
                                      const QString &aggIds,
                                      const QString &ids,
                                      const QString &hiddenIds,
                                      QMap<QString, QString> &uploadDataMap) {
    AppConfig *config = GlobalController::getInstance()->appconfig;
    QString idsModify = ids;
    QString hiddenIdsModify = hiddenIds;

    uploadDataMap.clear();
#ifdef DOWNLOAD_DIR_USE_USERNAME
    idsModify.remove(config->username);
    hiddenIdsModify.remove(config->username);
#endif

    QString systype = "unknown";
#if defined(Q_OS_WIN)
    systype = "win";
#elif defined(Q_OS_LINUX)
    systype = "linux";
#elif defined(Q_OS_OSX)
    systype = "osx";
#endif

    QString enc_imei;
    QString enc_aggids;
    QString enc_ids;
    QString enc_brand;
    QString enc_model;
    ZDMHttp::url_escape(node->ag_info.imei.toUtf8(), enc_imei);
    ZDMHttp::url_escape(aggIds.toUtf8(), enc_aggids);
    ZDMHttp::url_escape(idsModify.toUtf8(), enc_ids);
    ZDMHttp::url_escape(node->ag_info.brand.toUtf8(), enc_brand);
    ZDMHttp::url_escape(node->ag_info.model.toUtf8(), enc_model);


    QString query = QString("agg_id=%1&id=%2&imei=%3&pwd=%4&userid=%5&ver=%6")
            .arg(enc_aggids, enc_ids, enc_imei, _encpass, _encuser, MMQT2_VER);
    QString key = calcMd5(query.toUtf8(), 5);

    query += QString("&key=%1&brand=%2&model=%3&version_release=%4&version_sdk=%5&rooted=%6&agged=%7&mac=%8&hwid=%9&sys=%10")
            .arg(key, enc_brand, enc_model, node->ag_info.version, QString::number(node->ag_info.sdk_int))
            .arg(QString::number(node->hasRoot), QString::number(node->hasAgg), _mac, _hwid, systype);
    query += QString("&platform=%1").arg(node->platform);
    query += QString("&hid=%1").arg(hiddenIdsModify);
    query += QString("&cmd=1&client_type=speed");  // 显示套餐详情

    if(!config->opername.isEmpty()) {
        query += "&ywun=";
        query += config->opername;
    }
    query += QString("&tcid=%1").arg(node->sBundleID);
    if(config->printfLog){
        DBG("upload query:%s\n",query.toUtf8().data());
    }

    // NOTE: uploadIds, hiddenIds都是由ZM去组织
    QUrl url(config->sUrl + "/install.aspx");
    revokeUrl(url);
    uploadDataMap.insert("url", url.toString());
    uploadDataMap.insert("agg_id", enc_aggids);
    uploadDataMap.insert("imei", enc_imei);
    uploadDataMap.insert("pwd", _encpass);
    uploadDataMap.insert("userid", _encuser);
    uploadDataMap.insert("ver", QString(MMQT2_VER));
    uploadDataMap.insert("brand", enc_brand);
    uploadDataMap.insert("model", enc_model);
    uploadDataMap.insert("version_release", node->ag_info.version);
    uploadDataMap.insert("version_sdk", QString::number(node->ag_info.sdk_int));
    uploadDataMap.insert("rooted", QString::number(node->hasRoot));
    uploadDataMap.insert("agged", QString::number(node->hasAgg));
    uploadDataMap.insert("mac", _mac);
    uploadDataMap.insert("hwid", _hwid);
    uploadDataMap.insert("sys", systype);
    uploadDataMap.insert("ywun", config->opername);
    uploadDataMap.insert("platform", node->platform);
    uploadDataMap.insert("tcid", node->sBundleID);

    QJson::Serializer json;
    QVariantMap jmap;
    jmap.insert("user", config->username);
    jmap.insert("oper", config->opername);
    jmap.insert("imei", node->ag_info.imei);
    jmap.insert("brand", node->brand);
    jmap.insert("model", node->model);
    jmap.insert("sales_code", node->sales_code);
    jmap.insert("aggs", aggIds);
    jmap.insert("list", idsModify);
    jmap.insert("hasAgg", node->hasAgg);
    jmap.insert("hasRoot", node->hasRoot);

    InstLog *d = new InstLog();
    d->id = QUuid::createUuid().toString();
    d->time = QDateTime::currentDateTime().toTime_t();
    d->json = QString::fromUtf8(json.serialize(jmap));
    d->url = query;
    d->adbSerial = node->adb_serial;

    // check isInstalled
    QUrl checkInstalledUrl(config->sUrl + "/checkinstallImei.aspx");
    revokeUrl(checkInstalledUrl);
    QString checkInstalledQuery = QString("imei=%1").arg(node->ag_info.imei);
#ifdef USE_QT5
    url.setQuery(checkInstalledQuery);
#else
    checkInstalledUrl.setEncodedQuery(checkInstalledQuery.toLocal8Bit());
#endif

    QByteArray out;
    char *uri = strdup(checkInstalledUrl.toEncoded().data());
    int times = 3;
    while (times--) {
        if (0 != ZDMHttp::get(uri, out)) {
            if (times == 0) {
                DBG("connect server fail\n");
            }
        } else {
            break;
        }
    }
    if(config->printfLog){
        DBG("<%s>\nout = <%s>\n", uri, out.data());
    }
    free(uri);

    QJson::Parser p;
    QVariantMap map = p.parse(ZStringUtil::fromGBK(out).toUtf8()).toMap();

    d->isInstalled = map.value("IsInstalled").toBool();
    d->canReplace = map.value("CanReplace").toBool();
    d->installHint = map.value("Message").toString();

    return d;
}

int LoginManager::core_uploadInstLog(const QString &urlparams, QString &hint) {
    hint.clear();

    AppConfig *config = GlobalController::getInstance()->appconfig;
    if (!config->useLAN) {
        QStringList urls;
        if(config->CMCCFirst){
            urls << "http://ydsjapk.dengxian.net/install.aspx"
                 << "http://61.160.248.66:90/install.aspx";
        }else{
            urls << "http://sjapk.dengxian.net/install.aspx"
                 << "http://61.160.248.66:90/install.aspx";
        }

        QByteArray out;
        foreach (const QString &var, urls) {
            QUrl url(var);
            revokeUrl(url);
#ifdef USE_QT5
            url.setQuery(urlparams);
#else
            url.setEncodedQuery((urlparams).toLocal8Bit());
#endif
            char *uri = strdup(url.toEncoded().data());
            if (0 != ZDMHttp::get(uri, out)) {
                DBG("url=<%s>, network error\n", uri);
                free(uri);

                if (var != urls.last()) {
                    continue;
                }

                hint = QObject::tr("network error");
                return ERR_NETWORK;
            }
            if(config->printfLog){
                DBG("<%s>\nout = <%s>\n", uri, out.data());
            }
            free(uri);

            break;
        }

        if (!out.startsWith("OK")) {
            hint = ZStringUtil::fromGBK(out);
            return ERR_CUSTOM;
        } else {
            hint = ZStringUtil::fromGBK(out);
        }

        return ERR_NOERR;
    } else {
        // 采用局域网上报
        QUrl url(QString("http://%1:%2/SIAService/SaveInstallLog").arg(config->LANUrl).arg(config->LANPort));
        revokeUrl(url);

        QString urlparamsModify = urlparams;
        urlparamsModify.replace("%2c", ",");
        if(config->printfLog){
            DBG("get urlparamsModify=<%s\n>", qPrintable(urlparamsModify));
        }

        QString IMEIs;
        QStringList list = urlparamsModify.split("&");
        foreach (const QString &param, list) {
            if (param.startsWith("imei=")) {
                IMEIs = QString(param).remove("imei=");
                if(config->printfLog){
                    DBG("get IMEIs=<%s\n>", qPrintable(IMEIs));
                }
                break;
            }
        }

        QJson::Serializer json;
        QVariantMap jmap;
        jmap.insert("imeis", IMEIs);
        jmap.insert("queryparam", urlparamsModify);
        QByteArray jsonData = json.serialize(jmap);
        if(config->printfLog){
            DBG("url=<%s>, jsonData=<%s>\n", qPrintable(url.toString()), jsonData.data());
        }

        QByteArray out;
        if (0 != ZDMHttp::postJson(url.toString().toUtf8().data(), jsonData.data(), out)) {
            hint = QObject::tr("network error");
            return ERR_NETWORK;
        }
        if(config->printfLog){
            DBG("out=<%s>\n", out.data());
        }

        return ERR_NOERR;
    }
}

bool LoginManager::core_getInstLog(DevInstLog &log, const QString &imei) {
    AppConfig *config = GlobalController::getInstance()->appconfig;
    QUrl url(config->sUrl + "/get_apps.aspx");
    revokeUrl(url);
    QString query = QString("imei=%1").arg(imei);
#ifdef USE_QT5
    url.setQuery(query);
#else
    url.setEncodedQuery(query.toLocal8Bit());
#endif

    QByteArray out;
    char *uri = strdup(url.toEncoded().data());
    if(0 != ZDMHttp::get(uri, out)) {
        DBG("connect server fail\n");
        free(uri);
        return false;
    }
    DBG("<%s>\nout = <%s>\n", uri, out.data());
    free(uri);

    QJson::Parser p;
    QVariantList list = p.parse(out).toList();
    foreach(const QVariant& v, list) {
        QVariantMap m = v.toMap();
        QString pkg = m.value("pkg").toString();
        DBG("found instLog: %s\n", pkg.toLocal8Bit().data());
        log.pkgList.append(pkg);
    }
    return true;
}

#ifdef NEW_INTERFACE
static QString getSignRequest(const QMap<QString, QString> &map, const QString& appSecret) {
    QStringList keys = map.keys();
    keys.sort();

    QString str = appSecret;
    foreach(const QString& key, keys) {
        QString value = map.value(key);
        if(key.isEmpty() || value.isEmpty()) {
            continue;
        }

        str.append(key);
        str.append(value);
    }
    DBG("str=<%s>\n", str.toLocal8Bit().data());
    QString md5 = calcMd5(str.toUtf8());
    DBG("md5=<%s>\n", md5.toLocal8Bit().data());
    return md5;
}

static QString getReqFromMap(const QMap<QString, QString> &map) {
    QString ret;
    bool first = true;
    QMapIterator<QString,QString> it(map);
    while(it.hasNext()) {
        it.next();
        QString key;
        QString value;
        ZDMHttp::url_escape(it.key().toUtf8(), key, true);
        ZDMHttp::url_escape(it.value().toUtf8(), value, true);
        if(first) {
            ret += QString("%1=%2").arg(key, value);
            first = false;
        } else {
            ret += QString("&%1=%2").arg(key, value);
        }
    }
    return ret;
}

bool LoginManager::core_uploadInstLogNew(ZAdbDeviceNode *node, const QString &ids, QString &hint) {
    AppConfig *config = GlobalController::getInstance()->appconfig;
    QUrl url("http://61.160.248.66:8080/api/Install/Setup");
    revokeUrl(url);
    QString systype = "unknown";
#if defined(Q_OS_WIN)
    systype = "win";
#elif defined(Q_OS_LINUX)
    systype = "linux";
#elif defined(Q_OS_OSX)
    systype = "osx";
#endif

    QMap<QString, QString> map;
    map.insert("Appkey", "635398993563915124");
    map.insert("Imei", node->ag_info.imei);
    map.insert("Brand", node->ag_info.brand);
    map.insert("Model", node->ag_info.model);
    map.insert("Customerno", "C00002"); // customer id
    map.insert("Memberid", "2"); // operator id
    map.insert("Membername", config->username); // username
    map.insert("Packid", "2"); // bundle id
    map.insert("Apkids", ids);
    map.insert("Productid", "mz200");
    map.insert("Version", MMQT2_VER);
    map.insert("Isrooted", QString::number(node->hasRoot));
    map.insert("Installtype", QString::number(node->hasAgg));

    QString appSecret = "583c46644738439cbb20c9c2de2a1ad8";
    map.insert("Sign", getSignRequest(map, appSecret));

    QString query = getReqFromMap(map);
    DBG("query = <%s>\n", query.toLocal8Bit().data());

    QByteArray out;
    char *uri = strdup(url.toEncoded().data());
    if(0 != ZDMHttp::post(uri, query.toLocal8Bit().data(), out)) {
        free(uri);
        hint = QObject::tr("network error");
        return false;
    }
    DBG("<%s>\nout = <%s>\n", uri, out.data());
    free(uri);

    if(out != "OK") {
        hint = ZStringUtil::fromUTF8(out);
        return false;
    }
    return true;
}
#endif
