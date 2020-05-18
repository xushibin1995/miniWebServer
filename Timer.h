#ifndef TIME_WHEEL_H
#define TIME_WHEEL_H

#include<time.h>
#include<netinet/in.h>
#include<stdio.h>
#include<list>
#include<vector>

using namespace std;

#define BUFFER_SIZE 64
#define N  60 //时间轮上的数目
#define SI  1 //每一秒转动一次，即槽的间隔为1s

class Timer;

struct Client_data{
	sockaddr_in address;
	int sockfd;
	char buf[BUFFER_SIZE];
	Timer* timer;
};


class Timer{
public:
	//构造函数
	Timer(const int minutes, const int seconds ): rotation(minutes), time_slot(seconds){ }

public:
	int rotation;	//记录定时器转多少圈后失效
	int time_slot;	//记录定时器属于时间轮上哪个槽，从而找出对应的链表。
	void (*cb_func)(Client_data*); //定时器回调函数
	Client_data* user_data;		//客户数据
};


/*
	时间轮：轮子外层是一个vector构成的循环对列，vector的每一个格子(slot)中存放一个指向一条链表list的指针，
	而list链表将所有定时器timer串联起来。
	不同的槽对应不同的计时秒数，timer中的rotation代表计时分钟数
*/
class Time_wheel{
public:
    typedef typename list<Timer>::iterator iter;		

	//构造
	Time_wheel() : cur_slot(0){
		for(int i = 0; i < N; ++i){
			slots.push_back(new list<Timer> );
		}
	}

	//析构
	~Time_wheel(){
		for(int i = 0; i < N; i++){
			(*slots[i]).clear();
		}
	}

	//添加定时器
	Timer* add_timer(int timeout){
		if(timeout < 0){
			return NULL;
		}
		int ticks = (timeout) < SI ? 1 : (timeout / SI);

		int rotation = ticks / N;
		int ts = (cur_slot + ticks) % N;
		(*slots[ts]).push_back(Timer(rotation, ts) );

	}

	//删除定时器
	void del_timer(iter timer){
			int ts = timer->time_slot;
			(*slots[ts]).erase(timer);
	}

	//秒针走一格
	void tick(){
        for(auto listPtr = (*slots[cur_slot]).begin(); listPtr != (*slots[cur_slot]).end(); listPtr++){
			if(listPtr->rotation > 0){
				listPtr->rotation--;
			}
			else{
				listPtr->cb_func(listPtr->user_data);
				del_timer(listPtr);
			}
		}
		cur_slot = ++cur_slot / N;
	}

private:
	int cur_slot;				   //当前时刻秒针指向的槽
	vector< list<Timer>* > slots;  //时间轮,共有N个槽
};

#endif