#include <iostream>
#include <unistd.h>
#include <cstring>
#include <signal.h>
#include <arpa/inet.h>
#include "event2/event.h"
#include "event2/listener.h"
#include "event2/thread.h"

using namespace std;

//定时时间，1s，0us
static timeval t1 = {1,0};
static timeval t2 = {1,200000};
static timeval tv_in = {3,0};

//非持久化
//void timer1_cb(evutil_socket_t sock, short which, void* arg)
//{
//	cout << "timer1 arrival" << endl;
//
//	//非持久时间进入后，会回到no pending状态，需要重新添加，添加之前先删除
//	event* ev = (event*)arg;
//	if (!evtimer_pending(ev,&t1))
//	{
//		evtimer_del(ev);
//		evtimer_add(ev,&t1);
//	}
//}

//非持久化
void timer1_cb(evutil_socket_t sock, short which, void* arg)
{
	cout << "timer1 arrival" << endl;

	//非持久时间进入后，会回到no pending状态，需要重新添加，添加之前先删除
	event* ev = (event*)arg;
	if (!evtimer_pending(ev, NULL))
	{
		evtimer_del(ev);
		event_base* base = event_get_base(ev);
		event* ev1 = evtimer_new(base, timer1_cb, event_self_cbarg());
		timeval t1 = { 5,0 };
		evtimer_add(ev1, &t1);
	}
}

//持久化
void timer2_cb(evutil_socket_t sock, short which, void* arg)
{
	cout << "timer2 arrival" << endl;
}

//使用双向队列替换二叉堆来进行优化
void timer3_cb(evutil_socket_t sock, short which, void* arg)
{
	cout << "timer3 arrival" << endl;
}


void timer_handler()
{
	//创建上下文
	event_base* base = event_base_new();

	//evtimer_new 也是对event_new接口的隐藏，来方便使用
	// #define evtimer_new(b, cb, arg)	       event_new((b), -1, 0, (cb), (arg))
	//这里的ev1是非持久化
	event* ev1 = evtimer_new(base,timer1_cb,event_self_cbarg());
	if (!ev1)
	{
		cout << "evtimer_new ev1 error" << endl;
		return;
	}
	evtimer_add(ev1,&t1);

	//持久化定时器
	//event* ev2 = event_new(base,-1,EV_PERSIST,timer2_cb,NULL);
	//event_add(ev2,&t2);

	//定时器性能优化，默认libevent使用的是二叉堆（最小堆、最大堆），插入删除O(logn)
	//优化到双向队列，插入删除就是O(1)
	//event* ev3 = event_new(base,-1,EV_PERSIST,timer3_cb,NULL);
	//const timeval* t3 = event_base_init_common_timeout(base,&tv_in);   //使用双向队列优化
	//event_add(ev3,t3);   //插入性能O(1)

	event_base_dispatch(base);

	event_free(ev1);
	event_base_free(base);
}