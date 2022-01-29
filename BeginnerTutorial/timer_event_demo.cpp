#include <iostream>
#include <unistd.h>
#include <cstring>
#include <signal.h>
#include <arpa/inet.h>
#include "event2/event.h"
#include "event2/listener.h"
#include "event2/thread.h"

using namespace std;

//��ʱʱ�䣬1s��0us
static timeval t1 = {1,0};
static timeval t2 = {1,200000};
static timeval tv_in = {3,0};

//�ǳ־û�
//void timer1_cb(evutil_socket_t sock, short which, void* arg)
//{
//	cout << "timer1 arrival" << endl;
//
//	//�ǳ־�ʱ�����󣬻�ص�no pending״̬����Ҫ������ӣ����֮ǰ��ɾ��
//	event* ev = (event*)arg;
//	if (!evtimer_pending(ev,&t1))
//	{
//		evtimer_del(ev);
//		evtimer_add(ev,&t1);
//	}
//}

//�ǳ־û�
void timer1_cb(evutil_socket_t sock, short which, void* arg)
{
	cout << "timer1 arrival" << endl;

	//�ǳ־�ʱ�����󣬻�ص�no pending״̬����Ҫ������ӣ����֮ǰ��ɾ��
	event* ev = (event*)arg;
	if (!evtimer_pending(ev, NULL))
	{
		evtimer_del(ev);
		event_base* base = event_get_base(ev);
		event* ev1 = evtimer_new(base, timer1_cb, event_self_cbarg());
		timeval t1 = { 5,0 };
		evtimer_add(ev1, &t1);
	}
}

//�־û�
void timer2_cb(evutil_socket_t sock, short which, void* arg)
{
	cout << "timer2 arrival" << endl;
}

//ʹ��˫������滻������������Ż�
void timer3_cb(evutil_socket_t sock, short which, void* arg)
{
	cout << "timer3 arrival" << endl;
}


void timer_handler()
{
	//����������
	event_base* base = event_base_new();

	//evtimer_new Ҳ�Ƕ�event_new�ӿڵ����أ�������ʹ��
	// #define evtimer_new(b, cb, arg)	       event_new((b), -1, 0, (cb), (arg))
	//�����ev1�Ƿǳ־û�
	event* ev1 = evtimer_new(base,timer1_cb,event_self_cbarg());
	if (!ev1)
	{
		cout << "evtimer_new ev1 error" << endl;
		return;
	}
	evtimer_add(ev1,&t1);

	//�־û���ʱ��
	//event* ev2 = event_new(base,-1,EV_PERSIST,timer2_cb,NULL);
	//event_add(ev2,&t2);

	//��ʱ�������Ż���Ĭ��libeventʹ�õ��Ƕ���ѣ���С�ѡ����ѣ�������ɾ��O(logn)
	//�Ż���˫����У�����ɾ������O(1)
	//event* ev3 = event_new(base,-1,EV_PERSIST,timer3_cb,NULL);
	//const timeval* t3 = event_base_init_common_timeout(base,&tv_in);   //ʹ��˫������Ż�
	//event_add(ev3,t3);   //��������O(1)

	event_base_dispatch(base);

	event_free(ev1);
	event_base_free(base);
}