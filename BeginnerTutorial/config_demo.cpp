#include <iostream>
#include <unistd.h>
#include <cstring>
#include <signal.h>
#include <arpa/inet.h>
#include "event2/event.h"
#include "event2/listener.h"
#include "event2/thread.h"

using namespace std;

/*初始化配置Libevent上下文*/
void config_libevent()
{
	//配置上下文,先生成配置类对象
	event_config* conf = event_config_new();

	//显示支持的网络模式
	const char ** methods =  event_get_supported_methods();
	cout << "support method" << endl;
	for (int i = 0; methods[i] != NULL; ++i)
	{
		cout << methods[i] << endl;    //会一优先级打印出来
	}

	/*设置特征，即配置边缘触发、添加/删除事件/确定事件激活的时间复杂度为O(1)、是否支持任意文件描述符、检测连接关闭事件*/
	//EV_FEATURE_FDS和其他特征互斥，不能同时设置
	//默认情况下，默认设置EV_FEATURE_ET | EV_FEATURE_O1 | EV_FEATURE_EARLY_CLOSE， 默认不设置EV_FEATURE_FDS
	//一旦设置EV_FEATURE_FDS，则其他3个将无法被设置
	//EV_FEATURE_FDS 特征不支持epoll，如果设置EV_FEATURE_FDS特征，则不能再使用epoll网络模型，此时根据优先级会默认使用poll，优先级epoll>poll>select

	//event_config_require_features(conf,EV_FEATURE_FDS);
	//event_config_require_features(conf,EV_FEATURE_ET);

	//设置网络模型，使用select
	event_config_avoid_method(conf,"epoll");       //避免epoll
	event_config_avoid_method(conf,"poll");        //避免poll

	
#ifdef _WIN32
	//如果是windows，设置标志，开启IOCP（IOCP内部使用线程池）
	event_config_set_flag(conf,EVENT_BASE_FLAG_STARTUP_IOCP);
	evthread_use_windows_threads();           //如果要使用IOCP，必须初始化IOCP的线程
	SYSTEM_INFO si;               //设置cpu数量
	GetSystemInfo(&si);
	event_config_set_num_cpus_hint(conf,si.dwNumberOfProcessors);

#endif

	//根据配置生成libevent上下文
	event_base* base = event_base_new_with_config(conf);   //可能失败，因为配置信息可能有错
	//释放
	event_config_free(conf);

	

	/*根据配置生成libevent上下文失败*/
	if (!base)
	{
		cout << "event_base_new_with_config failed, try to use event_base_new " << endl;

		//不使用手动配置，全部使用默认配置
		base = event_base_new();
		if (!base)
		{
			cerr << "event_base_new failed" << endl;
			return;
		}
	}
	else
	{
		//获取当前网络模型（select 、poll 、epoll这些）
		//linux里如果不进行设置，默认的网络模型是epoll
		const char *cur_method = event_base_get_method(base);
		cout << "current network method:" <<cur_method << endl;

		cout << "event_base_new_with_config success" << endl;

		//获取目前设置的的特征(如果没有设置，那就是获取默认的)
		//ET触发模式
		int feature = event_base_get_features(base);
		if (feature & EV_FEATURE_ET)
		{
			cout << "EV_FEATURE_ET mode are set" << endl;
		}
		else
		{
			cout << "EV_FEATURE_ET mode are not support" << endl;
		}

		//O(1)时间复杂度设置
		if (feature & EV_FEATURE_O1)
		{
			cout << "EV_FEATURE_O1 mode are set" << endl;
		}
		else
		{
			cout << "EV_FEATURE_O1 mode are not support" << endl;
		}

		//任意文件描述符支持
		if (feature & EV_FEATURE_FDS)
		{
			cout << "EV_FEATURE_FDS mode are set" << endl;
		}
		else
		{
			cout << "EV_FEATURE_FDS mode are not support" << endl;
		}

		//检测连接关闭事件支持
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