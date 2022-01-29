/*
* 
* 利用libevent的接口来创建socket，并对listen描述符进行监听，设置简单的事件回调函数
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

#define ADDR_PORT 5001

using namespace std;

static void listen_cb(struct evconnlistener* e, evutil_socket_t s, struct sockaddr* a, int socklen, void* arg)
{
	cout << "listen callback" << endl;
}

void listen_demo()
{
	/*创建libevent上下文*/
	event_base* base = event_base_new();

	/*监听端口*/
	/*socket()、bind()、listen()已经都包含，绑定事件(连接事件)*/
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_port = htons(ADDR_PORT);
	addr.sin_family = AF_INET;
	inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
	evconnlistener* ev = evconnlistener_new_bind(base,
		listen_cb,          //接收到连接的回调函数
		base,               //回调函数获取的参数
		LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE,       //地址重用，listen关闭同时关闭socket
		10,                //listen函数，已完成连接队列的数量
		(sockaddr*)&addr,
		sizeof(addr)
	);

	/*事件分发处理*/
	if (base)
		event_base_dispatch(base);

	if (ev)
		evconnlistener_free(ev);

	/*销毁libevent*/
	if (base)
	{
		cout << "success" << endl;
	}

	sleep(2);
}