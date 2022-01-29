#include <iostream>
#include <unistd.h>
#include <cstring>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "event2/event.h"
#include "event2/listener.h"
#include "event2/thread.h"
#include <thread>

using namespace std;

void read_file(evutil_socket_t fd, short event, void* arg)
{
	char buf[1024];
	memset(buf,0,sizeof(buf));

	int len = read(fd,buf,sizeof(buf) - 1);
	if (len > 0)   //读到数据
	{
		cout << "buf:" << buf << endl;
	}
	else
	{
		cout << "." << endl;
	}
}

void file_read_handler()
{
	//libevent的配置文件
	event_config* conf = event_config_new();

	//设置支持文件描述符
	//EV_FEATURE_ET | EV_FEATURE_O1 | EV_FEATURE_EARLY_CLOSE这些特征将不被支持
	//EV_FEATURE_FDS 特征也不支持epoll
	event_config_require_features(conf,EV_FEATURE_FDS);

	event_base* base = event_base_new_with_config(conf);
	if (!base)
	{
		cerr << "event_base_new_with_config error" << endl;
		return;
	}

	//打开文件描述符
	int fd = open("/home/windos/test.log",O_RDONLY | O_NONBLOCK,0);
	if (fd < 0)
	{
		cerr << "open error" << endl;
		return;
	}

	//移动到文件尾部,来监听是否有新的内容到达
	lseek(fd, 0, SEEK_END);

	//创建事件处理器
	event* fev = event_new(base,fd,EV_READ | EV_PERSIST,read_file,NULL);

	event_add(fev,NULL);

	event_base_dispatch(base);

	event_base_free(base);
}