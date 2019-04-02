#include "thunder_core.h"
#include <windows.h>

//////////////////////////////////////////////////////////////////////////
// 初始化库（全局）
//////////////////////////////////////////////////////////////////////////
class down_core_object
{
public:
	down_core_object();
	~down_core_object();
	int sub_count();
	static down_core_object* get_core_object();
	int add_count();
protected:
private:
	unsigned long m_count;
	CRITICAL_SECTION m_lock;
};

down_core_object g_core_object;

down_core_object::down_core_object()
{
	m_count = 0;
	InitializeCriticalSection(&m_lock);
}

down_core_object::~down_core_object()
{
	DeleteCriticalSection(&m_lock);
}

int down_core_object::add_count()
{
	EnterCriticalSection(&m_lock);
	m_count++;
	int count = m_count;
	
	if (count == 1)
	{
		init(0); // 初始化
		
		set_product_identifier(0, 
			"1.5.3.288", 
			strlen("1.5.3.288"), 
			"80000012", 
			strlen("80000012")); // 设置版本号（PS: 其实没有用的）
		set_upload_speed_limit(-1, -1);
		cancel_speed_limit();
		set_speed_limit(-1);
		m_count++;
	}

	LeaveCriticalSection(&m_lock);

	return count;
}

int down_core_object::sub_count()
{
	EnterCriticalSection(&m_lock);
	m_count--;
	int count = m_count;

	if (count == 0)
	{
		uninit();
	}
	
	LeaveCriticalSection(&m_lock);

	return count;
}

down_core_object* down_core_object::get_core_object()
{
	return &g_core_object;
}



//////////////////////////////////////////////////////////////////////////
//  下载接口
//////////////////////////////////////////////////////////////////////////

down_file_object::down_file_object()
{
	down_core_object* core_h = down_core_object::get_core_object();
	core_h->add_count();

	m_down_url = NULL;
	m_save_path = NULL;
	m_save_name = NULL;
	m_task_handle = NULL;
	m_is_pause = FALSE;

	m_file_size = 0;
}


down_file_object::~down_file_object()
{
	down_core_object* core_h = down_core_object::get_core_object();
	core_h->sub_count();
}

int down_file_object::add_task(char* szurl, char* szpath, char* szname)
{
	int iurl, ipath;
	int iszname = 0;

	if (szurl == NULL || szpath == NULL)
	{
		return -1;
	}
	if (m_down_url != NULL)
	{
		return -2;
	}

	iurl = strlen(szurl);
	ipath = strlen(szpath);
	if (szname != NULL)
	{
		iszname = strlen(szname);
		m_save_name = new char[iszname+1];
		strcpy(m_save_name, szname);
	}

	m_down_url = new char[iurl+1];
	strcpy(m_down_url, szurl);
	m_save_path = new char[ipath+1];
	strcpy(m_save_path, szpath);

	int iret = create_new_task( szurl, iurl, 
							NULL, 0, 
							NULL, 0, 
							szpath, ipath, 
							szname, iszname, 
							&m_task_handle
							);

	get_down_file_name(); // 获取一次保存的文件名
	return iret;
}

int down_file_object::continue_task(char* szurl, char* szpath, char* szname)
{
	int iurl, ipath, iszname;
	if (m_task_handle != NULL)
	{
		// 重新启动程序后，加原先任务
		iurl = strlen(m_down_url);
		ipath = strlen(m_save_path);
		iszname = strlen(m_save_name);

		return create_continued_task( m_down_url, iurl, 
								NULL, 0, 
								NULL, 0, 
								m_save_path, ipath, 
								m_save_name, iszname, 
								&m_task_handle);
	}
	else
	{
		if (szurl == NULL || szpath == NULL || szname == NULL)
		{
			return -1;
		}

		iurl = strlen(szurl);
		m_down_url = new char[iurl+1];
		if (m_down_url == NULL)
		{
			return -1;
		}
		strcpy(m_down_url, szurl);

		ipath = strlen(szpath);
		m_save_path = new char[ipath+1];
		strcpy(m_save_path, szpath);

		iszname = strlen(szname);
		m_save_name = new char[iszname+1];
		strcpy(m_save_name, szname);

		return create_continued_task( szurl, iurl, 
								NULL, 0, 
								NULL, 0, 
								szpath, ipath, 
								szname, iszname, 
								&m_task_handle);
	}
}

int down_file_object::pause_task()
{
	if (m_task_handle == NULL)
	{
		return -1;
	}
	int ival = stop_task(m_task_handle);
	if (ival != 0)
	{
		return ival;
	}
	m_is_pause = TRUE;
 	return delete_task(m_task_handle);;
}

int down_file_object::start()
{
	if (m_is_pause == TRUE)
	{
		ptask_info ptinf = new task_info;
		ZeroMemory(ptinf, sizeof(task_info));
		if( query_task_info(m_task_handle, ptinf) == 0xfffffff4 ) // 没有任务的情况下才进行添加
		{
			continue_task();
		}
		delete ptinf;

		m_is_pause = FALSE; 
	}

	for (int i=0; i<10; i++)
	{
		set_proxy_info(m_task_handle, i, 0);
		Sleep(50);
	}
	set_task_type(m_task_handle, 1);
	return start_task(m_task_handle);
}

int down_file_object::del_task(bool del_sucess_file)
{
	if (m_task_handle == 0)
	{
		return -1;
	}
	stop_task(m_task_handle);
	delete_task(m_task_handle);

	// 需要删除文件呀
	char* file = new char[MAX_PATH*2];
	ZeroMemory(file, MAX_PATH*2);
	strcpy(file, get_save_path());
	strcat(file, get_down_file_name());
	if (del_sucess_file)
	{
		DeleteFileA(file); // 删除下载完成的文件DeleteFileA(file);
	}

	// 删除未下载完成的零时文件
	strcat(file, ".td");
	DeleteFileA(file); 

	strcat(file, ".cfg");
	DeleteFileA(file);
	return 0;
}

__int64 down_file_object::get_current_size()
{
	if ( m_task_handle == NULL )
	{
		return -1;
	}

	__int64 cur_size = -1;
	ptask_info ptinf = new task_info;
	ZeroMemory(ptinf, sizeof(task_info));
	if( query_task_info(m_task_handle, ptinf) == 0 )
	{
		cur_size = ptinf->down_file_size;
	}
	delete ptinf;

	return cur_size;
}

__int64 down_file_object::get_file_size()
{
	if (m_file_size != 0)
	{
		return m_file_size;
	}
	if ( m_task_handle == NULL )
	{
		return 0;
	}
	ptask_info ptinf = new task_info;
	ZeroMemory(ptinf, sizeof(task_info));
	query_task_info(m_task_handle, ptinf);
	m_file_size = ptinf->total_file_size;
	delete ptinf;

	return m_file_size;
}


const char* down_file_object::get_down_file_name()
{
	if ( m_save_name != 0)
	{
		delete []m_save_name;
//		return m_save_name;
	}
	if ( m_task_handle == NULL )
	{
		return NULL;
	}

	ptask_info ptinf = new task_info;
	ZeroMemory(ptinf, sizeof(task_info));
	query_task_info(m_task_handle, ptinf);
	m_save_name = new char[ptinf->file_name_len+1];
	ZeroMemory(m_save_name, ptinf->file_name_len+1);
	memcpy(m_save_name, ptinf->file_name, ptinf->file_name_len);
	delete ptinf;
	
	return m_save_name;
}


double down_file_object::query_down_pos() // 查询下载进度
{
	double fsize = get_current_size();
	if (fsize == 0)
	{
		return 0;
	}
	return fsize/(double)get_file_size()*100;
}

const char* down_file_object::get_down_url()  // 查询下载url
{
	return m_down_url;
}

const char* down_file_object::get_save_path()  // 得到保存路径
{
	return m_save_path;
}

int down_file_object::get_down_status()
{
	if ( m_task_handle == NULL )
	{
		return -1;
	}
	
	ptask_info ptinf = new task_info;
	ZeroMemory(ptinf, sizeof(task_info));
	query_task_info(m_task_handle, ptinf);
	int status = ptinf->task_state;
	delete ptinf;
	
	return status;
}
