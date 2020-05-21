#ifndef LOG_H
#define LOG_H

#include<stdio.h>
#include<iostream>
#include<string>
#include<stdarg.h>
#include<pthread.h>

#include"BlockQueue.h"

using namespace std;

class Log{
public:
	static Log *get_instance(){
		static Log instanec;
		return &instance;
	}

	static void *flush_log_thread(void *args)
	{
		Log::get_instance()->async_write_log();
	}

	bool init(const char *file_name, int log_buf_size = 8192, int split_lines = 5000000, int max_queue_size = 0);

	void write_log(int level, const char *format, ...);

	void flush(void);

private:
	Log();
	virtual ~Log();
	void async_write_log();

private:
	char dir_name[128];		//路径名
	char log_name[128];		//log文件名
	int m_split_lines;		//日志最大行数
	int m_log_buf_size;		//日志缓冲区大小
	long long m_count;		//日志行数记录
	int m_today;			//记录当前时间是哪一天
	FILE *m_fp;				//打开log的文件指针
	char *m_buf;
	BlockQueue<string> *m_log_queue;	//阻塞队列
	bool m_is_async;		//是否同步标志位
	Locker m_mutex;


};

#define LOG_DEBUG(format, ...)	Log::get_instance()->write_log(0, format, ##__VA_ARGS__)
#define LOG_INFO(format, ...)	Log::get_instance()->write_log(1, format, ##__VA_ARGS__)
#define LOG_WARN(format, ...)	Log::get_instance()->write_log(2, format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...)	Log::get_instacne()->write_log(3, format, ##__VA_ARGS__)

#endif