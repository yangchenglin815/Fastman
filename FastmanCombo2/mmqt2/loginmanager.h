#ifndef LOGINMANAGER_H
#define LOGINMANAGER_H

#include <QString>
#include <QStringList>
#include <QUrl>
#include <QDomDocument>
#include "zdatabaseutil.h"

#include <QHostInfo> 
#include <QHostAddress> 


class ZAdbDeviceNode;
class InstLog;

class DevInstLog {
public:
    QStringList pkgList;
    QString user;
};

class UrlEntry {
public:
    QString host;
    int port;
    UrlEntry();
    UrlEntry(const QString& str);
    void fromString(const QString& str);
    QString toString();
    bool isEmpty();
    void clear();
    void copyFrom(const UrlEntry& other);
    bool operator == (const UrlEntry& other);
    bool operator == (const QUrl& other);
};

class UrlConf {
public:
    UrlEntry key;
    UrlEntry prefered;
    QList<UrlEntry*> alternatives;
    ~UrlConf();
};

class UrlConfDb : public ZDatabaseUtil<UrlConf*> {
public:
    UrlConfDb();
    QByteArray toByteArray(UrlConf *obj);
    UrlConf *fromByteArray(const QByteArray &data);
    QString getId(UrlConf *obj);
    void setId(UrlConf *obj, const QString& id);
};

class LoginManager
{
    QString _mac;
    QString _hwid;
    QString _systype;
    QString _ver;

    QString _oriuser;
    QString _encuser;
    QString _encpass;

    QString _client_type;

    UrlConfDb *urlDb;
    QList<UrlConf *> urlList;
    void addUrlConf(const QString &domain, const QString &ip);

    LoginManager();
    ~LoginManager();

    bool getFastestUrl(UrlConf *conf, bool CMCCFirst = false);
    void loadConfigFromXml(const QDomNode &node);
public:
    QHostInfo HostInfo;
    static LoginManager* getInstance();
    void initDb();
    bool initUrlRevoke(QString &hint, bool CMCCFirst = false);
    void resetUrlRevoke();
    void revokeUrl(QUrl& url);
    void revokeUrlString(QString& url_str);

    inline QString getMAC() const {return _mac;}

    enum {
        ERR_NOERR = 0,
        ERR_NETWORK,
        ERR_PARSE,
        ERR_CUSTOM
    };

    bool disableBundleDiy;
    bool disableBundleLocalFile;
    bool disableNavIndex;
    bool disableNavApps;
    bool disableNavAccount;
    bool disableAsync;
    bool disableRoot;

    void fillUrlParams(QUrl& url, QString param = QString(), bool nouid = false);
    void getLoginInfo(QString& userid, QString& encpasswd);
    bool core_login(const QString& user, const QString& pass, QString& hint);
    bool core_login2(const QString& user, const QString& encPasswd, QString& hint);
    bool core_login_offline(const QString& user, const QString& pass, QString& hint);
    bool core_getRemoteApp(const QString& pkgname, const QString &filename, const QString& coId);

    InstLog *prepareInstLog(ZAdbDeviceNode *node,
                            const QString &aggIds,
                            const QString &ids,
                            const QString &hiddenIds,
                            QMap<QString, QString> &uploadDataMap);
    int core_uploadInstLog(const QString& urlparams, QString& hint);
    bool core_getInstLog(DevInstLog& log, const QString& imei);
#ifdef NEW_INTERFACE
    bool core_uploadInstLogNew(ZAdbDeviceNode *node, const QString& ids, QString &hint);
#endif
};

#endif // LOGINMANAGER_H
  
