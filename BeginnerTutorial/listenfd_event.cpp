/*
* 
* ����libevent�Ľӿ�������socket������listen���������м��������ü򵥵��¼��ص�����
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
	/*����libevent������*/
	event_base* base = event_base_new();

	/*�����˿�*/
	/*socket()��bind()��listen()�Ѿ������������¼�(�����¼�)*/
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_port = htons(ADDR_PORT);
	addr.sin_family = AF_INET;
	inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
	evconnlistener* ev = evconnlistener_new_bind(base,
		listen_cb,          //���յ����ӵĻص�����
		base,               //�ص�������ȡ�Ĳ���
		LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE,       //��ַ���ã�listen�ر�ͬʱ�ر�socket
		10,                //listen��������������Ӷ��е�����
		(sockaddr*)&addr,
		sizeof(addr)
	);

	/*�¼��ַ�����*/
	if (base)
		event_base_dispatch(base);

	if (ev)
		evconnlistener_free(ev);

	/*����libevent*/
	if (base)
	{
		cout << "success" << endl;
	}

	sleep(2);
}