/*
* 
*          invalid ptr
*              *
*              * event_new()
*              *
*             \|/
*          non-pending
*             /|\
*              *
*   event_del()* event_add()
*              *
*             \|/
*            pending
*             /|\
*              *
*              * 
*              *
*             \|/
*            active
* 
* 
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
#include "event2/thread.h"

using namespace std;

/*
* 
* sock �ļ�������
* which �¼����ͣ����糬ʱ���źš���д
* arg ����
* 
*/
static void Ctrl_C_cb(int sock,short which,void *arg)
{
	cout << "Ctrl-C" << endl;
	event_base* base = (event_base*)arg;
	//event_base_loopbreak(base);   //ִ���굱ǰ������¼������������˳�
	event_base_loopexit(base,NULL); //ִ���굱ǰ�����л�¼��Ĵ�����֮����˳�,��������һ����ʱʱ�䣨����������ô���ʱ�䣩; ����¼�ѭ����û�п�ʼ���е�ʱ��ҲҪ������һ�����˳�
}

static void Kill(int sock, short which, void* arg)
{
	cout << "Kill" << endl;
	event* ev = (event *)arg;

	//�����Ƿǳ־��¼������뵽�ص�����֮�󣬾���no pending״̬��
	//if(evsignal_pending(ev,NULL)),����Ҳ����
	if (!event_pending(ev,EV_SIGNAL,NULL))
	{
		event_del(ev);
		event_add(ev,NULL);
	}
}

void signal_handler()
{
	event_base* base = event_base_new();

	//libeventͨ��һЩ�������ؽӿ�,evsignal_new���ʾ��ǵ���event_new
	//evsignal_new����no pending״̬
	event* csignal = evsignal_new(base,SIGINT, Ctrl_C_cb,base);
	if (!csignal)
	{
		cout << "SIGINT evsignal_new error" << endl;
		return;
	}

	//����¼����� pending
	if (event_add(csignal,0) != 0)
	{
		cout << "SIGINT evsignal_new error" << endl;
		return;
	}

	//��ʹ��evsignal_new��
	//�ǳ־��¼�,ֻ�����һ��
	//event_self_cbarg���ݵ�ǰ��event,��Ϊ��ʱksignal��û��ʼ����
	event* ksignal = event_new(base,SIGTERM,EV_SIGNAL,Kill,event_self_cbarg());
	if (!ksignal)
	{
		cout << "SIGTERM event_new error" << endl;
		return;
	}

	if (event_add(ksignal, 0) != 0)
	{
		cout << "SIGTERM evsignal_new error" << endl;
		return;
	}

	//��ʼ�¼�ѭ��
	//event_base_dispatch(base);            //�ȼ���event_base_loop(base,0),����ʹ���κ�ѡ���� �����¼�ѭ����ʱ��û��ע���κ��¼�����Ҳ����������,���������¼����������������������
	//event_base_loop(base,EVLOOP_ONCE);    //�ȴ�һ���¼����У�Ȼ���������л�¼�ֱ��û�л�¼��󣬲��˳�
	/*
	bool isexit = false;
	while (!isexit)
	{
		event_base_loop(base, EVLOOP_NONBLOCK);  //�л�¼��ɴ���û�о��������أ���������ģʽ��Ĭ���������ģ�
	}*/
	event_base_loop(base,EVLOOP_NO_EXIT_ON_EMPTY);  //��ʹû���¼�ע�ᵽbase��Ҳ�����أ�������ģʽ,�����ں��ڶ��̵߳�ʱ��̬���
	

	event_free(csignal);
	event_base_free(base);
}



