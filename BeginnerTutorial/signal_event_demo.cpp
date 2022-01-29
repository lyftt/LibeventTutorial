/*
* 
*          invalid ptr
*              *
*              * event_new()
*              *
*             \|/
*          non-pending
*             /|\
*              *
*   event_del()* event_add()
*              *
*             \|/
*            pending
*             /|\
*              *
*              * 
*              *
*             \|/
*            active
* 
* 
* 
* 
*/


#include <iostream>
#include <unistd.h>
#include <cstring>
#include <signal.h>
#include <arpa/inet.h>
#include "event2/event.h"
#include "event2/listener.h"
#include "event2/thread.h"

using namespace std;

/*
* 
* sock 文件描述符
* which 事件类型，例如超时、信号、读写
* arg 参数
* 
*/
static void Ctrl_C_cb(int sock,short which,void *arg)
{
	cout << "Ctrl-C" << endl;
	event_base* base = (event_base*)arg;
	//event_base_loopbreak(base);   //执行完当前处理的事件函数就立即退出
	event_base_loopexit(base,NULL); //执行完当前的所有活动事件的处理函数之后才退出,可以设置一个超时时间（至少运行这么多的时间）; 如果事件循环还没有开始运行的时候，也要等运行一次再退出
}

static void Kill(int sock, short which, void* arg)
{
	cout << "Kill" << endl;
	event* ev = (event *)arg;

	//由于是非持久事件，进入到回调函数之后，就是no pending状态了
	//if(evsignal_pending(ev,NULL)),这样也可以
	if (!event_pending(ev,EV_SIGNAL,NULL))
	{
		event_del(ev);
		event_add(ev,NULL);
	}
}

void signal_handler()
{
	event_base* base = event_base_new();

	//libevent通过一些宏来隐藏接口,evsignal_new本质就是调用event_new
	//evsignal_new到达no pending状态
	event* csignal = evsignal_new(base,SIGINT, Ctrl_C_cb,base);
	if (!csignal)
	{
		cout << "SIGINT evsignal_new error" << endl;
		return;
	}

	//添加事件，到 pending
	if (event_add(csignal,0) != 0)
	{
		cout << "SIGINT evsignal_new error" << endl;
		return;
	}

	//不使用evsignal_new宏
	//非持久事件,只会进入一次
	//event_self_cbarg传递当前的event,因为此时ksignal还没初始化好
	event* ksignal = event_new(base,SIGTERM,EV_SIGNAL,Kill,event_self_cbarg());
	if (!ksignal)
	{
		cout << "SIGTERM event_new error" << endl;
		return;
	}

	if (event_add(ksignal, 0) != 0)
	{
		cout << "SIGTERM evsignal_new error" << endl;
		return;
	}

	//开始事件循环
	//event_base_dispatch(base);            //等价于event_base_loop(base,0),即不使用任何选项，如果 开启事件循环的时候还没有注册任何事件，则也会立即返回,如果添加了事件则会阻塞，不会立即返回
	//event_base_loop(base,EVLOOP_ONCE);    //等待一个事件运行，然后处理完所有活动事件直到没有活动事件后，才退出
	/*
	bool isexit = false;
	while (!isexit)
	{
		event_base_loop(base, EVLOOP_NONBLOCK);  //有活动事件旧处理，没有就立即返回，即非阻塞模式（默认是阻塞的）
	}*/
	event_base_loop(base,EVLOOP_NO_EXIT_ON_EMPTY);  //即使没有事件注册到base，也不返回，即阻塞模式,可以在后期多线程的时候动态添加
	

	event_free(csignal);
	event_base_free(base);
}



