#ifndef __DOWNLOAD_ENFINE_DENGXIAN_2014_12_30_HH__
#define __DOWNLOAD_ENFINE_DENGXIAN_2014_12_30_HH__

#pragma comment(lib, "download_engine")

//////////////////////////////////////////////////////////////////////////
//  数据结构定义
//////////////////////////////////////////////////////////////////////////
typedef void* TASKHANDLE;


#pragma pack(push, 1)
typedef struct _task_info
{
	int task_state; // 0 表示正在下载，可以下载, == 2 ???, ==C  要调用 get_failure_detail, == 
	unsigned long unknow_val[6];
	__int64 total_file_size;   // 文件总大小
	__int64 flush_byte_size;   // 目前为止网络上总的数据流量
	__int64 down_file_size;    // 下载下来的文件大小
	unsigned long unknow_val2;
	unsigned long file_name_len;  // 下载保存的文件名称
	char file_name[260];
	unsigned char unknow_data[1024];
}task_info, *ptask_info;
#pragma pack(pop)




//////////////////////////////////////////////////////////////////////////
// 函数定义
//////////////////////////////////////////////////////////////////////////
#ifdef __cplusplus
extern "C" {
#endif

// 参数其实没意义，此版本中的 set_product_identifier 直接返回0
int __stdcall set_product_identifier(unsigned long unknow, char* marver, size_t mar_len, char* majver, size_t maj_len);
int __stdcall init(unsigned long mode); // 直接传0，返回0
int __stdcall set_upload_speed_limit(int val1, int val2); // 设置上传限速, -1 不限制
int __stdcall cancel_speed_limit();  // 取消限速  -1
int __stdcall set_speed_limit(int val1);

// 新加下载任务
int __stdcall create_new_task(char* url, size_t url_len, 
					  char* buf1, size_t buf1_len, 
					  char* buf2, size_t buf2_len,
					  char* save_addr, size_t addr_len,
					  char* save_name, size_t name_len, // 保存名字传空，表示默认名字
					  TASKHANDLE* task);


// 继续以前的下载任务
int __stdcall create_continued_task(char* url, size_t url_len, 
	char* buf1, size_t buf1_len, 
	char* buf2, size_t buf2_len,
	char* save_addr, size_t addr_len,
	char* save_name, size_t name_len, // 保存名字传空，表示默认名字
	TASKHANDLE* task);

int __stdcall get_failure_detail(TASKHANDLE task, char* pbuf); 
int __stdcall set_task_type(TASKHANDLE task, unsigned long mode); 
int __stdcall start_task(TASKHANDLE task); 
int __stdcall stop_task(TASKHANDLE task);     // 关闭/暂停任务
int __stdcall delete_task(TASKHANDLE task);   // 删除任务from任务列表

int __stdcall set_proxy_info(TASKHANDLE task, int, int);
int __stdcall query_task_info(TASKHANDLE task, ptask_info ptaskinfo);   // 查询任务信息，保存文件名，加载大小等
int __stdcall get_task_gcid(TASKHANDLE,unsigned char*, int); 
int __stdcall query_part_cid(TASKHANDLE, ptask_info); 
 
int __stdcall uninit();      // 关闭前取消初始化


#ifdef __cplusplus
	};
#endif



#endif  // __DOWNLOAD_ENFINE_DENGXIAN_2014_12_30_HH__