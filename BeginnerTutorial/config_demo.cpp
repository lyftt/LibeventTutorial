#include <iostream>
#include <unistd.h>
#include <cstring>
#include <signal.h>
#include <arpa/inet.h>
#include "event2/event.h"
#include "event2/listener.h"
#include "event2/thread.h"

using namespace std;

/*��ʼ������Libevent������*/
void config_libevent()
{
	//����������,���������������
	event_config* conf = event_config_new();

	//��ʾ֧�ֵ�����ģʽ
	const char ** methods =  event_get_supported_methods();
	cout << "support method" << endl;
	for (int i = 0; methods[i] != NULL; ++i)
	{
		cout << methods[i] << endl;    //��һ���ȼ���ӡ����
	}

	/*���������������ñ�Ե���������/ɾ���¼�/ȷ���¼������ʱ�临�Ӷ�ΪO(1)���Ƿ�֧�������ļ���������������ӹر��¼�*/
	//EV_FEATURE_FDS�������������⣬����ͬʱ����
	//Ĭ������£�Ĭ������EV_FEATURE_ET | EV_FEATURE_O1 | EV_FEATURE_EARLY_CLOSE�� Ĭ�ϲ�����EV_FEATURE_FDS
	//һ������EV_FEATURE_FDS��������3�����޷�������
	//EV_FEATURE_FDS ������֧��epoll���������EV_FEATURE_FDS������������ʹ��epoll����ģ�ͣ���ʱ�������ȼ���Ĭ��ʹ��poll�����ȼ�epoll>poll>select

	//event_config_require_features(conf,EV_FEATURE_FDS);
	//event_config_require_features(conf,EV_FEATURE_ET);

	//��������ģ�ͣ�ʹ��select
	event_config_avoid_method(conf,"epoll");       //����epoll
	event_config_avoid_method(conf,"poll");        //����poll

	
#ifdef _WIN32
	//�����windows�����ñ�־������IOCP��IOCP�ڲ�ʹ���̳߳أ�
	event_config_set_flag(conf,EVENT_BASE_FLAG_STARTUP_IOCP);
	evthread_use_windows_threads();           //���Ҫʹ��IOCP�������ʼ��IOCP���߳�
	SYSTEM_INFO si;               //����cpu����
	GetSystemInfo(&si);
	event_config_set_num_cpus_hint(conf,si.dwNumberOfProcessors);

#endif

	//������������libevent������
	event_base* base = event_base_new_with_config(conf);   //����ʧ�ܣ���Ϊ������Ϣ�����д�
	//�ͷ�
	event_config_free(conf);

	

	/*������������libevent������ʧ��*/
	if (!base)
	{
		cout << "event_base_new_with_config failed, try to use event_base_new " << endl;

		//��ʹ���ֶ����ã�ȫ��ʹ��Ĭ������
		base = event_base_new();
		if (!base)
		{
			cerr << "event_base_new failed" << endl;
			return;
		}
	}
	else
	{
		//��ȡ��ǰ����ģ�ͣ�select ��poll ��epoll��Щ��
		//linux��������������ã�Ĭ�ϵ�����ģ����epoll
		const char *cur_method = event_base_get_method(base);
		cout << "current network method:" <<cur_method << endl;

		cout << "event_base_new_with_config success" << endl;

		//��ȡĿǰ���õĵ�����(���û�����ã��Ǿ��ǻ�ȡĬ�ϵ�)
		//ET����ģʽ
		int feature = event_base_get_features(base);
		if (feature & EV_FEATURE_ET)
		{
			cout << "EV_FEATURE_ET mode are set" << endl;
		}
		else
		{
			cout << "EV_FEATURE_ET mode are not support" << endl;
		}

		//O(1)ʱ�临�Ӷ�����
		if (feature & EV_FEATURE_O1)
		{
			cout << "EV_FEATURE_O1 mode are set" << endl;
		}
		else
		{
			cout << "EV_FEATURE_O1 mode are not support" << endl;
		}

		//�����ļ�������֧��
		if (feature & EV_FEATURE_FDS)
		{
			cout << "EV_FEATURE_FDS mode are set" << endl;
		}
		else
		{
			cout << "EV_FEATURE_FDS mode are not support" << endl;
		}

		//������ӹر��¼�֧��
		if (feature & EV_FEATURE_EARLY_CLOSE)
		{
			cout << "EV_FEATURE_EARLY_CLOSE mode are set" << endl;
		}
		else
		{
			cout << "EV_FEATURE_EARLY_CLOSE mode are not support" << endl;
		}

		event_base_free(base);
	}

}