//#include "HTcpServer.h"
#include "HTcpServer.h"
#include "event2/bufferevent.h"
#include "event2/listener.h"
#include <iostream>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MaxBufSize 65535

#define BUFFER_ONCE  10240

#define  OUPUT_ERRMSG(x)         do                                  \
								 {                                  \
									if (x) *x = m_ErrMsg;           \
								 }                                  \
								 while(0);   


#define  MEM_RELEASE_FREE(x)  do             \
							  {               \
								if(x)        \
								{            \
									free(x); \
									x = NULL;\
								}            \
							  }while (0);


#define  MEM_RELEASE_DELETE(x) do               \
							  {                 \
								if(x)          \
								{              \
									delete x;  \
									x = NULL;  \
								}              \
							  }while (0);


#define  MEM_RELEASE_DELETE_ARR(x)  do                   \
									{                    \
										if(x)            \
										{                \
											delete[] x;  \
											x = NULL;    \
										}                \
									}while (0);


int QueueBuffer::PushBack(unsigned char* buff, int size)
{
	if (!QueBuffer) return -1;

	return evbuffer_add(QueBuffer,buff,size);
}

int QueueBuffer::RemoveData(const int size)
{
	if (!QueBuffer) return -1;

	return evbuffer_drain(QueBuffer,size);    //ֱ�ӴӶ���ͷ���Ƴ�����
}

int QueueBuffer::GetAllBuffer(unsigned char*& buff, int& size)
{
	if (!QueBuffer)
	{
		buff = NULL;
		size = 0;
		return -1;
	}

	size = evbuffer_get_length(QueBuffer);
	if (0 == size)
	{
		buff = NULL;
		return size;
	}

	buff = new unsigned char[size];
	evbuffer_copyout(QueBuffer,buff,size);   //��evbuffer�и��Ƴ����ݣ������Ƴ�

	return size;
}

int QueueBuffer::GetAllBufferLen()
{
	if (!QueBuffer) return -1;

	return evbuffer_get_length(QueBuffer);
}


/**********************************************
* Tcp������
* 
* 
**********************************************/
//���ص�����
static void ReadCallBack(struct bufferevent* bev, void* ctx)
{
	int len = 0;
	int DataLen = 0;
	unsigned char* Data = NULL;
	unsigned char  buffer[BUFFER_ONCE] = { 0 };
	TcpTask* task = (TcpTask*)ctx;

	for (;;)
	{
		len = bufferevent_read(bev, buffer, sizeof(buffer));
		if (len <= 0) break;

		//����Tcp����Ļ�����
		task->PushBackToBuffer(buffer,len);
	}

	//ȡ����������
	task->GetAllData(Data,DataLen);

	//����������
	if (task->m_Handler)
	{
		int result = task->m_Handler(task, Data, DataLen);
		if (result > 0)
		{
			task->RemoveData(result);   //�Ƴ��Ѿ���ȷ����������
		}
	}
#ifdef DEBUG
	else std::cout << "no m_Handler was set" << std::endl;
#endif

	//�ͷ��ڴ�
	MEM_RELEASE_DELETE_ARR(Data);
}

static void WriteCallBack(struct bufferevent* bev, void* ctx)
{
	TcpTask* task = (TcpTask*)ctx;

	if (task->m_WriteHandler)
	{
		task->m_WriteHandler(task);
	}
#ifdef DEBUG
	else std::cout << "no m_WriteHandler was set" << std::endl;
#endif
}

//��ʱ���Ͽ����ӵ��쳣�¼��Ļص�����
static void EventCallBack(struct bufferevent* bev, short what, void* ctx)
{
	unsigned short port = 0;
	std::string ip;
	TcpTask* task = (TcpTask*)ctx;
	int sock = bufferevent_getfd(bev);
	
	GetPeerInfo(sock,ip,port);

	//�Է��ϵ���������������ղ���BEV_EVENT_EOF�¼�����ʱ��Ҫ�������߳�ʱ������
	if (what & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) 
	{
#ifdef DEBUG
		std::cout << "connection closed by ip:"<< ip << " port:" << port << std::endl;
#endif
		if (task->m_ClosedHandler)
		{
			task->m_ClosedHandler(task);
		}
#ifdef DEBUG
		else std::cout << "no m_ClosedHandler was set" << std::endl;
#endif
		
		bufferevent_free(bev); 
		MEM_RELEASE_DELETE(ctx);
	}
	//��ʱ�ж�
	else if (what & BEV_EVENT_TIMEOUT)
	{
#ifdef DEBUG
		std::cout << "timeout for ip:" << ip << " port:" << port << std::endl;
#endif
		if (task->m_TimeoutHandler)
		{
			task->m_TimeoutHandler(task);
		}
#ifdef DEBUG
		else std::cout << "no m_TimeoutHandler was set" << std::endl;
#endif
		
		bufferevent_free(bev);
		MEM_RELEASE_DELETE(ctx);
	}
}


int TcpTask::PushBackToBuffer(unsigned char* buffer, int size)
{
	int len = m_Buffer.PushBack(buffer,size);
	return len;
}

int TcpTask::SendData(unsigned char* data, int size)
{
	return bufferevent_write(m_BufferEv,data,size);
}

int TcpTask::GetPeerConnInfo()
{
	int ret = 0;
	char ipAddr[16] = { 0 };
	struct sockaddr_in clientAddrInfo;
	socklen_t addrLen = sizeof(clientAddrInfo);

	memset(&clientAddrInfo, 0, sizeof(clientAddrInfo));
	if (getpeername(m_Sock, (struct sockaddr*)&clientAddrInfo, &addrLen) < 0) return -1;
	if (inet_ntop(AF_INET, &clientAddrInfo.sin_addr, ipAddr, sizeof(ipAddr)) == NULL) return -1;

	m_PeerPort	= ntohs(clientAddrInfo.sin_port);
	m_PeerIp	= ipAddr;

	return 0;
}

std::string TcpTask::GetPeerIp()
{
	return m_PeerIp;
}

int TcpTask::GetPeerPort()
{
	return m_PeerPort;
}

void TcpTask::Init()
{
	std::string ip;
	unsigned short port = 0;
	GetPeerInfo(m_Sock,ip,port);
#ifdef DEBUG
	std::cout << "tcp connction task init for ip:" << ip << " port:" << port << std::endl;
#endif

	m_BufferEv = bufferevent_socket_new(m_Base, m_Sock, BEV_OPT_CLOSE_ON_FREE);   //����bufferevent����socket���м���
	bufferevent_setcb(m_BufferEv, ReadCallBack, WriteCallBack, EventCallBack, this);
	bufferevent_enable(m_BufferEv, EV_READ);

	if (m_Timeout.tv_sec != 0 || m_Timeout.tv_usec != 0)
	{
		timeval timeout = m_Timeout;   
		bufferevent_set_timeouts(m_BufferEv, &timeout, NULL);
	}
}

void TcpTask::Close()
{
	bufferevent_free(m_BufferEv);
	m_BufferEv = NULL;
}

/**********************************************
* �߳���
*
*
**********************************************/
static void NotifyCallBack(evutil_socket_t s, short which, void* arg)
{
	Thread* t = (Thread*)arg;
	t->Notify(s, which);
}

void Thread::Notify(evutil_socket_t s, short which)
{
	char buf[2] = { 0 };
	int ret = read(s, buf, 1);   //linux�йܵ�������recv��Ҫ��read
	if (ret <= 0) return;

	TcpTask* task = NULL;

	//��ȡ���񣬲���ʼ������
	m_TasksMutex.lock();
	if (m_Tasks.empty())
	{
		m_TasksMutex.unlock();
		return;
	}

	task = m_Tasks.front(); 
	m_Tasks.pop_front();
	m_TasksMutex.unlock();

	task->Init();   //�����ʼ��
}

void Thread::Activate()
{
	int ret = write(this->m_NotifySendFd, "c", 1);

#ifdef DEBUG
	if (ret <= 0)
	{
		std::cout << "Activate error" << std::endl;
	}
#endif
}

void Thread::AddTask(TcpTask* t)
{
	if (!t) return;

	t->m_Base = this->m_Base;
	m_TasksMutex.lock();
	m_Tasks.push_back(t);
	m_TasksMutex.unlock();
}

void* Thread::ThreadEntry(void* arg)
{
	Thread* Th = (Thread*)arg;
	Th->Main();
}

void Thread::Start()
{
	//��װ
	Setup();

	//�����߳�
	pthread_create(&m_Tid, NULL, Thread::ThreadEntry, this);
	pthread_detach(m_Tid);
}


void Thread::Main()
{
#ifdef DEBUG
	std::cout << m_Id << " thread start" << std::endl;
#endif
	event_base_dispatch(m_Base);

	event_base_free(m_Base);
#ifdef DEBUG
	std::cout << m_Id << " thread end" << std::endl;
#endif
}

bool Thread::Setup()
{
	int fds[2];
	if (pipe(fds)) //0ֻ�ܶ���1ֻ��д
	{
#ifdef DEBUG
		std::cout << "error for pipe" << std::endl;
#endif
		return false;
	}

	//��pipe�ϵĿɶ��¼����м�����д�ܵ���������Ҫ������
	m_NotifySendFd = fds[1];

	//����libevent������(����)
	event_config* ev_conf = event_config_new();
	event_config_set_flag(ev_conf, EVENT_BASE_FLAG_NOLOCK);
	this->m_Base = event_base_new_with_config(ev_conf);   //��������event_base
	event_config_free(ev_conf);
	if (!m_Base)
	{
#ifdef DEBUG
		std::cout << "event_base_new_with_config error" << std::endl;
#endif
		return false;
	}

	//���ӹܵ������¼������������߳�
	event* ev = event_new(m_Base, fds[0], EV_READ | EV_PERSIST, NotifyCallBack, this);   //�־��¼���ˮƽ����
	event_add(ev, NULL);

	return true;
}


/**********************************************
* �̳߳���
*
*
**********************************************/
void ThreadPool::Init(int ThreadCount)
{
	m_ThreadCount = ThreadCount;
	m_LastThread = -1;

	for (int i = 0; i < m_ThreadCount; ++i)
	{
		Thread* t = new Thread;
		t->SetId(i + 1);
		t->Start();
		m_Threads.push_back(t);
	}
}

void ThreadPool::Dispatch(TcpTask* task)
{
	if (!task) return;

	//��ѯ��ʽ�ַ�
	int tid = (m_LastThread + 1) % m_ThreadCount;
	m_LastThread = tid;
	Thread* t = m_Threads[tid];

	//���߳���������
	t->AddTask(task);

	//�����߳�
	t->Activate();
}

/**********************************************
* Tcp��������
*
*
**********************************************/
static void listen_cb(struct evconnlistener* e, evutil_socket_t s, struct sockaddr* a, int socklen, void* arg)
{
	HTcpServer* svr = (HTcpServer*)arg;
	TcpTask* task = new TcpTask();
	task->SetConnFd(s,svr->m_Handler,svr->m_WriteHandler,svr->m_ClosedHandler,svr->m_TimeoutHandler,svr->GetTimeout());  //������������socket
	svr->Dispatch(task);                                  //�ַ�
}

HTcpServer::HTcpServer() :m_Base(NULL), m_Listener(NULL), m_ThreadPool(NULL), m_Handler(NULL), m_WriteHandler(NULL), m_ClosedHandler(NULL), m_TimeoutHandler(NULL)
{

}

HTcpServer::~HTcpServer()
{
	if (m_Base)
	{
		event_base_free(m_Base);
	}

	if (m_Listener)
	{
		evconnlistener_free(m_Listener);
	}
}

int HTcpServer::Init(short Port, ProcessHandler Handler,WriteProcessHandler WriteHandler, ClosedProcessHandler ClosedHandler, TimeoutProcessHandler TimeoutHandler, const struct timeval& Timeout, const char** ErrMsg, int ThreadNums)
{
	//����libevent������
	m_Base = event_base_new();
	if (!m_Base)
	{
#ifdef DEBUG
		printf("event_base_new error\n");
#endif
		sprintf(m_ErrMsg,"event_base_new error");
		OUPUT_ERRMSG(ErrMsg);
		return -1;
	}

	//���ü�����ַ
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_port = htons(Port);
	addr.sin_family = AF_INET;

	//�����̳߳�
	if (ThreadNums > 0)
	{
		m_ThreadPool = ThreadPool::GetInstance();
		if (!m_ThreadPool)
		{
#ifdef DEBUG
			printf("thread pool init error\n");
#endif
			sprintf(m_ErrMsg, "thread pool init error");
			OUPUT_ERRMSG(ErrMsg);
			return -1;
		}
		m_ThreadPool->Init(ThreadNums);
	}

	//�����û���������
	m_Handler = Handler;
	m_WriteHandler = WriteHandler;
	m_ClosedHandler = ClosedHandler;
	m_TimeoutHandler = TimeoutHandler;

	//���ó�ʱʱ��
	m_Timeout = Timeout;

	//������������
	m_Listener = evconnlistener_new_bind(m_Base,
		listen_cb,                                       //���յ����ӵĻص�����
		this,                                            //�ص�������ȡ�Ĳ���
		LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE,       //��ַ���ã�m_Listener�ͷŵ�ͬʱ�ر�socket
		10,                                              //listen��������������Ӷ��еĴ�С
		(sockaddr*)&addr,                                //������ַ
		sizeof(addr)
	);
	if (!m_Listener)
	{
#ifdef DEBUG
		printf("evconnlistener_new_bind error\n");
#endif
		sprintf(m_ErrMsg, "evconnlistener_new_bind error");
		OUPUT_ERRMSG(ErrMsg);
		return -1;
	}

	return 0;
}

int HTcpServer::Init(short Port, int ThreadNums, ProcessHandler Handler, const struct timeval& Timeout)
{
	return Init(Port, Handler, NULL, NULL, NULL, Timeout, NULL, ThreadNums);
}

void HTcpServer::Run()
{
	event_base_dispatch(m_Base);
}

void HTcpServer::Dispatch(TcpTask* task)
{
	if(m_ThreadPool) m_ThreadPool->Dispatch(task);   //�̳߳ؽ�������ַ�
	else
	{
		//ֻʹ�����߳�
		task->m_Base = this->m_Base;  
		task->Init();
	}
}

/**********************************************
* ͨ�ú���
*
*
**********************************************/
int Send(TcpTask* task,unsigned char *data, int size)
{
	return task->SendData(data, size);
}

void Close(TcpTask* task)
{
	task->Close();
	MEM_RELEASE_DELETE(task);
}

int  GetPeerInfo(int sockfd, std::string& ip, unsigned short& port)
{
	int ret = 0;
	char ipAddr[16] = { 0 };
	struct sockaddr_in clientAddrInfo;
	socklen_t addrLen = sizeof(clientAddrInfo);

	memset(&clientAddrInfo, 0, sizeof(clientAddrInfo));
	if (getpeername(sockfd, (struct sockaddr*)&clientAddrInfo, &addrLen) < 0) return -1;
	if (inet_ntop(AF_INET, &clientAddrInfo.sin_addr, ipAddr, sizeof(ipAddr)) == NULL) return -1;

	port = ntohs(clientAddrInfo.sin_port);
	ip = ipAddr;

	return 0;
}