#ifndef LOCKER_H
#define LOCKER_H

#include<exception>
#include<pthread>
#include<semaphore.h>

class Sem{
public:
	Sem(){
		//在构造函数中初始化信号量， 第二个参数，表示信号量用于单个进程内部（线程之间）
		if(sem_init(&m_sem, 0, 0) != 0){
			throw std::exception();
		}
	}
	//利用RAII机制，在析构函数中销毁信号量
	~Sem(){
		sem_destory(&m_sem);
	}

	bool wait(){
		return sem_wait(&m_sem) == 0;

	}

	bool post(){
		return sem_post(&m_sem) == 0;
	}

private:
	sem_t m_sem;
};

//线程间同步用的互斥锁
class Locker{
public:
	Locker(){
		if(pthread_mutex_init(&m_mutex, NULL) != 0){  //NULL表示使用默认属性
			throw std::exception();
		}
	}

	~Locker(){
		pthread_mutex_destory(&m_mutex);
	}

	//对互斥锁进行p操作
	bool lock(){
		return pthread_mutex_lock(&m_mutex) == 0; 
	}

	//对互斥锁进行v操作
	bool unlock(){
		return pthread_mutex_unlock(&m_mutex) == 0;
	}

private:
	pthread_mutex_t m_mutex;

};

//条件变量
class Cond{
	Cond(){
		if(pthread_mutex_init(&m_mutex, NULL) != 0){
			throw std::excetion();
		}
		if(pthread_cond_init(&m_cond, NULL) != 0){
			pthread_mutex_destory(&m_mutex); //条件变量初始化失败，销毁的互斥锁，防止资源泄露
			throw std::exception();
		}
	}

	~Cond(){
		pthread_mutex_destory(&m_mutex);
		pthread_cond_destory(&m_cond);

	}

	bool wait(){
		int ret = 0; //在加锁前定义变量是为了减小锁的粒度。
		pthread_mutex_lock(&m_mutex);
		ret = ptread_cond_wait(&m_cond, &m_mutex);
		pthread_mutex_unlock(&m_mutex);
		return ret == 0;
	}

	bool signal(){
		return pthread_cond_signal(&m_cond) == 0;
	}

private:
	pthread_mutex_t m_mutex;
	pthread_cond_t m_cond;

};

#endif