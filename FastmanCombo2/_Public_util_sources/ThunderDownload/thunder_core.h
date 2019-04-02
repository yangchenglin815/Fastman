#ifndef __THUNDER_CORE_DENGXIAN_2014_12_30_HH__
#define __THUNDER_CORE_DENGXIAN_2014_12_30_HH__
#include "download_engine.h"
// 
// #define NULL ((void*)0)

class down_file_object
{
public:
	down_file_object();
	~down_file_object();

	// 下载接口
	int add_task(char* szurl, char* szpath = 0, char* szname = 0);
	int continue_task(char* szurl = 0, char* szpath = 0, char* szname = 0);
	int pause_task();
	int del_task(bool del_sucess_file); // 参数是否下载已完成的文件
	int start();

	// 查询下载情况
	__int64 get_current_size();  // 目前下载的数据大小
	__int64 get_file_size();  // 文件总大小
	int get_down_status();

	// 查询一些信息
	double query_down_pos(); // 查询下载进度
	const char* get_down_file_name();  // 下载保存名字
	const char* get_down_url();  // 查询下载url
	const char* get_save_path();  // 得到保存路径

private:
	char* m_down_url;
	char* m_save_path;
	char* m_save_name;
	__int64 m_file_size;
	TASKHANDLE m_task_handle;
	bool m_is_pause;
};





























#endif // __THUNDER_CORE_DENGXIAN_2014_12_30_HH__

