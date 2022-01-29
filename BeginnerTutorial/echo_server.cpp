#include <iostream>
#include <unistd.h>
#include <cstring>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <memory>
#include "event2/event.h"
#include "event2/listener.h"
#include "event2/thread.h"
#include <thread>
#include <errno.h>
#include <vector>

#define SPORT 5001

using namespace std;

//s是发生事件的描述符
//w是描述符发生的事件类型
//args是传递的参数
//有数据可读时，会触发这个回调函数
//如果客户端断开连接,也会触发这个回调函数
//超时也会触发这个回调函数
void read_cb(evutil_socket_t s, short w, void* arg)
{
	cout << "." << endl;
#if 1
	event* ev = (event*)arg;
	//判断超时
	if (w & EV_TIMEOUT)
	{
		cout << "TIMEOUT" << endl;
		event_free(ev);         //清理事件
		evutil_closesocket(s);  //关闭socket
		return;
	}

	char buffer[1024];

	int len = recv(s,buffer,sizeof(buffer)-1,0);
	if (len > 0)
	{
		cout << "Get " << len << endl;
		cout << "Get msg:" << buffer << endl;

		send(s,"ok",2,0);
	}
	else if(len == 0)
	{
		//需要清理event
		cout << "len == 0" << endl;
		event_free(ev);         //清理事件
		evutil_closesocket(s);  //关闭socket
	}
	else
	{
		//需要清理event
		cout << "len < 0" << endl;
		event* ev = (event*)arg;
		event_free(ev);
	}
#endif
}


//如果是LT触发模式，则会一直触发这个函数的调用
void listen_cb(evutil_socket_t s,short w,void* arg)
{
	event_base* base = (event_base*)arg;
	cout << "listen_cb" << endl;
	sockaddr_in clientAddr;
	socklen_t size = sizeof(clientAddr);
	memset(&clientAddr,0,sizeof(clientAddr));

	//读取连接信息
	evutil_socket_t sock = accept(s,(sockaddr*)&clientAddr,&size);   //取连接socket
	char ip[16];
	evutil_inet_ntop(AF_INET,&clientAddr.sin_addr,ip,sizeof(ip)-1);
	cout << "client IP:" << ip << endl;

	//设置客户端数据的读取事件
	struct timeval tm;
	tm.tv_sec = 5;
	tm.tv_usec = 0;
	//event* ev_read = event_new(base,sock,EV_READ | EV_PERSIST,read_cb,event_self_cbarg());   //LT触发
	event* ev_read = event_new(base, sock, EV_READ | EV_PERSIST | EV_ET, read_cb, event_self_cbarg());  //ET触发
	event_add(ev_read,&tm);   //设置超时
}

void echo_server()
{
	event_base* base = event_base_new();

	evutil_socket_t sock = socket(AF_INET,SOCK_STREAM,0);
	if (sock <= 0)
	{
		cout << "socket error, " << strerror(errno) << endl;
		return;
	}

	//设置地址复用和非阻塞
	evutil_make_listen_socket_reuseable(sock);
	evutil_make_socket_nonblocking(sock);

	struct sockaddr_in addr;
	memset(&addr,0,sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(SPORT);

	int ret = ::bind(sock,(sockaddr*)&addr,sizeof(addr));
	if (ret != 0)
	{
		cout << "bind error, " << strerror(errno) << endl;
		return;
	}

	ret = listen(sock,10);
	if (ret < 0)
	{
		cout << "listen error, " << strerror(errno) << endl;
		return;
	}

	/*添加对监听描述符的读事件,默认LT触发，Epoll模型*/
	event* ev = event_new(base,sock,EV_READ | EV_PERSIST, listen_cb,base);
	/*设置ET触发*/
	//event* ev = event_new(base,sock,EV_READ | EV_PERSIST | EV_ET,listen_cb,base);
	event_add(ev,NULL);    //到达待决状态

	event_base_dispatch(base);

	event_free(ev);
	evutil_closesocket(sock);   //关闭连接
	event_base_free(base);
}