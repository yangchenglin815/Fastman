#ifndef _BAIDU_CORE_H_
#define _BAIDU_CORE_H_

#include <windows.h>
#include <QProcess>
#include <QString>


class yun_file_object
{
public:
    yun_file_object();
    ~yun_file_object();

	// 下载接口
    int add_task(QString url, QString yunurl, QString path, QString saveFile);
	int pause_task();
    int del_task(bool del_sucess_file=false); // 参数是否下载已完成的文件
	int start();

	// 查询下载情况
	__int64 get_current_size();  // 目前下载的数据大小
	__int64 get_file_size();  // 文件总大小
	int get_down_status();
    int get_broken_status();

	// 查询一些信息
	double query_down_pos(); // 查询下载进度
    QString get_down_file_name();  // 下载保存名字
    QString get_down_url();  // 查询下载url
    QString get_save_path();  // 得到保存路径

private:
    void runCmd(QString app, QString cmdLine);
    void parseLine(const QString &line);

    QString m_yun_url;
    QString m_down_url;
    QString m_save_path;
    QString m_save_name;
    QString _fn;
	__int64 m_file_size;
    __int64 m_read_size;
	bool m_is_pause;
    float _percent;
    HANDLE _hRead;
    QString _buf;
    int _status;
    int _broken_download;
    PROCESS_INFORMATION _pi;
};


#endif // _BAIDU_CORE_H_

