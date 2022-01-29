//evhttp 上下文
//创建上下文     evhttp_new()
//释放上下文     evhttp_free()
//
//绑定端口       evhttp_bind_socket()   默认80端口
//
//设置回调函数   evhttp_set_gencb()
//
//
//分析请求       evhttp_request 结构体将作为回调函数的参数，里面包含了请求的URL、请求类型等信息
//  
//分析请求的配套函数   evhttp_request_get_evhttp_uri()        获取请求中的URL
//                     evhttp_request_get_command()           获取请求中的请求方法，GET\POST等
//                     evhttp_request_get_input_buffer()      获取POST请求的请求正文                    
//                     evhttp_request_get_input_headers()     获取请求头部
//                     
//                     evhttp_add_header()                    插入头部
//                     evhttp_request_get_output_headers()    获取响应的头部
//                     evhttp_send_reply()                    发送响应
//
//
//
//
//
//
//
//


#include <iostream>
#include <unistd.h>
#include <cstring>
#include <signal.h>
#include <string>
#include <arpa/inet.h>
#include <string>
#include "event2/event.h"
#include "event2/listener.h"
#include "event2/bufferevent.h"
#include "event2/bufferevent_struct.h"
#include "event2/buffer.h"
#include "event2/http.h"   
#include "event2/keyvalq_struct.h"
#define WEBROOT "."
#define DEFAULT "index.html"

using namespace std;

void http_cb(struct evhttp_request* request, void* arg)
{
	cout << "http call back" << endl;

	//1.获取浏览器的请求信息
	//uri
	const char* uri = evhttp_request_get_uri(request);
	cout << "uri:" << uri << endl;

	//请求类型
	string CmdType;
	switch (evhttp_request_get_command(request))
	{
		case EVHTTP_REQ_GET:
		{
			CmdType = "GET";
			break;
		}
		case EVHTTP_REQ_POST:
		{
			CmdType = "POST";
			break;
		}
		default:
		{
			break;
		}
	}
	cout << "CmdType:" << CmdType << endl;

	//请求头部
	evkeyvalq* headers = evhttp_request_get_input_headers(request);
	cout << "==========headers========" << endl;
	for (evkeyval* p = headers->tqh_first; p != NULL; p = p->next.tqe_next)
	{
		cout << p->key << ":" << p->value << endl;
	}

	//请求正文（GET方法为空,POST有表单信息）
	evbuffer* inbuf = evhttp_request_get_input_buffer(request);
	char buf[1024] = {0};
	int n = 0;
	cout << "==================input data================" << endl;
	while (evbuffer_get_length(inbuf))
	{
		n  = evbuffer_remove(inbuf, buf + n, sizeof(buf) - n - 1);
		if (n > 0)
		{
			buf[n] = '\0';
			cout << buf << endl;
		}
	}

	//2.回复浏览器响应
	//从uri分析请求的文件
	//设置根目录
	string filePath = WEBROOT;
	filePath += uri;
	if (strcmp(uri,"/") == 0)
	{
		filePath += DEFAULT;
	}

	//打开html文件
	FILE* fp = fopen(filePath.c_str(),"rb");
	if (!fp)
	{
		evhttp_send_reply(request, HTTP_NOTFOUND, "", 0);  //没有响应头部和响应体	
		return;
	}

	//响应头部，支持图片、js、css、下载zip文件
	int pos = filePath.rfind('.');
	evkeyvalq* outhead = evhttp_request_get_output_headers(request);
	string postfix = filePath.substr(pos + 1, filePath.size() - pos - 1);
	if (postfix == "jpg" || postfix == "gif" || postfix == "png")
	{
		string tmp = "image/" + postfix;
		evhttp_add_header(outhead, "Content-Type", tmp.c_str());
	}
	else if (postfix == "zip")
	{
		evhttp_add_header(outhead, "Content-Type", "application/zip");
	}
	else if (postfix == "html")
	{
		evhttp_add_header(outhead, "Content-Type", "text/html;charset=UTF8");   //也可以加上编码格式
		//evhttp_add_header(outhead, "Content-Type", "text/html");
	}
	else if (postfix == "css")
	{
		evhttp_add_header(outhead, "Content-Type", "text/css");
	}
	else if (postfix == "js")
	{
		evhttp_add_header(outhead, "Content-Type", "text/html");
	}

	//读物html文件返回，没有处理响应头部
	evbuffer* outbuf = evhttp_request_get_output_buffer(request); //放响应体内容
	memset(buf,0,sizeof(buf));
	for (;;)
	{
		int len = fread(buf,1,sizeof(buf),fp);
		if (len <= 0)
		{
			break;  //读完跳出
		}
		evbuffer_add(outbuf, buf, len);
	}
	
	fclose(fp);
	evhttp_send_reply(request, HTTP_OK, "", outbuf);
}

void evhttp_demo_starts()
{
	event_base* base = event_base_new();
	if (!base)
	{
		cout << "event base error" << endl;
		return;
	}

	//http服务器
	//1.创建evhttp上下文
	evhttp* evh = evhttp_new(base);

	//2.绑定端口和ip
	if (evhttp_bind_socket(evh, "0.0.0.0", 8080) != 0)
	{
		cout << "evhttp_bind_socket error" << endl;
		event_base_free(base);
		evhttp_free(evh);
		return;
	}

	//3.设定回调函数
	evhttp_set_gencb(evh,http_cb,NULL);

	event_base_dispatch(base);
	event_base_free(base);
	evhttp_free(evh);
}