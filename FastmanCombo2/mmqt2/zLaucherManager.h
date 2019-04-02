#ifndef ZLAUCHERMANAGER_H
#define ZLAUCHERMANAGER_H

//#include "ztaskmanager_global.h"

#include <QObject>
#include <QMap>

//class zAdbNode;

class LauncherConfig;
class LauncherMaker;
class QMutex;

class AppDesc
{
public:
	QString m_qstrApkName;
	QString m_qstrFilePath;
};

class zLaucherManager : public QObject
{
	Q_OBJECT

	
public:
	zLaucherManager();
	enum{Laucher_Install, Laucher_Push};
	static zLaucherManager* getInstance();
	//初始化需要的参数
	void init(const QString& strUser);
	//手机连接上的时候初始化对应的config对象
	void initLaucherConfig(QString s_adbNode);
	//应用安装完成之后，添加应用到列表中
	void addLaucherApp(QString s_adbNode, AppDesc* appDesc, int iType = Laucher_Install);
	//安装完成之后，生成xml文件推送到手机
	void syncLaucher(QString s_adbNode);

public:
	QString strBrand;
	QString strModel;
	QString strPhoneKey;
	QString strUrl;

protected:
	LauncherConfig* findConfig(QString s_adbNode);
	LauncherConfig* createConfig(QString s_adbNode);
	LauncherMaker* createMaker(LauncherConfig* config, QString s_adbNode);

signals:
	void signal_resetDesktopFinished();

private:
	QMap<QString, LauncherConfig*>	m_configMap;
	//QMap<zAdbNode*, LauncherMaker*>	m_makerMap;
	QMap<QString, LauncherMaker*>	m_makerMap;

	QString	m_strUser;

	QMutex*	m_mutex;
};

#endif//ZLAUCHERMANAGER_H