#include <iostream>
#include <unistd.h>
#include <cstring>
#include <signal.h>
#include <arpa/inet.h>
#include "event2/event.h"
#include "event2/listener.h"

//extern void config_libevent();
//extern void listen_demo();
//extern void signal_handler();
extern void timer_handler();
//extern void file_read_handler();
//extern void echo_server();
//extern void event_buffer_demo();
//extern void event_buffer_client();
//extern void bufferevent_filter_demo();
//extern void bufferevent_filter_zlib();
//extern void evhttp_demo_starts();

int main()
{
	/*忽略管道信号,发送数据给已经关闭的socket会触发这个信号*/
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
	{
		return -1;
	}

	//config_libevent();
	//listen_demo();
	//signal_handler();
	timer_handler();
	//file_read_handler();
	//echo_server();
	//event_buffer_demo();
	//event_buffer_client();

	//bufferevent_filter_zlib();
	//evhttp_demo_starts();

	sleep(5);
	return 0;
}