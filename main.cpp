#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<stdio.h>
#include<unistd.h>
#include<errno.h>
#include<stdlib.h>
#include<fcntl.h>
#include<cassert>
#include<sys/epoll.h>

#include"Timer.h"
#include"Log.h"
#include"Locker.h"
#include"ThreadPool.h"
#include"Http.h"

#define MAX_FD 65536
#define MAX_EVENT_NUMBER 10000
#define TIMESLOT 5

extern int addfd(int epollfd, int fd, bool one_shot);
extern int remove(int epollfd, int fd);
extern int setnonblocking(int fd);

//设置定时器相关参数
static int pipefd[2];
static Time_wheel timewheel;
static int epollfd = 0;

//信号处理函数
void sig_handler(int sig){
	int save_errno = errno; 
	int msg = sig;
	send(pipefd[1], (char *)&sig, 1, 0);
	errno = save_errno;		//回复原来的errno;
}

//添加信号处理函数
void addsig(int sig, void (handler)(int), bool restart = true){
	struct sigaction sa;
	memset(&sa, '\0', sizeof(sa));
	sa.sa_handler = handler;
	if(restart){
		sa.sa_flags |= SA_RESTART;   //被信号打断的系统调用自动重新发起
	}
	sigfillset(&sa.sa_mask);	//将吸信号的屏蔽关键字设置为1,全部屏蔽
	assert(sigaction(sig, &sa, NULL) != -1);

}

void timer_handler(){
	timewheel.tick();
	alarm(TIMESLOT);

}

void cb_func(Client_data *user_data){
	epoll_ctl(epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
	assert(user_data);
	close(user_data->sockfd);
	LOG_INFO("close fd %d",user_data->sockfd);
	Log::get_instance()->flush();
}


void show_error(int connfd, const char *info){
	printf("%s", info);
	send(connfd,info,strlen(info), 0);
	close(connfd);

}

void addfd_(int epollfd, int fd, bool one_shot){
	epoll_event event;
	event.data.fd = fd;
	event.events = EPOLLIN | EPOLLRDHUP;
	if(one_shot){
		event.events |= EPOLLONESHOT;
	}
	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
}




int main(int argc, char *argv[]){
	Log::get_instance()->init("./mylog.log", 8192, 2000000, 0);
	if(argc <= 1){
		printf("usage: %s ip_adress port_number\n", basename(argv[0]));
		return 1;
	}

	int port = atoi(argv[1]);

	addsig(SIGPIPE, SIG_IGN);	//忽略SIGPIPE信号

	ThreadPool<Http> *pool = NULL;
	try{
		pool = new ThreadPool<Http>;
	}
	catch(...){
		return 1;
	}

	Http *users = new Http[MAX_FD];
	assert(users);
	int user_count = 0;

	int listenfd = socket(PF_INET, SOCK_STREAM, 0);
	assert(listenfd >= 0);

	int ret = 0;
	struct sockaddr_in address;
	bzero(&address, sizeof(address));
	address.sin_family = AF_INET;
	//inet_pton(AF_INET, argv[0], &address.sin_addr);
	address.sin_addr.s_addr = htonl(INADDR_ANY);
	address.sin_port = htons(port);

	int flag = 1;
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
	ret = bind(listenfd, (struct sockaddr *)&address, sizeof(address));
	printf("ret = %d", ret);	
	assert(ret >= 0);
	ret = listen(listenfd, 5);
	assert(ret >= 0);

	epoll_event events[MAX_EVENT_NUMBER];
	int epollfd = epoll_create(5);
	assert(epollfd != -1);
	addfd_(epollfd, listenfd, false);
	Http::m_epollfd = epollfd;

	ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
	assert(ret != -1);
	setnonblocking(pipefd[1]);
	addfd(epollfd, pipefd[0], false);

	addsig(SIGALRM, sig_handler, false);
	addsig(SIGTERM, sig_handler, false);
	bool stop_server = false;

	Client_data *users_timer = new Client_data[MAX_FD];
	bool timeout;
	alarm(TIMESLOT);

	while(!stop_server){
		int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
		if(number < 0 && errno != EINTR){
			LOG_ERROR("%S", "epoll failure");
			break;
		}

		for(int i = 0; i <= number; i++){
			int sockfd = events[i].data.fd;
			//如果是监听描述符，表明连接事件到来
			if(sockfd == listenfd){
				struct sockaddr_in client_address;
				socklen_t client_addrlength = sizeof(client_address);
				int connfd = accept(listenfd, (struct sockaddr *)&client_address, &client_addrlength);
				if(connfd < 0){
					LOG_ERROR("%s:errno id : %d", "accept error", errno);
					continue;
				}
				if(Http::m_user_count >= MAX_FD){
					show_error(connfd, "Internal server busy");
					LOG_ERROR("%s", "Internal server busy");
					continue;
				}
				users[connfd].init(connfd, client_address);

				users_timer[connfd].address = client_address;
				users_timer[connfd].sockfd = connfd;
				//Timer * timer = new Timer(0, 20);
				//timer->user_data= &users_timer[connfd];
				//timer->cb_func = cb_func;
				//time_t cur = time(NULL);
				//timer->expire = cur + 3*TIMESLOT;
				auto iter = timewheel.add_timer(3 * TIMESLOT);
				iter->user_data = &users_timer[connfd];
				iter->cb_func = cb_func;

				users_timer[connfd].timerIter = iter;
				

			}
			//客户端关闭连接或者客户端错误，删除对应的定时器
			else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)){
				users[sockfd].close_conn();
				//服务器关闭连接，移除对应的定时器
				cb_func(&users_timer[sockfd]);
				auto iter = users_timer[sockfd].timerIter;
				if(&(*iter)){
					timewheel.del_timer(iter);
				}
			}

			//处理信号事件
			else if ((sockfd == pipefd[0]) &&(events[i].events & EPOLLIN)){
				int sig;
				char signals[1024];
				ret = recv(pipefd[0], signals, sizeof(signals), 0);
				if(ret == -1){
					continue;
				}
				else if(ret == 0){
					continue;
				}
				else{
					for(int i = 0; i < ret; ++i){
						switch(signals[i]){
							case SIGALRM:{
								timeout = true;
								break;
							}
							case SIGTERM:{
								stop_server = true;
							}
						}
					}
				}
			}
			//客户端发送数据，socket buffer有数据可读
			else if(events[i].events & EPOLLIN){
				auto iter = users_timer[sockfd].timerIter;
				if(users[sockfd].read_once()){
					LOG_INFO("deal with the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));
					Log::get_instance()->flush();
					pool->append(users + sockfd);

					if(&(*iter) ){
						//time_t cur = time(NULL);
						//timer->expire = cur + 3 * TIMESLOT;
						timeout = iter->rotation * 60 + iter->time_slot + 3*TIMESLOT;

						auto newIter = timewheel.add_timer( timeout);
						newIter->user_data = iter->user_data;
						newIter->cb_func = iter->cb_func;
						LOG_INFO("%s", "adjust timer once");
						Log::get_instance()->flush();
						timewheel.del_timer(iter);
					}
				}
			}
			//写事件
			else if(events[i].events & EPOLLOUT){
				if(!users[sockfd].write()){
					users[sockfd].close_conn();
				}
			}		
		}

		if(timeout){
			timer_handler();
			timeout = false;
		}

	}

		close(epollfd);
		close(listenfd);
		close(pipefd[1]);
		close(pipefd[0]);
		delete [] users;
		delete [] users_timer;
		delete pool;

		return 0;
	
}
