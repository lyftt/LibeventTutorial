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

//bufferevent Ĭ�ϵĵ�/��ˮλ�����0
static void read_cb(bufferevent* be, void* arg)
{
	char data[1024];
	memset(data, 0, sizeof(data));
	int len = bufferevent_read(be, data, sizeof(data) - 1);       //��ȡbufferevent�е����뻺���е�����
	if (len <= 0) return;
	//cout << "[" << data << "]" << endl;
	recvStr += data;
	recvCount += len;

	/*
	if (strstr(data, "quit") != NULL)
	{
		cout << "quit" << endl;
		bufferevent_free(be);   //�˳����ر�socket,��Ϊ����bufferevent��ʱ�����BEV_OPT_CLOSE_ON_FREE����
	}*/

	bufferevent_write(be, "okokok", 7);   //������Ӧ���ݵ�bufferevent�е������������
}

static void write_cb(bufferevent* be, void* arg)
{
	cout << "[W]" << endl;
}

//���󡢳�ʱ�����ӶϿ��ᴥ������ص�����
static void event_cb(bufferevent* be, short what, void* arg)
{
	cout << "[E]" << endl;

	//��ʱ�¼����������ݶ�ȡֹͣ
	if (what & BEV_EVENT_TIMEOUT && what & BEV_EVENT_READING)   //�ж��Ƿ�Ϊ����ʱ�¼�
	{
		cout << "BEV_EVENT_TIMEOUT" << endl;
		//bufferevent_enable(be,BEV_EVENT_READING);    //��ʱ������ʹ��

		//��ȡ���������ڵ�ˮλ�����µ�û�ж�ȡ������
		char data[1024] = {0};
		int len = bufferevent_read(be,data,sizeof(data) - 1);
		if (len > 0)
		{
			recvCount += len;
			recvStr += data;
		}

		bufferevent_free(be);   //�ͷŲ��ر�����BEV_OPT_CLOSE_ON_FREE�����Ǹ��������,����ͻ����ӳ���ռ����Դ
		cout << recvStr << endl;
		cout << "recvCount=" << recvCount <<" sendCount="<<sendCount<< endl;

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
static void listen_cb(struct evconnlistener* ev, evutil_socket_t s, struct sockaddr* addr, int socklen, void* args)
{
	cout << "come in" << endl;
	event_base* base = (event_base*)args;

	//����bufferevent������
	bufferevent* bev = bufferevent_socket_new(base, s, BEV_OPT_CLOSE_ON_FREE);    //����buffereventʱ�ر�socket
	bufferevent_enable(bev, EV_READ | EV_WRITE);        //��Ӷ���д�ļ��Ȩ��

	//����ˮλ
	//bufferevent_setwatermark(bev,EV_READ,10,0);    //��ˮλ10����ˮλ�����0���������ƣ�Ĭ����0������ˮλ0���������ƣ�Ĭ��Ҳ��0��
												   //���������˵�ˮλ10��������Ҫ�յ�10�����ݣ��ص�����read_cb�Żᱻ�ص�

	bufferevent_setwatermark(bev, EV_READ, 5, 10);    //��ˮλ5����ˮλ10�����2�ζ�ȡ����Ϊ����10���ֽں�bufferevent�ͻ�ֹͣ��ȡ���������ص�����

	bufferevent_setwatermark(bev, EV_WRITE, 5, 0);    //���͵Ļ���ֻ�е�ˮλ��Ч��������5�����bufferevent�Ļ������ݵ���5��д��ص�����������

	//��ʱʱ�������
	timeval tval;
	tval.tv_sec = 3;
	tval.tv_usec = 0;
	bufferevent_set_timeouts(bev, &tval, NULL);      //ֻ���ö���3s��ʱ�������ʱ���򴥷�event_cb�ص�����

	bufferevent_setcb(bev, read_cb, write_cb, event_cb, base);
}


//bufferevent Ĭ�ϵĵ�/��ˮλ�����0
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

	//�ļ�������߶�����β
	if (len <= 0)
	{
		//�ر��ļ����
		fclose(fp);

		//�������������ᵼ�»�������û�з��ͽ���
		//bufferevent_free(be);
		bufferevent_disable(be,EV_WRITE);   //��ֹ������������д�¼�����Ϊ��������������꣬�����������д�¼�

		return;
	}

	sendCount += len;

	//����д��buffer
	bufferevent_write(be,data,len);

}

//���󡢳�ʱ�����ӶϿ��ᴥ������ص�����
static void client_event_cb(bufferevent* be, short what, void* arg)
{
	cout << "[CLIENT E]" << endl;

	//��ʱ�¼����������ݶ�ȡֹͣ
	if (what & BEV_EVENT_TIMEOUT && what & BEV_EVENT_READING)   //�ж��Ƿ�Ϊ����ʱ�¼�
	{
		cout << "BEV_EVENT_TIMEOUT" << endl;
		//bufferevent_enable(be,BEV_EVENT_READING);    //��ʱ������ʹ��

		bufferevent_free(be);   //�ͷŲ��ر�����BEV_OPT_CLOSE_ON_FREE�����Ǹ��������,����ͻ����ӳ���ռ����Դ
		return;
	}
	//��������
	else if (what & BEV_ERROR)   
	{
		cout << "BEV_ERROR" << endl;
		bufferevent_free(be);   //����������Ǵ�����Ҳ�رգ�����ͻ����ӳ���ռ����Դ
		return;
	}
	//���ӽ����¼�
	else if (what & BEV_EVENT_CONNECTED)   
	{
		cout << "connected success" << endl;

		bufferevent_trigger(be,EV_WRITE,0);    //����д�¼�

	}
	//����˵����ӹر��¼�
	else if (what & BEV_EVENT_EOF)
	{
		cout << "connection closed" << endl;
		bufferevent_free(be);   //�ر�����
	}
	//�����¼�	
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

	//�������������
	sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));   //ip��ַҲ��0
	addr.sin_family = AF_INET;
	addr.sin_port = htons(5001);

	//����һ��socket�����¼�����ִ��socket()��bind()��listen()����������һ�������¼�
	evconnlistener* ev = evconnlistener_new_bind(base,
		listen_cb,      //�ص�����
		base,           //�ص������Ĳ���args
		LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE,    //��ַ���ú�evconnlistener�ͷŵ�ʱ��رմ�������������׽���
		10,
		(sockaddr*)&addr,
		sizeof(addr));
	{
		//-1 �ڲ��Լ�����socket�����������洴����֮�󴫵ݸ���
		bufferevent* bev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);   //
		sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(5001);
		evutil_inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr.s_addr);

		//���ļ�
		FILE* fp = fopen("event_buffer_client.cpp","rb");   //�����ƶ�
		if (!fp)
		{
			cout << "open file event_buffer_client.cpp error" << endl;
			return;
		}

		//���ûص�����
		bufferevent_setcb(bev, client_read_cb, client_write_cb, client_event_cb, fp);    //fp�Ǵ���ص��Ĳ���
		bufferevent_enable(bev, EV_READ | EV_WRITE);   //�趨��дȨ��

		//��������
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
