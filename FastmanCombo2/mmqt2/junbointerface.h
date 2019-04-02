#ifndef JUNBOINTERFACE_H
#define JUNBOINTERFACE_H

#include <QtCore/qglobal.h>
#include <QString>
#include <QList>

#if defined(JUNBOINTERFACE_LIBRARY)
#  define JUNBOINTERFACESHARED_EXPORT Q_DECL_EXPORT
#else
#  define JUNBOINTERFACESHARED_EXPORT Q_DECL_IMPORT
#endif

class JunboArea {
public:
    // for parse only
    QString _parentCode;

    JunboArea *parent;
    QList<JunboArea *> children;

    QString id;
    QString name;
    QString code;

    void print(const QString& indent);
};

class MobileInfo {
public:
    QString mobileMac;
    QString mobileIMEI;
    QString mobileBrand;
    QString mobileType;
};

class ApkInfo {
public:
    QString packageId;
    QString fileName;
    QString packageVersion;
    QString activateStatus;
    QString resourceId;
    QString resourceName;
    QString resourcePackageName;
    QString resourceVersion;
    QString score;
    QString installType;
    QString experienceId;
    QString caid;
};

class JUNBOINTERFACESHARED_EXPORT JunboInterface
{
    QString guid;
    quint64 tstamp;

    QString report_UserId;
    QString report_Id;

    JunboArea *rootArea;
    QList<JunboArea*> allArea;

    QString getOSName();
    QString getOSBits();
    QString getRandom(unsigned int max_num);

    JunboInterface();
    ~JunboInterface();
public:
    static JunboInterface* getInstance();

    QString app_list_name;
    QList<ApkInfo*> appList;
    bool isLoggedIn;

    QString savedUsername;
    QString savedPassword;
    void loadCfg();

    bool getTimeStamp();
    bool getAllArea();
    bool loadAppPackage(QString zip_path, QString dest_path);
    bool mmLogin(const QString &username, const QString &pwd, bool rememberPassword = false);
    bool mmReport(MobileInfo *mobileInfo, ApkInfo *apkInfo);
};

#endif // JUNBOINTERFACE_H
