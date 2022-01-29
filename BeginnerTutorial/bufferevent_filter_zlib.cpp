
#include <iostream>
#include <unistd.h>
#include <cstring>
#include <signal.h>
#include <string>
#include <vector>
#include <arpa/inet.h>
#include "event2/event.h"
#include "event2/listener.h"
#include "event2/bufferevent.h"
#include "event2/bufferevent_struct.h"
#include "event2/buffer.h"
#include "ctype.h"

#define ADDR_PORT 5001

using namespace std;


//bufferevent 默认的低/高水位标记是0
static void read_cb(bufferevent* be, void* arg)
{
	cout << "[R]" << endl;

	//读取filter处理后的数据
	char data[1024] = { 0 };
	int len = bufferevent_read(be, data, sizeof(data) - 1);
	cout << data << endl;

	//恢复消息，经过输出过滤器
	bufferevent_write(be, data, len);
}

static void write_cb(bufferevent* be, void* arg)
{
	cout << "[W]" << endl;
}

//错误、超时、连接断开会触发这个回调函数
static void event_cb(bufferevent* be, short what, void* arg)
{
	cout << "[E]" << endl;

	if (what & BEV_EVENT_EOF)
	{
		cout << "connection closed" << endl;
		bufferevent_free(be);
		return;
	}
	else if (what & BEV_EVENT_ERROR)
	{
		cout << "connection error" << endl;
		bufferevent_free(be);
		return;
	}
	else
	{
		cout << "other event" << endl;
	}
}

//
// filter_in---->read_cb---->cout
//
static bufferevent_filter_result filter_in(struct evbuffer* src, struct evbuffer* dst, ev_ssize_t dst_limit,
	enum bufferevent_flush_mode mode, void* ctx)
{
	cout << "filter in" << endl;

	//读取并清理数据
	char data[1024] = { 0 };
	int len = evbuffer_remove(src, data, sizeof(data) - 1);

	//所有字母转换成大写(简单的处理)
	for (int i = 0; i < len; ++i)
	{
		data[i] = toupper(data[i]);
	}

	//处理好数据后重新放入evbuffer
	evbuffer_add(dst, data, len);

	return BEV_OK;
}

static bufferevent_filter_result filter_out(struct evbuffer* src, struct evbuffer* dst, ev_ssize_t dst_limit,
	enum bufferevent_flush_mode mode, void* ctx)
{
	cout << "filter out" << endl;

	//读取并清理数据
	char data[1024] = { 0 };
	int len = evbuffer_remove(src, data, sizeof(data) - 1);

	//添加头部消息(简单的处理)
	string str = "";
	str += "==============\n";
	str += data;
	str += "==============\n";

	//处理好数据后重新放入evbuffer
	evbuffer_add(dst, str.c_str(), str.size());

	return BEV_OK;
}

static void listen_cb(struct evconnlistener* e, evutil_socket_t s, struct sockaddr* a, int socklen, void* arg)
{
	event_base* base = (event_base*)arg;
	cout << "listen callback" << endl;

	//创建bufferevent，并绑定bufferevent_filter
	bufferevent* bev = bufferevent_socket_new(base, s, BEV_OPT_CLOSE_ON_FREE);

	//绑定到bufferevent_filter过滤器
	bufferevent* bev_filter = bufferevent_filter_new(bev,
		filter_in,      //输入过滤函数
		filter_out,     //输出过滤函数
		BEV_OPT_CLOSE_ON_FREE, //关闭filter的时候关闭bufferevent
		0,           //清理的回调函数
		0            //回调函数的参数
	);

	//设置bufferevent的回调函数，直接设置bev_filter就行，它本身也是个bufferevent
	bufferevent_setcb(bev_filter, read_cb, write_cb, event_cb, NULL);

	//开启bufferevent的读写权限
	bufferevent_enable(bev_filter, EV_WRITE | EV_READ);
}

void bufferevent_filter_zlib()
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
