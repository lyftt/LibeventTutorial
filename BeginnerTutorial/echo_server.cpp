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

//s�Ƿ����¼���������
//w���������������¼�����
//args�Ǵ��ݵĲ���
//�����ݿɶ�ʱ���ᴥ������ص�����
//����ͻ��˶Ͽ�����,Ҳ�ᴥ������ص�����
//��ʱҲ�ᴥ������ص�����
void read_cb(evutil_socket_t s, short w, void* arg)
{
	cout << "." << endl;
#if 1
	event* ev = (event*)arg;
	//�жϳ�ʱ
	if (w & EV_TIMEOUT)
	{
		cout << "TIMEOUT" << endl;
		event_free(ev);         //�����¼�
		evutil_closesocket(s);  //�ر�socket
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
		//��Ҫ����event
		cout << "len == 0" << endl;
		event_free(ev);         //�����¼�
		evutil_closesocket(s);  //�ر�socket
	}
	else
	{
		//��Ҫ����event
		cout << "len < 0" << endl;
		event* ev = (event*)arg;
		event_free(ev);
	}
#endif
}


//�����LT����ģʽ�����һֱ������������ĵ���
void listen_cb(evutil_socket_t s,short w,void* arg)
{
	event_base* base = (event_base*)arg;
	cout << "listen_cb" << endl;
	sockaddr_in clientAddr;
	socklen_t size = sizeof(clientAddr);
	memset(&clientAddr,0,sizeof(clientAddr));

	//��ȡ������Ϣ
	evutil_socket_t sock = accept(s,(sockaddr*)&clientAddr,&size);   //ȡ����socket
	char ip[16];
	evutil_inet_ntop(AF_INET,&clientAddr.sin_addr,ip,sizeof(ip)-1);
	cout << "client IP:" << ip << endl;

	//���ÿͻ������ݵĶ�ȡ�¼�
	struct timeval tm;
	tm.tv_sec = 5;
	tm.tv_usec = 0;
	//event* ev_read = event_new(base,sock,EV_READ | EV_PERSIST,read_cb,event_self_cbarg());   //LT����
	event* ev_read = event_new(base, sock, EV_READ | EV_PERSIST | EV_ET, read_cb, event_self_cbarg());  //ET����
	event_add(ev_read,&tm);   //���ó�ʱ
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

	//���õ�ַ���úͷ�����
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

	/*��ӶԼ����������Ķ��¼�,Ĭ��LT������Epollģ��*/
	event* ev = event_new(base,sock,EV_READ | EV_PERSIST, listen_cb,base);
	/*����ET����*/
	//event* ev = event_new(base,sock,EV_READ | EV_PERSIST | EV_ET,listen_cb,base);
	event_add(ev,NULL);    //�������״̬

	event_base_dispatch(base);

	event_free(ev);
	evutil_closesocket(sock);   //�ر�����
	event_base_free(base);
}