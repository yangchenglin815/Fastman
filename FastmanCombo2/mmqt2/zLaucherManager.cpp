#include "zLaucherManager.h"
#include "launchermaker.h"
#include "launcherconfig.h"
#include "loginmanager.h"
#include "zlog.h"

#include <QMutex>
#include "adbclient.h"

zLaucherManager::zLaucherManager()
{
	m_mutex = new QMutex;
}

static zLaucherManager* g_LaucherManager = NULL;
zLaucherManager* zLaucherManager::getInstance()
{
	if (NULL == g_LaucherManager)
	{
		g_LaucherManager = new zLaucherManager;
	}
	return g_LaucherManager;
}

void zLaucherManager::init(const QString& strUser)
{
	m_strUser = strUser;
}

void zLaucherManager::initLaucherConfig(QString s_adbNode)
{

	if ("" == s_adbNode)
	{
		DBG("Param error!");
		return;
	}

	if (findConfig(s_adbNode))
	{
		return;
	}

	createConfig(s_adbNode);

}

void zLaucherManager::addLaucherApp(QString s_adbNode, AppDesc* appDesc, int iType)
{

	if (s_adbNode == NULL  || NULL == appDesc)
	{
		DBG("Param error!");
		return;
	}

	LauncherConfig* config = findConfig(s_adbNode);
	if (!config)
	{
		return;
	}

	LauncherMaker* maker = createMaker(config, s_adbNode);
	if (!maker)
	{
		DBG("Create maker failed!");
		return;
	}

	if (Laucher_Install == iType)
	{
		if (!maker->install(appDesc->m_qstrApkName.toUtf8().data(), appDesc->m_qstrFilePath.toUtf8().data()))
		{
			DBG("add install apk failed!");
		}
	}
	else if (Laucher_Push == iType)
	{
		if (!maker->push(appDesc->m_qstrApkName.toUtf8().data(), appDesc->m_qstrFilePath.toUtf8().data()))
		{
			DBG("add push apk failed!");
		}
	}

}

void zLaucherManager::syncLaucher(QString s_adbNode)
{
	QMutexLocker locker(m_mutex);

	LauncherMaker* maker = m_makerMap.value(s_adbNode, NULL);
	if (!maker)
	{
		DBG("no maker!");
		return;
	}

	AdbClient adb(s_adbNode);
	adb.adb_cmd("shell:am start -n com.shyz.desktop/.Launcher");

	if (maker->sync())
	{
		adb.adb_cmd("shell:am start -n com.shyz.desktop/.Launcher");
	}

	emit signal_resetDesktopFinished();
}

LauncherConfig* zLaucherManager::findConfig(QString s_adbNode)
{

	QMutexLocker locker(m_mutex);

	if(s_adbNode == "") return NULL;
	if (m_configMap.contains(strPhoneKey))
	{
		return m_configMap.value(strPhoneKey);
	}

	return NULL;	
}

LauncherConfig* zLaucherManager::createConfig(QString s_adbNode)
{

	QMutexLocker locker(m_mutex);

	if(s_adbNode == "") return NULL;

	LauncherConfig* config = new LauncherConfig(this);
    strUrl = "http://v3.18pc.18.net/PC/18/V4/DeskTopApkList.aspx";
    LoginManager::getInstance()->revokeUrlString(strUrl);

	if (!config->init(strUrl.toUtf8().data()))
	{
		DBG("Init config failed!");
		delete config;
		config = NULL;
		return NULL;
	}

	m_configMap.insert(strPhoneKey, config);

	return config;	
}

LauncherMaker* zLaucherManager::createMaker(LauncherConfig* config, QString s_adbNode)
{
	QMutexLocker locker(m_mutex);

	if (!config || (s_adbNode!=""))
	{
        DBG("Param error!");
		return NULL;
	}

	if (m_makerMap.contains(s_adbNode))
	{
		return m_makerMap.value(s_adbNode);
	}

    DBG("Add new maker, adb: %s\n", s_adbNode.toUtf8().data());
	LauncherMaker* maker = new LauncherMaker(config, s_adbNode.toUtf8().data());
	maker->init();
	m_makerMap.insert(s_adbNode, maker);
	return maker;
}
