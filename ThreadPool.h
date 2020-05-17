/*
	Author: xushibin1995
	email: 17358445010@163.com
*/

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include<list>
#include<cstdio>
#include<exception>
#include<pthrad.h>
#include"Locker.h"

template<typename T>
class ThreadPool{
public:
	ThreadPool(int thread_number = 8, int max_request = 10000);
	~ThreadPool();
	bool append(T *request);  //生产者调用的函数，往请求队列中添加任务

private:
	static void* worker(void* arg); //消费者调用的函数，是工作函数，也是线程的回调函数
	void run();

private:
	int m_thread_number;		//消费者数量，也是线程池中的线程数
	int m_max_request;			//请求队列允许的最大请求数，也就是任务队列中的产品最大数目
	pthread_t* m_threads;		//指针指向一个数组的起始地址，该数组中保存所有线程的tid
	std::list< T* > m_workqueue;	//用来装产品
	Locker m_queuelocker;		//保护请求队列的互斥锁,采用RAII机制在构造函数中自动初始化
	Sem m_queuestat;			//是否有任务需要处理，同样采用RAII机制，在构造函数中自动初始化
	bool m_stop;				//是否结束线程


};


template<typename T>
ThreadPool<T>::ThreadPool(int thread_number, int max_requests)
						  : m_thread_number(thread_number), 
							m_max_requests(max_requests),
							m_stop(false), m_threads(NULL)
{
	if(thread_number <= 0 || max_requests <= 0){
		throw std::exception();
	}
	m_threads = new pthread_t[m_threads_number];
	if(!m_threads){
		throw std::exception();
	}
	//创建thread_number个变量，并且设置未分离
	for(int i = 0; i < thread_number; ++i){
		if(pthread_create(&m_threads[i], NULL, worker, this) !=0){
			delete [] m_threads;
		    throw std::exception();
		}
		if(pthread_detach(m_threads[i])){
			delete[] m_threads;
			throw std::exception();
		}
	}

}

template <typename T>
ThreadPool<T>::~ThreadPool(){
	delete[] m_threads;
	m_stop = true;
}

template<typename T>
bool ThreadPool<T>::append(T *request){
	m_queuelocker.lock();
	if(m_woekqueue.size() > m_max_requests){   /*加上这一步判断操作，省去了另一个信号量"空位"，以及省去了对其p操作，也就是
												消费一个空位所带来的p操作，因为如果那样做就需要一开始就设定好初始状态的空位的数量，导致任务队列容量变成定额。
												也可以使用条件变量来做，达到类似的效果
												*/
		m_queuelocker.unlock();
		return false;
	}
	m_workqueue.push_back(request);
	m_queuelocker.unlock();
	m_queuestat.post();  	//生产者生产一个产品
	return true;
}

template<typename T>
void *ThreadPool<T>::worker(void *arg){
	ThreadPool *pool = (ThreadPool *)arg;
	pool->run();
	return pool;
}

template<typename T>
void ThreadPool<T>::run(){
	while(!m_stop)
	{
		m_queuestat.wait();  //wait必须在lock前面，不然会造成死锁。
		m_queuelocker.lock();//在生产者生产出产品，会唤醒多个阻塞在wait上的线程，
							// 接着他们会去抢m_queuelocker锁，没抢到锁的线程会阻塞
		if(m_workqueue.empty()){
			m_queuelocker.unlock();
			continue;		//轮询，直到产品队列不为空。
							/* 加上这一步判断操作，省去了另一个信号量"空位"，以及省去了对其v操作，也就是生产一个空位所带来的v操作，
							因为如果那样做就需要一开始就设定好初始状态的空位的数量，导致任务队列容量变成定额。
							m_workqueue空，表示没有任务，而"空位"占满了m_workqueue 。 也可以使用条件变量来做，达到类似的效果*/
		}
		T *request = m_workqueue.frot();
		m_workqueue.pop_front();
		m_queuelocker.unlock();  //解锁
		if(!request)
			continue;


	}
}



#endif