#include <iostream>
#include <unistd.h>
#include <cstring>
#include <signal.h>
#include <arpa/inet.h>
#include "event2/event.h"
#include "event2/listener.h"
#include "event2/bufferevent.h"

using namespace std;

//bufferevent Ĭ�ϵĵ�/��ˮλ�����0
static void read_cb(bufferevent* be,void *arg)
{
	char data[1024];
	memset(data,0,sizeof(data));
	int len = bufferevent_read(be,data,sizeof(data) - 1);       //��ȡbufferevent�е����뻺���е�����
	if (len <= 0) return;
	cout << "[" <<data<<"]"<< endl;
	/*
	if (strstr(data, "quit") != NULL)
	{
		cout << "quit" << endl;
		bufferevent_free(be);   //�˳����ر�socket,��Ϊ����bufferevent��ʱ�����BEV_OPT_CLOSE_ON_FREE����
	}*/

	bufferevent_write(be,"okokok",7);   //������Ӧ���ݵ�bufferevent�е������������
}

static void write_cb(bufferevent* be, void* arg)
{
	cout << "[W]" << endl;
}

//���󡢳�ʱ�����ӶϿ��ᴥ������ص�����
static void event_cb(bufferevent* be,short what, void* arg)
{
	cout << "[E]" << endl;

	//��ʱ�¼����������ݶ�ȡֹͣ
	if (what & BEV_EVENT_TIMEOUT && what & BEV_EVENT_READING)   //�ж��Ƿ�Ϊ����ʱ�¼�
	{
		cout << "BEV_EVENT_TIMEOUT" << endl;
		//bufferevent_enable(be,BEV_EVENT_READING);    //��ʱ������ʹ��

		bufferevent_free(be);   //�ͷŲ��ر�����BEV_OPT_CLOSE_ON_FREE�����Ǹ��������,����ͻ����ӳ���ռ����Դ
	}
	else if (what & BEV_ERROR)   //��������
	{
		bufferevent_free(be);   //����������Ǵ�����Ҳ�رգ�����ͻ����ӳ���ռ����Դ
	}
	else   //�����¼�	
	{
		cout << "other  BEV_EVENT" << endl;
	}
}

//��������
static void listen_cb(struct evconnlistener* ev, evutil_socket_t s, struct sockaddr*addr, int socklen, void* args)
{
	cout << "come in" << endl;
	event_base* base = (event_base*)args;

	//����bufferevent������
	bufferevent *bev = bufferevent_socket_new(base,s,BEV_OPT_CLOSE_ON_FREE);    //����buffereventʱ�ر�socket
	bufferevent_enable(bev,EV_READ | EV_WRITE);        //��Ӷ���д�ļ��Ȩ��

	//����ˮλ��һ�㲻��
	//bufferevent_setwatermark(bev,EV_READ,10,0);    //��ˮλ10����ˮλ�����0���������ƣ�Ĭ����0������ˮλ0���������ƣ�Ĭ��Ҳ��0��
	                                               //���������˵�ˮλ10��������Ҫ�յ�10�����ݣ��ص�����read_cb�Żᱻ�ص�

	bufferevent_setwatermark(bev,EV_READ,5,10);    //��ˮλ5����ˮλ10�����2�ζ�ȡ����Ϊ����10���ֽں�bufferevent�ͻ�ֹͣ��ȡ���������ص�����

	bufferevent_setwatermark(bev,EV_WRITE,5,0);    //���͵Ļ���ֻ�е�ˮλ��Ч��������5�����bufferevent�Ļ������ݵ���5��д��ص�����������

	//��ʱʱ�������
	timeval tval;
	tval.tv_sec = 3;
	tval.tv_usec = 0;
	bufferevent_set_timeouts(bev,&tval,NULL);      //ֻ���ö���3s��ʱ�������ʱ���򴥷�event_cb�ص�����

	bufferevent_setcb(bev,read_cb,write_cb,event_cb,base);
}

void event_buffer_demo()
{
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
	{
		cout << "signal error" << endl;
		return;
	}
	event_base* base = event_base_new();

	//�������������
	sockaddr_in addr;
	memset(&addr,0,sizeof(addr));   //ip��ַҲ��0
	addr.sin_family = AF_INET;
	addr.sin_port = htons(5001);

	//����һ��socket�����¼�����ִ��socket()��bind()��listen()����������һ�������¼�
	evconnlistener* ev =  evconnlistener_new_bind(base,
		                    listen_cb,      //�ص�����
		                    base,           //�ص������Ĳ���args
							LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE,    //��ַ���ú�evconnlistener�ͷŵ�ʱ��رմ�������������׽���
							10,
							(sockaddr*)&addr,
							sizeof(addr));

	if(base)
		event_base_dispatch(base);

	if (ev)
		evconnlistener_free(ev);

	event_base_free(base);

}