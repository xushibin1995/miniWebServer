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
const char* doc_root = "./";

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
//从内核时间表删除描述符
void removefd(int epollfd, int fd){
	epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
	close(fd);
}
//将事件重置为EPOLLONESHOT
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
	m_real_file = "./index.html";
}

//从状态机
Http::LINE_STATUS Http::parse_line(){
	char temp;
	for(; m_checked_idx < m_read_idx; ++m_checked_idx){
		temp = m_read_buf[m_checked_idx];
		if(temp == '\r'){
			if((m_checked_idx +1) == m_read_idx){
				return LINE_OPEN;
			}
			else if(m_read_buf[m_checked_idx +1] == '\n'){
				m_read_buf[m_checked_idx++] = '\0';
				m_read_buf[m_checked_idx++] = '\0';
				return LINE_OK;
			}
			return LINE_BAD;
		}
		else if(temp == '\n'){
			if((m_checked_idx > 1) && (m_read_buf[m_checked_idx-1] == '\r')){
				m_read_buf[m_checked_idx++] = '\0';
				m_read_buf[m_checked_idx++] = '\0';
				return LINE_OK;
			}
			return LINE_BAD;
		}
	}
	return LINE_OPEN;
}

//循环读取客户数据，直到无数据可读或者对方关闭连接
bool Http::read_once(){
	if(m_read_idx >= READ_BUFFER_SIZE){
		return false;
	}
	int bytes_read = 0;
	while(true){
		//从m_sockfd中读取数据，读取到m_read_buf偏移m_read_idx个单位的地址，读取数据的大小是READ_BUFFER_SIZE - m_read_idx,最后一个参数是flag，一般置为0
		//因为数据是由对端发送过来的，不能确保每次对端发送的数据都够BUFFER_SIZE个字节，所以只要有数据就会返回读取的数据字节数，所以要设置成m_read_buf + m_read_idx
		bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
		//无数据，返回-1，并且设置errno
		if(bytes_read == -1){
			if(errno == EAGAIN || errno == EWOULDBLOCK){
				break;		//非阻塞模式下，read函数会返回一个错误EAGAIN，提示你应用程序现在没有数据可读请稍后再试。
			}
			else{
				return false;
			}
		}
		else if(bytes_read == 0){
			return false;  //0 表示通信对端已经关闭连接了
		}
		m_read_idx += bytes_read;


	}
	return true;
}

//解析HTTP请求行，获得请求方法、目标url，以及http版本号
Http::HTTP_CODE Http::parse_request_line(char*text){
	m_url = strpbrk(text, " \t");
	if(!m_url){
		return BAD_REQUEST;
	}
	*m_url++ = '\0';

	char* method = text;
	if(strcasecmp(method, "GET") == 0){
		m_method = GET;
	}
	else{
		return BAD_REQUEST;
	}

	m_url += strspn(m_url, " \t");
	m_version = strpbrk(m_url, " \t");
	if(!m_version){
		return BAD_REQUEST;
	}
	*m_version++ = '\0';
	m_version += strspn(m_version, " \t");
	if(!m_version){
		return BAD_REQUEST;
	}
	*m_version++ = '\0'; 
	m_version += strspn(m_version, " \t");
	if(strcasecmp(m_version, "HTTP/1.1") != 0){
		return BAD_REQUEST;
	}
	if(strncasecmp(m_url, "http://", 7) == 0){
		m_url += 7;
		m_url = strchr(m_url, '/');

	}

	if(!m_url || m_url[0] != '/'){
		return BAD_REQUEST;
	}

	m_check_state = CHECK_STATE_HEADER;
	return NO_REQUEST;
}

Http::HTTP_CODE Http::parse_headers(char * text){
	//遇到空行表示头部字段解析完毕
	if(text[0] == '\0' ){
		//如果http请求有消息体，则还需要读取m_content_length字节的消息体，状态机转移到CHECK_STATE_CONTENT状态
		if(m_content_length != 0){
			m_check_state = CHECK_STATE_CONTENT;
			return NO_REQUEST;
		}
		//否则得到一个完整的http请求
		return GET_REQUEST;
	}
	//处理connection字段
	else if(strncasecmp(text, "Connection:", 11) == 0){
		text += 11;
		text += strspn(text, " \t");
		if(strcasecmp(text, "keep-alive") == 0){
			m_linger = true;
		}

	}
	//处理content-length字段
	else if(strncasecmp(text, "Content-length", 15) == 0){
		text += 15;
		text += strspn(text, " \t");
		m_content_length = atol(text );
	}
	//处理host字段
	else if (strncasecmp(text, "Host:", 5) == 0)
    {
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }
    else
    {

        LOG_INFO("oop!unknow header: %s", text);
        Log::get_instance()->flush();
    }
    return NO_REQUEST;
}

//判断http请求是否被完整读入
Http::HTTP_CODE Http::parse_content(char *text)
{
    if (m_read_idx >= (m_content_length + m_checked_idx))
    {
        text[m_content_length] = '\0';
      
    /****************************************************/
        m_string = text;
    /****************************************************/
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

Http::HTTP_CODE Http::process_read()
{
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char *text = 0;

    while ((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = parse_line()) == LINE_OK))
    {
        text = get_line();
        m_start_line = m_checked_idx;
        LOG_INFO("%s", text);
        Log::get_instance()->flush();
        switch (m_check_state)
        {
        case CHECK_STATE_REQUESTLINE:
        {
            ret = parse_request_line(text);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            break;
        }
        case CHECK_STATE_HEADER:
        {
            ret = parse_headers(text);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            else if (ret == GET_REQUEST)
            {
                return do_request();
            }
            break;
        }
        case CHECK_STATE_CONTENT:
        {
            ret = parse_content(text);
            if (ret == GET_REQUEST)
                return do_request();
            line_status = LINE_OPEN;
            break;
        }
        default:
            return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}

Http::HTTP_CODE Http::do_request(){
    //strcpy( m_real_file, doc_root );
    //int len = strlen( doc_root );
    //strncpy( m_real_file + len, m_url, FILENAME_LEN - len - 1 );
 
    if ( stat( m_real_file.c_str(), &m_file_stat ) < 0){
        return NO_RESOURCE;
    }

    if ( ! ( m_file_stat.st_mode & S_IROTH )){
        return FORBIDDEN_REQUEST;
    }

    if ( S_ISDIR( m_file_stat.st_mode)){
        return BAD_REQUEST;
    }

    int fd = open( m_real_file.c_str(), O_RDONLY );
    m_file_address = ( char* )mmap( 0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0 );
    close( fd );
    return FILE_REQUEST;
}

void Http::unmap()
{
    if( m_file_address )
    {
        munmap( m_file_address, m_file_stat.st_size );
        m_file_address = 0;
    }
}

bool Http::write()
{
    int temp = 0;
    int bytes_have_send = 0;
    int bytes_to_send = m_write_idx;
    if ( bytes_to_send == 0 )
    {
        modfd( m_epollfd, m_sockfd, EPOLLIN );
        init();
        return true;
    }

    while( 1 )
    {
        temp = writev( m_sockfd, m_iv, m_iv_count );
        if ( temp <= -1 )
        {
            if( errno == EAGAIN )
            {
                modfd( m_epollfd, m_sockfd, EPOLLOUT );
                return true;
            }
            unmap();
            return false;
        }

        bytes_to_send -= temp;
        bytes_have_send += temp;
        if ( bytes_to_send <= bytes_have_send )
        {
            unmap();
            if( m_linger )
            {
                init();
                modfd( m_epollfd, m_sockfd, EPOLLIN );
                return true;
            }
            else
            {
                modfd( m_epollfd, m_sockfd, EPOLLIN );
                return false;
            } 
        }
    }
}

bool Http::add_response( const char* format, ... ){
    if( m_write_idx >= WRITE_BUFFER_SIZE ){
        return false;
    }
    va_list arg_list;
    va_start( arg_list, format );
    int len = vsnprintf( m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list );
    if( len >= ( WRITE_BUFFER_SIZE - 1 - m_write_idx ) )
    {
        return false;
    }
    m_write_idx += len;
    va_end( arg_list );
    return true;
}

bool Http::add_status_line( int status, const char* title )
{
    return add_response( "%s %d %s\r\n", "HTTP/1.1", status, title );
}

bool Http::add_headers( int content_len )
{
    add_content_length( content_len );
    add_linger();
    add_blank_line();
}

bool Http::add_content_length( int content_len )
{
    return add_response( "Content-Length: %d\r\n", content_len );
}

bool Http::add_linger()
{
    return add_response( "Connection: %s\r\n", ( m_linger == true ) ? "keep-alive" : "close" );
}

bool Http::add_blank_line()
{
    return add_response( "%s", "\r\n" );
}

bool Http::add_content( const char* content )
{
    return add_response( "%s", content );
}

bool Http::process_write( HTTP_CODE ret )
{
    switch ( ret )
    {
        case INTERNAL_ERROR:
        {
            add_status_line( 500, error_500_title );
            add_headers( strlen( error_500_form ) );
            if ( ! add_content( error_500_form ) )
            {
                return false;
            }
            break;
        }
        case BAD_REQUEST:
        {
            add_status_line( 400, error_400_title );
            add_headers( strlen( error_400_form ) );
            if ( ! add_content( error_400_form ) )
            {
                return false;
            }
            break;
        }
        case NO_RESOURCE:
        {
            add_status_line( 404, error_404_title );
            add_headers( strlen( error_404_form ) );
            if ( ! add_content( error_404_form ) )
            {
                return false;
            }
            break;
        }
        case FORBIDDEN_REQUEST:
        {
            add_status_line( 403, error_403_title );
            add_headers( strlen( error_403_form ) );
            if ( ! add_content( error_403_form ) )
            {
                return false;
            }
            break;
        }
        case FILE_REQUEST:
        {
            add_status_line( 200, ok_200_title );
            if ( m_file_stat.st_size != 0 )
            {
                add_headers( m_file_stat.st_size );
                m_iv[ 0 ].iov_base = m_write_buf;
                m_iv[ 0 ].iov_len = m_write_idx;
                m_iv[ 1 ].iov_base = m_file_address;
                m_iv[ 1 ].iov_len = m_file_stat.st_size;
                m_iv_count = 2;
                return true;
            }
            else
            {
                const char* ok_string = "<html><body></body></html>";
                add_headers( strlen( ok_string ) );
                if ( ! add_content( ok_string ) )
                {
                    return false;
                }
            }
        }
        default:
        {
            return false;
        }
    }

    m_iv[ 0 ].iov_base = m_write_buf;
    m_iv[ 0 ].iov_len = m_write_idx;
    m_iv_count = 1;
    return true;
}

void Http::process()
{
    HTTP_CODE read_ret = process_read();
    if ( read_ret == NO_REQUEST )
    {
        modfd( m_epollfd, m_sockfd, EPOLLIN );
        return;
    }

    bool write_ret = process_write( read_ret );
    if ( ! write_ret )
    {
        close_conn();
    }

    modfd( m_epollfd, m_sockfd, EPOLLOUT );
}













