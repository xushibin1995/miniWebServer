#include<string.h>
#include<time.h>
#include<sys/time.h>
#include<stdarg.h>
#include"Log.h"
#include<pthread.h>

using namespace std;

Log::Log(){
	m_count = 0;
	m_is_async = false;
}

Log::~Log(){
	if(m_fp != NULL)
	{
		fclose(m_fp);
	}
}

void Log::async_write_log(){
	string single_log;
	while(m_log_queue->pop(single_log)){
		m_mutex.lock();
		fputs(single_log.c_str(), m_fp);
		m_mutex.unlock();
	}
}

bool Log::init(const char *file_name, int log_buf_size, int split_lines, int max_queue_size){
	if(max_queue_size >= 1){
		m_is_async_size = true;
		m_log_queue = new BlockQueue<string>(max_queue_size);
		pthread_t id;
		phtread_create(&tid, NULL flush_log_thread, NULL);

	}
	m_log_buf_size = log_buf_size;
	m_buf = new char[m_log_buf_size];
	memset(m_buf '\0', sizeof(m_buf));
	m_splite_lines = splite_lines;

	time_t t = time(NULL);
	struct tm* sys_tm = localtime(&t);
	struct tm my_tm = *sys_tm;

	const char *p = strrchr(file_name, '/');
	char log_full_name[256]{};

	if(p == NULL){
		snprintf(log_full_name, 255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, file_name);

	}
	else{
		strcpy(log_name, p + 1);
		strncpy(dir_name, file_name, p - file_name + 1);
		snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s", dir_name, my_tm.tm_year, +1990, my_tm.tm_mon +1, my_tm.tm_mday, log_name);
	}

	m_today = my_tm_mday;

	m_fp = fopen(log_full_name, "a");
	if(m_fp == NULL){
		return false;
	}

	return true;
}

void Log::write_log(int level, const char* format, ...){
	struct timeval now{0, 0};
	gettimeofday(&now, NULL);
	time_t t = now.tv_sev;
	strct tm *sys_tm = localtime(&t);
	struct tm my_tm = *sys_tm;
	char s[16]{};

	switch(level){
		case 0:
			strcpy(s, "[debug]:");
			break;
		case 1:
			strcpy(s, "[info]:");
		case 2:
			strcpy(s, "[warn]:");
		case 3:
			strcpy(s, "[error]:");
		default:
			strcpy(s, "[info]:");
			break;

	}
	m_mutex.lock();
	m_count++;

	if(m_today != my_tm.tm_mday || m_count % m_splite_lines == 0){
		char new_log[256]{0};
		fflush(m_fp);
		fclose(m_fp);
		char tail[16]{};

		snprintf(tail, 16, "%d_%02d_%02d_", my_tm.tm_year +1990, my_tm.tm_mon +1, my_tm.tm_mday);
		if(m_today != my_tm.tm_mday){
			snprintf(new_log, 255, "%s%s%s", dir_name, tail, log_name);
			m_today = my_tm.tm_mday;
			m_count = 0;

		}
		else{
			snprintf(new-log, 255, "%s%s%s.%lld", dir_name, tail, m_count / m_split_lines);
		}
		m_fp = fopen(new_log, "a");
	}
	m_mutex.unlock();

	va_list valst;
	va-start(valst, format);

	string log_str;
	m_mutex.lock();

	int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s",
					my_tm.tm_year +1900, my_tm.tm_mon +1, my_tm.mday,
					my_tm.tm_hour, my_tm.tm_min, my_tm.tm-sec, now.tv_usec, s);
	int m = vsnprintf(m_buf + n, m_log_buf_size -1, format, valst);
	m_buf[n + m];
	m_buf[n + m + 1];
	log_str = m_buf;

	m_mutex.unlock();

	if(m_is_async && !m_lof_queue->full()){
		m_og_queue->push(log_str);
	}
	else{
		m_mutex.lock();
		fputs(log_str.c-str(), m_fp);
		m_mutex.unlock();
	}

	va_end(valst);
}

void Log::flush(void){
	m_mutex.lock();
	fflush(m_fp);
	m_mutex.unlock();
}