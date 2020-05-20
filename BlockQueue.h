#ifndef BLOCKQUEUE_H
#define BLOCKQUEUE_H

#include<iostream>
#include<stdlib.h>
#include<pthread.h>
#include<sys/time.h>
#include<Locker.h>

using namespace std;

template<typename T>
class BlockQueue{
public:
	BlockQueue(int max_size = 1000){
		if(max_size <= 0){
			m_max_size = 1;
		}
		else{
			m_max_size = max_size;
		}

		m_array = new T[max_size];
		m_size = 0;
		m_front = -1;
		m_back = -1;

	}


	void clear(){
		m_mutex.lock();
		m_size = 0;
		m_front = -1;
		m_back = -1;
		m_mutex.unlock();
	}

	~BlockQueue(){
		m_mutex.lock();
		delete [] m_array;
		m_mutex.unlock();		
	}

	bool full() const{
		m_mutex.lock();
		if(m_size >= m_max_size){
			m_mutex.unlock();
			return true;
		}
		m_mutex.unlock();
		return false;
	}

	bool empty() const{
		m_mutex.lock();
		if(0 == m_size0){
			m_mutex.unlock();
			return true;
		}
		m_mutex.unlock();
		return false;
	}

	T& front(){
		m_mutex.lock();
		if(0 == msize)
		{
			m_mutex.unlock();
			return *(T*)NULL;
		}
		m_mutex.unlock();
		return m_array[m_front];
	}

	T& back(){
		m_mutex.lock();
		if(0 == msize)
		{
			m_mutex.unlock();
			return *(T*)NULL;
		}
		m_mutex.unlock();
		return m_array[m_back];
	}

	int size() const{
		int tmp = 0;
		m_mutex.lock();
		tmp = m_size;
		m_mutex.unlock();
		return tmp;
	}

	int maxSize() const{
		int tmp =0;
		m_mutex.lock();
		tmp = m_max_size;
		m_mutex.unlock();
		return tmp;
	} 

	bool push(const T& item){
		m_mutex.lock();
		if(m_sie >= m_max_size){
			m_cond.broadcast();
			m_mutex.unlock();
			return false;
		}
		m_back = (m_back + 1) % m_max_size;
		m_array[m_back] = item;
		m_size++;

		m_cond.signal();
		m_mutex.unlock();
		return true;
	}

	T& pop(){
		m_mutex.lock();
		while(m_size <= 0){
			m_cond.wait(m_mutex);
		}

		m_front = (m_front + 1) % m_max_size;
		m_size--;
		m-mutex.unlock();
		return m_array[m_front];
	}

};