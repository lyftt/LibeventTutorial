/*
* ������
* ����ʱ�� �����Ⱦ���filter�ٷ���
* ����ʱ�� �����Ⱦ���filter�ٶ�ȡ
* 
* ��Ҫ�������ݵļ��ܡ����ܡ�ѹ������ѹ��
* 
* ��bufferevent_read ֮ǰ����
* ��bufferevent_write ֮�����
* 
* bufferevent �� evbuffer
* 
* bufferevent�ڲ���һ��evbuffer�������Ǵ�������evbuffer�еģ�
* ��������Ҫ�����evbuffer�е����ݽ��д���
* 
* ��Ҫʹ�õļ���������:
*   1.evbuffer_remove    ��evbuffer�е������Ƴ�����
*   2.evbuffer_add       ���������������·���evbuffer
*   3.bufferevent_filter_new   ����һ�����������ڴ���һ��������֮ǰ�������Ѿ�����һ��bufferevent
* 
* �������� BEV_OPT_CLOSE_ON_FREE ��ʾ�رչ�������ʱ���ͬ�¹ر�bufferevent
* 
* 
* ����socket��listen_cb----->bufferevent_filter��filter_in---->bufferevent��read_cb
* 
* 
*/

#include <iostream>
#include <unistd.h>
#include <cstring>
#include <signal.h>
#include <string>
#include <arpa/inet.h>
#include "event2/event.h"
#include "event2/listener.h"
#include "event2/bufferevent.h"
#include "event2/bufferevent_struct.h"
#include "event2/buffer.h"
#include "ctype.h"

#define ADDR_PORT 5001

using namespace std;

//bufferevent Ĭ�ϵĵ�/��ˮλ�����0
static void read_cb(bufferevent* be, void* arg)
{
	cout << "[R]" << endl;

	//��ȡfilter����������
	char data[1024] = {0};
	int len = bufferevent_read(be,data,sizeof(data)-1);
	cout << data << endl;

	//�ָ���Ϣ���������������
	bufferevent_write(be,data,len);
}

static void write_cb(bufferevent* be, void* arg)
{
	cout << "[W]" << endl;
}

//���󡢳�ʱ�����ӶϿ��ᴥ������ص�����
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
// filter_in---->read_cb
//
static bufferevent_filter_result filter_in(struct evbuffer* src, struct evbuffer* dst, ev_ssize_t dst_limit,
	enum bufferevent_flush_mode mode, void* ctx)
{
	cout << "filter in" << endl;

	//��ȡ����������
	char data[1024] = {0};
	int len = evbuffer_remove(src,data,sizeof(data)-1);

	//������ĸת���ɴ�д(�򵥵Ĵ���)
	for (int i = 0; i < len; ++i)
	{
		data[i] = toupper(data[i]);
	}

	//��������ݺ����·���evbuffer
	evbuffer_add(dst,data,len);

	return BEV_OK;
}

static bufferevent_filter_result filter_out(struct evbuffer* src, struct evbuffer* dst, ev_ssize_t dst_limit,
	enum bufferevent_flush_mode mode, void* ctx)
{
	cout << "filter out" << endl;

	//��ȡ����������
	char data[1024] = { 0 };
	int len = evbuffer_remove(src, data, sizeof(data) - 1);

	//���ͷ����Ϣ(�򵥵Ĵ���)
	string str = "";
	str += "==============\n";
	str += data;
	str += "==============\n";

	//��������ݺ����·���evbuffer
	evbuffer_add(dst, str.c_str(), str.size());

	return BEV_OK;
}

static void listen_cb(struct evconnlistener* e, evutil_socket_t s, struct sockaddr* a, int socklen, void* arg)
{
	event_base* base = (event_base*)arg;
	cout << "listen callback" << endl;

	//����bufferevent������bufferevent_filter
	bufferevent *bev = bufferevent_socket_new(base,s,BEV_OPT_CLOSE_ON_FREE);

	//�󶨵�bufferevent_filter������
	bufferevent *bev_filter = bufferevent_filter_new(bev,
		                   filter_in,      //������˺���
			               filter_out,     //������˺���
		                   BEV_OPT_CLOSE_ON_FREE, //�ر�filter��ʱ��ر�bufferevent
		                   0,           //����Ļص�����
		                   0            //�ص������Ĳ���
		                   );

	//����bufferevent�Ļص�������ֱ������bev_filter���У�������Ҳ�Ǹ�bufferevent
	bufferevent_setcb(bev_filter,read_cb,write_cb, event_cb,NULL);

	//����bufferevent�Ķ�дȨ��
	bufferevent_enable(bev_filter,EV_WRITE | EV_READ);
}

void bufferevent_filter_demo()
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