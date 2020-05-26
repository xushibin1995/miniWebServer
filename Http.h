#ifndef HTTP_H
#define HTTP_H
#include<unistd.h>
#include<signal.h>
#include<sys/types.h>
#include<sys/epoll.h>
#include<fcntl.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<assert.h>
#include<sys/stat.h>
#include<string.h>
#include<pthread.h>
#include<stdio.h>
#include<stdlib.h>
#include<sys/mman.h>
#include<stdarg.h>
#include<errno.h>
#include<sys/wait.h>
#include<sys/uio.h>
#include"Locker.h"

class Http{
public:
	static const int FILENAME_LEN = 200;	//文件名的最大长度
	static const int READ_BUFFER_SIZE = 2048;	//都缓冲区的大小
	static const int WRITE_BUFFER_SIZE = 1024;	//写缓冲区的大小
	enum METHOD {GET = 0, POST, HEAD, PUT, DELETE, TRACE, OPTION, CONNECT, PATCH };	//http请求方法，这里只支持get，别忘记最后的分号
	enum CHECK_STATE {CHECK_STATE_REQUESTLINE = 0,
					  CHECK_STATE_HEADER,
					CHECK_STATE_CONTENT };		//解析客户请求时，主状态机的状态
	enum HTTP_CODE {NO_REQUEST, GET_REQUEST, BAD_REQUEST,
					NO_RESOURCE, FORBNIDDEN_RESOURSE, FILE_REQUEST,
					INTERNAL_ERROR, CLOSED_CONNECTION};		//http请求的处理结果
	enum LINE_STATUS{ LINE_OK = 0, LINE_BAD, LINE_OPEN };

public:
	//初始化新接收的连接
	void init(int sockfd, const sockaddr_in& addr);
	//关闭连接
	void close_conn(bool real_close = true);
	//处理客户请求
	void process();
	//非阻塞读操作
	bool read();
	//非阻塞写操作
	bool write();

private:
	//初始化连接
	void init();
	//解析http请求
	HTTP_CODE process_read();
	//填充http应答
	bool process_write(HTTP_CODE RET);

	//被http_read()调用，用来解析http请求
	HTTP_CODE parse_request_line(char* text);
	HTTP_CODE parse_headers(char * text);
	HTTP_CODE parse_content(char* text);
	HTTP_CODE do_request();
	char* get_line(){return m_read_buf + m_start_line; }
	LINE_STATUS parse_line();	//从状态机

	//被process_write()调用用来填充http应答
	void unmap();
	bool add_reponse(const char*format, ...);
	bool add_content(const char* content);
	bool add_status_line(int status, const char* title);
	bool add_headers(int content_length);
	bool add_content_length(int content_length);
	bool add_linger();
	bool add_blank_line();

public:
	//同一事件模型，所有socket上的事件都被注册到同一个 epo内核事件表中，所以将epoll文件描述符设置为静态的
	static int m_epollfd;
	//统计用户数量
	static int m_user_count;

private:
	int m_sockfd;			//http的连接socket
	sockaddr_in m_address;	//对方的socket地址

	char m_read_buf[READ_BUFFER_SIZE];	//读缓冲区
	int m_read_idx;		//标志读缓冲中已经读入的客户端的最后一个字节的下一个字节的位置
	int m_checked_idx;	//当前正在分析的字符在读缓冲区中的位置；
	int m_start_line;	//当前正在解析的行的起始位置
	char m_write_buf[ WRITE_BUFFER_SIZE];	//写缓冲区
	int m_write_idx;	//写缓冲区中待发送的字符数

	CHECK_STATE m_check_state;		//主状态机当前的状态
	METHOD m_method;				//请求方法

	char m_real_file[FILENAME_LEN];	//客户请求的目标文件的完整路径，其内容扽与doc_root + m_url, doc_root时网站的根目录
	char* m_url;					//客户请求的目标文件的文件名
	char* m_version;					//http协议版本号，这里仅支持HTTP/1.1
	char* m_host;					//主机名
	int m_content_length;			//http请求消息体的长度
	bool m_linger;					//http请求是否保持连接

	char* m_file_adress;			//客户请求的目标文件被mmap到内存中的位置
	struct stat m_file_stat;		//目标文件的状态，通过他我们可以判断文件是否存在、是否为目录文件、是否可读，并获取文件大小等信息
	struct iovec m_iv[2];			//使用writev来执行写操作
	int m_iv_count;					//m_iv_count表示被写内存块的数量

};

#endif

