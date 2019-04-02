#ifndef LUNCHERMAKER_H
#define LUNCHERMAKER_H
#include <QString>
#include <QQueue>
#include <QMap>
#include <QStringList>

/**
 * @brief LuncherMaker
 * 当手机插入后
 * LuncherMaker maker = new LuncherMaker(config,serail);
 * 完成应用后，执行以下语句，通知对象配置节点
 * maker.install(app,path);
 * 完成安装后，执行以下语句，会将配置XML写入到手机的/data/local/tmp/zx_default_workspace.xml
 * maker.sync();
 *
 * @see LauncherConfig
 */
struct HiddenNode{
       QString pkgname;
       QString classname;
};

class LauncherConfig;
class LauncherEditor;
class LauncherMaker
{
    LauncherConfig* _config;
    QString _serial;
    //LauncherEditor* _editor;
    QMap<QString,QString> _apps;
	QMap<QString,QString> _outapps;
	QMap<QString,QString> _pushapps;
	//QMap<QString,QString> _trashcanapps;



public:
	LauncherEditor* _editor;
	QString m_configName;
    QString m_ini;
    QString m_hiddenConf;
    QString m_appmkt;

    QQueue<HiddenNode> _hiddenOrder;
    /**
     * @brief LauncherMaker
     * @param config
     * @param serial
     */
    LauncherMaker(LauncherConfig* config,const char* serial = NULL);
	void InsertAPK(QString package,QString ClassName);

    /**
     * @brief init 初始化工具
     * @param url 配置URL
     * @return 成功配置返回true，否则false
     */
    inline void setConfig(LauncherConfig* config){_config = config;}
    inline void setSerial(const char* serial){_serial = serial;}
    /**
     * @brief init folder and widget
     */
    void init();
    /**
     * @brief sync 将XML配置文件
     * @return
     */
    bool sync();
    /**
     * @brief install 说明指定手机，已安装应用
     * @param package 手机安装的应用的包名
     * @param path 手机安装的APK的本地存储路径
     * @return 接受包名，返回true,否则false
     */
    bool install(const char* package,const char* path);
	/**
     * @brief push 说明指定手机，已推送应用
     * @param package 手机安装的应用的包名
     * @param rpath 推送远程路径
     * @return 接受包名，返回true,否则false
     */
	bool push(const char* package,const char* rpath);
	/************************************************************************/
	/* 设置自动分类，默认启用                                               */
	/************************************************************************/
	void folder(bool auto_ = true);
};

#endif // LUNCHERMAKER_H
