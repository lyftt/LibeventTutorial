//evhttp ������
//����������     evhttp_new()
//�ͷ�������     evhttp_free()
//
//�󶨶˿�       evhttp_bind_socket()   Ĭ��80�˿�
//
//���ûص�����   evhttp_set_gencb()
//
//
//��������       evhttp_request �ṹ�彫��Ϊ�ص������Ĳ�������������������URL���������͵���Ϣ
//  
//������������׺���   evhttp_request_get_evhttp_uri()        ��ȡ�����е�URL
//                     evhttp_request_get_command()           ��ȡ�����е����󷽷���GET\POST��
//                     evhttp_request_get_input_buffer()      ��ȡPOST�������������                    
//                     evhttp_request_get_input_headers()     ��ȡ����ͷ��
//                     
//                     evhttp_add_header()                    ����ͷ��
//                     evhttp_request_get_output_headers()    ��ȡ��Ӧ��ͷ��
//                     evhttp_send_reply()                    ������Ӧ
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

	//1.��ȡ�������������Ϣ
	//uri
	const char* uri = evhttp_request_get_uri(request);
	cout << "uri:" << uri << endl;

	//��������
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

	//����ͷ��
	evkeyvalq* headers = evhttp_request_get_input_headers(request);
	cout << "==========headers========" << endl;
	for (evkeyval* p = headers->tqh_first; p != NULL; p = p->next.tqe_next)
	{
		cout << p->key << ":" << p->value << endl;
	}

	//�������ģ�GET����Ϊ��,POST�б���Ϣ��
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

	//2.�ظ��������Ӧ
	//��uri����������ļ�
	//���ø�Ŀ¼
	string filePath = WEBROOT;
	filePath += uri;
	if (strcmp(uri,"/") == 0)
	{
		filePath += DEFAULT;
	}

	//��html�ļ�
	FILE* fp = fopen(filePath.c_str(),"rb");
	if (!fp)
	{
		evhttp_send_reply(request, HTTP_NOTFOUND, "", 0);  //û����Ӧͷ������Ӧ��	
		return;
	}

	//��Ӧͷ����֧��ͼƬ��js��css������zip�ļ�
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
		evhttp_add_header(outhead, "Content-Type", "text/html;charset=UTF8");   //Ҳ���Լ��ϱ����ʽ
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

	//����html�ļ����أ�û�д�����Ӧͷ��
	evbuffer* outbuf = evhttp_request_get_output_buffer(request); //����Ӧ������
	memset(buf,0,sizeof(buf));
	for (;;)
	{
		int len = fread(buf,1,sizeof(buf),fp);
		if (len <= 0)
		{
			break;  //��������
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

	//http������
	//1.����evhttp������
	evhttp* evh = evhttp_new(base);

	//2.�󶨶˿ں�ip
	if (evhttp_bind_socket(evh, "0.0.0.0", 8080) != 0)
	{
		cout << "evhttp_bind_socket error" << endl;
		event_base_free(base);
		evhttp_free(evh);
		return;
	}

	//3.�趨�ص�����
	evhttp_set_gencb(evh,http_cb,NULL);

	event_base_dispatch(base);
	event_base_free(base);
	evhttp_free(evh);
}