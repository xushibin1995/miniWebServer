#ifndef TIME_WHEEL_H
#define TIME_WHEEL_H
#include<time.h>
#include<netinet/in.h>
#include<stdio.h>
#include<list>
#include<vector>

using namespace std;

#define BUFFER_SIZE 1024
#define N 60
#define SI 1

class Timer;

struct Client_data{
//	typedef typename vector<Timer>::iterator iter;
	sockaddr_in address;
	int sockfd;
	char buf[BUFFER_SIZE];
//	iter timerIter;
};


class Timer{
public:
	//构造函数
	Timer(const int minutes, const int seconds ): rotation(minutes), time_slot(seconds){ }

public:
	int rotation;	//记录定时器转多少圈后失效
//	int time_slot;	//记录定时器属于时间轮上哪个槽，从而找出对应的链表。
	void (*cb_func)(Client_data*); //定时器回调函数
	Client_data* user_data;		//客户数据
	int next_index;
	int pre_index;
	operator=(Timer&& timer){
		rotation = timer.rotation;
		cb_func = timer.cb_func;
		Client_data = timer.Client_data;
	}
};


class Time_wheel{
public:
	int cur_slot;
	vector<vector<Timer> > slots(N);
	Timer_wheel(){
		for(int i = 0; i < N; i++){
			slots[i].push_back(Timer() );
			slots[i][0].next_index = 0;

			slots[i].push_back(Timer() );
			slots[i][1].next_index = 1;
		}
	}
	pair<int, int> add_timer(int timeout){
		if(timeout < 0){
			return {};
		}
		//0是空闲链表头
		//1是定时器链表头
		int rotation = timeout/60;
		int slot_index = timeout%60;
		if( slots[slot_index][0].next_index = 0){
			slots[slot_index].push_back(Tiemr(1,timeout));
			int index = slots[slot_index].size() - 1;
			slots[slot_index][index].next_index =  slots[slot_index][1].next_index;
			slots[slot_index][1].next_index = index;
			tmp_index = slots[slot_index][index].next_index;
			slots[slot_index][tmp_index].pre_index = index;
			slots[slot_index][index].pre_index = slots[slot_index][1];
		}
		else{
			index = slots[slot_index][0].next_index;
			slots[slot_index][0].next_index = slots[slot_index][index].next_index;
			slots[slot_index][index] = Timer();

			slots[slot_index][index].next_index =  slots[slot_index][1].next_index;
			slots[slot_index][1].next_index = index;
			tmp_index = slots[slot_index][index].next_index;
			slots[slot_index][tmp_index].pre_index = index;
			slots[slot_index][index].pre_index = slots[slot_index][1];
		}

		return {slot_index, index};
	}

	void delete_timer(pair<int, int> location){
		
		auto slot_index = location.first;
		auto index = location.second;
		if(slots[slot_index][1] == 1)
			return;
		int tmp_index = slots[slot_index][index].pre_index;
		slots[slot_index][tmp_index].next =  slots[slot_index][index].next_index;
		 tmp_index = slots[slot_index][index].next_index;
		slots[slot_index][tmp_index].pre_index =  slots[slot_index][index].pre_index;
			
		slots[slot_index][index].next_index =  slots[slot_index][1].next_index;
		slots[slot_index][1].next_index = index;
		tmp_index = slots[slot_index][index].next_index;
		slots[slot_index][tmp_index].pre_index = index;
		slots[slot_index][index].pre_index = slots[slot_index][1];		


	}
	void tick(){
		 auto & vec  = slots[cur_slot];
		 int tmp = vec[1].next_index;
		 while(tmp != 1){
		 	
		 	if(vec[tmp].rotation != 0){
		 		vec[tmp].rotation--;
		 	}
		 	else{
		 		delete_timer({cur_slot, tmp})
		 	}
		 	tmp = vec[tmp].next_index;
		 }
	}


};

#endif