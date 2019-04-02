#ifndef LAUNCHERCONFIG_H
#define LAUNCHERCONFIG_H
#include <QString>
#include <QQueue>
#include <QObject>
#include <QMap>
#include <QStringList>
#include "zdatabaseutil.h"

/**
 * @brief The LauncherConfig class
 * LauncherConfig config = new LauncherConfig();
 * LauncherMaker maker = new LauncherMaker(&config,serial);
 */

struct PkgNode{
        QString pkgname;
		QString classname;
        bool inside;
        QString foldername;
};

struct ReplaceNode{
        QString pkgname;
		QString classname;
        QString lName;
        QString foldername;
};

struct PosNode{
        int screen;
        int x;
        int y;
        int xn;
        int yn;
#if 1
        QString type;
#else
        int type;
#endif
        bool autoselect;
        QString name;
        QString pname;
        QString cname;
		int containID;
        int istrashcan;
};



class LauncherConfig : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief The PkgNode struct
     */

    /**
     * @brief The PosNode struct
     */
    /// 应用程序类型
    const int static TYPE_A = 1;
    /// 文件夹类型
    const int static TYPE_F = 2;
    /// 系统挂件类型
    const int static TYPE_W = 3;

    QQueue<PosNode> _posOrder;	
    QQueue<PosNode> _folderOrder;
    QQueue<PosNode> _widgetOrder;
    QQueue<PosNode> _fixedPkg;
    QQueue<PkgNode> _pkgOrder;
    QQueue<ReplaceNode>_replaceOrder;
	QString sUninstallList;
	QString User;
	QString Brand;
	QString Model;
	QString Tc;
	QString Ispublic;
	QString sys;
    bool flag;
    bool offLine;
    bool standardFlag;

	QMap<QString,QString> _trashcanapps;
public:
    LauncherConfig(QObject* parent);
    void savedesktopini(QString id,QString data,QString TC);
    bool init(const char* url);
    void readini(QString model,QString TC);
    bool contains(const char* pkgname);
    bool inside(const char* pkgname,QString& folder);
    //inline int size(){return _posOrder.size();}
	int size();
    const PosNode& at(int it);	
    ~LauncherConfig();	
};

#endif // LAUNCHERCONFIG_H
