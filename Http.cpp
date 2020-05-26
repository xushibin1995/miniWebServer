#include"Http.h"


//定义http响应的一些状态信息
const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char* error_403_title = "Forbidden";
const char* error_403_form = "You do not have permission to get file from this server\n";
const char* error_404_title = "Not Found";
const char* error_404_form = "The requested file was not found on this server.\n";
const char* error_500_title = "Internet Error";
const char* error_500_form = "There was an unusual problem serving the requested file.\n";

//网站的根目录
const char* doc_root = "./index.html";

//设置文件描述符对应的文件为非阻塞
int setnonblocking(int fd){
	int old_option = fcntl(fd, F_GETFL);
	int new_option = old_option | O_NONBLOCK;
	fcntl(fd, F_SETFL, new_option);
	return old_option;
}

//往监听集合中添加所要监听的文件描述符
void addfd(int epollfd, int fd, bool one_shot){
	epoll_event event;
	event.data.fd = fd;
	event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;		//设置epoll监听读事件，并且采用边沿触发的方式，
	if(one_shot){
		event.events|= EPOLLONESHOT;					//为了保证一个socket仅又一个线程处理

	}
	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
	setnonblocking(fd);

}

void removefd(int epollfd, int fd){
	epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
	close(fd);
}

void modfd(int epollfd, int fd, int ev){
	epoll_event event;
	event.data.fd = fd;
	event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
	epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

int Http::m_user_count = 0;
int Http::m_epollfd = -1;

void Http::close_conn(bool real_close){
	if(real_close && (m_sockfd != -1) ){
		removefd(m_epollfd, m_sockfd);
		m_sockfd =-1;
		m_user_count--;		//关闭一个连接，将客户总量减1
	}
}

void Http::init(int sockfd, const sockaddr_in& addr){
	m_sockfd = sockfd;
	m_address = addr;
	int reuse = 1;
	setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse,sizeof(reuse)); //服务器主动关闭连接时，避免TIME_WAIT
	addfd(m_epollfd, sockfd, true);
	m_user_count++;			//新增加一个连接，客户总量加1
	init();					//真正的init方法
}

void Http::init(){
	m_check_state = CHECK_STATE_REQUESTLINE;
	m_linger = false;		//http是否保持连接

	m_method =	GET;		
	m_url = NULL;			//目标文件的文件名
	m_version = NULL;		//http协议版本号
	m_content_length = 0;	//http消息体长度
	m_host = NULL;			//主机名
	m_start_line = 0;		//当前正在解析的行的起始位置
	m_checked_idx = 0;		//当前正在分析的字符在缓冲区的位置
	m_read_idx = 0;			//标志读缓冲中已经读入的客户端的最后一个字节的下一个字节的位置
	m_write_idx = 0;		//写缓冲区中待发送的字符个数
	memset(m_read_buf, '\0', READ_BUFFER_SIZE);
	memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
	memset(m_real_file, '\0', FILENAME_LEN);
}








