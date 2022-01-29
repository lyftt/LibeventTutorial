#include <iostream>
#include <unistd.h>
#include <cstring>
#include <signal.h>
#include <string>
#include <arpa/inet.h>
#include "event2/event.h"
#include "event2/listener.h"
#include "event2/bufferevent.h"

using namespace std;

static string recvStr = "";
static int recvCount = 0;
static int sendCount = 0;

//bufferevent 默认的低/高水位标记是0
static void read_cb(bufferevent* be, void* arg)
{
	char data[1024];
	memset(data, 0, sizeof(data));
	int len = bufferevent_read(be, data, sizeof(data) - 1);       //读取bufferevent中的输入缓冲中的数据
	if (len <= 0) return;
	//cout << "[" << data << "]" << endl;
	recvStr += data;
	recvCount += len;

	/*
	if (strstr(data, "quit") != NULL)
	{
		cout << "quit" << endl;
		bufferevent_free(be);   //退出并关闭socket,因为创建bufferevent的时候加了BEV_OPT_CLOSE_ON_FREE属性
	}*/

	bufferevent_write(be, "okokok", 7);   //发送响应数据到bufferevent中的输出缓冲区中
}

static void write_cb(bufferevent* be, void* arg)
{
	cout << "[W]" << endl;
}

//错误、超时、连接断开会触发这个回调函数
static void event_cb(bufferevent* be, short what, void* arg)
{
	cout << "[E]" << endl;

	//超时事件发生后，数据读取停止
	if (what & BEV_EVENT_TIMEOUT && what & BEV_EVENT_READING)   //判断是否为读超时事件
	{
		cout << "BEV_EVENT_TIMEOUT" << endl;
		//bufferevent_enable(be,BEV_EVENT_READING);    //超时后重新使能

		//读取缓冲中由于低水位而导致的没有读取的数据
		char data[1024] = {0};
		int len = bufferevent_read(be,data,sizeof(data) - 1);
		if (len > 0)
		{
			recvCount += len;
			recvStr += data;
		}

		bufferevent_free(be);   //释放并关闭连接BEV_OPT_CLOSE_ON_FREE，这是更多的做法,避免客户连接长期占用资源
		cout << recvStr << endl;
		cout << "recvCount=" << recvCount <<" sendCount="<<sendCount<< endl;

	}
	else if (what & BEV_ERROR)   //发生错误
	{
		bufferevent_free(be);   //如果发生的是错误，则也关闭，避免客户连接长期占用资源
	}
	else   //其他事件	
	{
		cout << "other  BEV_EVENT" << endl;
	}
}

//监听函数
static void listen_cb(struct evconnlistener* ev, evutil_socket_t s, struct sockaddr* addr, int socklen, void* args)
{
	cout << "come in" << endl;
	event_base* base = (event_base*)args;

	//创建bufferevent上下文
	bufferevent* bev = bufferevent_socket_new(base, s, BEV_OPT_CLOSE_ON_FREE);    //清理bufferevent时关闭socket
	bufferevent_enable(bev, EV_READ | EV_WRITE);        //添加读和写的监控权限

	//设置水位
	//bufferevent_setwatermark(bev,EV_READ,10,0);    //低水位10（低水位如果是0，即无限制，默认是0），高水位0（即无限制，默认也是0）
												   //这里设置了低水位10，则至少要收到10个数据，回调函数read_cb才会被回调

	bufferevent_setwatermark(bev, EV_READ, 5, 10);    //低水位5，高水位10，会分2次读取，因为读到10个字节后bufferevent就会停止读取，来触发回调函数

	bufferevent_setwatermark(bev, EV_WRITE, 5, 0);    //发送的话，只有低水位有效，这里是5，如果bufferevent的缓冲数据低于5，写入回调哈数被调用

	//超时时间的设置
	timeval tval;
	tval.tv_sec = 3;
	tval.tv_usec = 0;
	bufferevent_set_timeouts(bev, &tval, NULL);      //只设置读的3s超时，如果超时，则触发event_cb回调函数

	bufferevent_setcb(bev, read_cb, write_cb, event_cb, base);
}


//bufferevent 默认的低/高水位标记是0
static void client_read_cb(bufferevent* be, void* arg)
{
	cout << "[CLIENT R]" << endl;
}

static void client_write_cb(bufferevent* be, void* arg)
{
	cout << "[CLIENT W]" << endl;

	FILE* fp = (FILE*)arg;
	char data[1024];
	memset(data,0,sizeof(data)-1);
	int len = fread(data,1,sizeof(data)-1,fp);

	//文件出错或者读到结尾
	if (len <= 0)
	{
		//关闭文件句柄
		fclose(fp);

		//如果立刻清理，则会导致缓冲数据没有发送结束
		//bufferevent_free(be);
		bufferevent_disable(be,EV_WRITE);   //禁止触发缓冲区可写事件，因为如果缓冲区发送完，会继续触发可写事件

		return;
	}

	sendCount += len;

	//数据写入buffer
	bufferevent_write(be,data,len);

}

//错误、超时、连接断开会触发这个回调函数
static void client_event_cb(bufferevent* be, short what, void* arg)
{
	cout << "[CLIENT E]" << endl;

	//超时事件发生后，数据读取停止
	if (what & BEV_EVENT_TIMEOUT && what & BEV_EVENT_READING)   //判断是否为读超时事件
	{
		cout << "BEV_EVENT_TIMEOUT" << endl;
		//bufferevent_enable(be,BEV_EVENT_READING);    //超时后重新使能

		bufferevent_free(be);   //释放并关闭连接BEV_OPT_CLOSE_ON_FREE，这是更多的做法,避免客户连接长期占用资源
		return;
	}
	//发生错误
	else if (what & BEV_ERROR)   
	{
		cout << "BEV_ERROR" << endl;
		bufferevent_free(be);   //如果发生的是错误，则也关闭，避免客户连接长期占用资源
		return;
	}
	//连接建立事件
	else if (what & BEV_EVENT_CONNECTED)   
	{
		cout << "connected success" << endl;

		bufferevent_trigger(be,EV_WRITE,0);    //触发写事件

	}
	//服务端的连接关闭事件
	else if (what & BEV_EVENT_EOF)
	{
		cout << "connection closed" << endl;
		bufferevent_free(be);   //关闭连接
	}
	//其他事件	
	else
	{
		cout << "other  BEV_EVENT" << endl;
	}
}

void event_buffer_client()
{
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
	{
		cout << "signal error" << endl;
		return;
	}
	event_base* base = event_base_new();

	//创建网络服务器
	sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));   //ip地址也清0
	addr.sin_family = AF_INET;
	addr.sin_port = htons(5001);

	//创建一个socket监听事件，会执行socket()、bind()、listen()，并将创建一个监听事件
	evconnlistener* ev = evconnlistener_new_bind(base,
		listen_cb,      //回调函数
		base,           //回调函数的参数args
		LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE,    //地址重用和evconnlistener释放的时候关闭创建的这个监听套接字
		10,
		(sockaddr*)&addr,
		sizeof(addr));
	{
		//-1 内部自己创建socket，而不是外面创建好之后传递给它
		bufferevent* bev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);   //
		sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(5001);
		evutil_inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr.s_addr);

		//打开文件
		FILE* fp = fopen("event_buffer_client.cpp","rb");   //二进制读
		if (!fp)
		{
			cout << "open file event_buffer_client.cpp error" << endl;
			return;
		}

		//设置回调函数
		bufferevent_setcb(bev, client_read_cb, client_write_cb, client_event_cb, fp);    //fp是传入回调的参数
		bufferevent_enable(bev, EV_READ | EV_WRITE);   //设定读写权限

		//进行连接
		int ret = bufferevent_socket_connect(bev, (sockaddr*)&addr, sizeof(addr));
		if (ret == 0)
		{
			cout << "connect success" << endl;
		}
		else
		{
			cout << "connect failed" << endl;
		}
	}

	if (base)
		event_base_dispatch(base);

	if (ev)
		evconnlistener_free(ev);

	event_base_free(base);
}
