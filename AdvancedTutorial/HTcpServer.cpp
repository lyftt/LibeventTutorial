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

/**********************************************
*���ͻ���ʹ��
*
*
**********************************************/
#define  PER_REPORT_MAX_LENGTH    65536
#define  SELECT_TIMEOUT_S          5
#define  SELECT_TIMEOUT_US         0
#define  MaxBufSize  65535


/**********************************************
*�������ʹ��
*
*
**********************************************/
#define BUFFER_ONCE  10240

/**********************************************
*ͨ�ú�
*�������ڴ�
*
**********************************************/
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

/**********************************************
*�����λ�����
*
*
**********************************************/
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
	int result = task->m_Handler(task, Data, DataLen);
	if (result > 0)
	{
		task->RemoveData(result);   //�Ƴ��Ѿ���ȷ����������
	}

	//�ͷ��ڴ�
	MEM_RELEASE_DELETE_ARR(Data);
}

//��ʱ���Ͽ����ӵ��쳣�¼��Ļص�����
static void EventCallBack(struct bufferevent* bev, short what, void* ctx)
{
	unsigned short port = 0;
	std::string ip;
	int sock = bufferevent_getfd(bev);
	
	GetPeerInfo(sock,ip,port);

	//�Է��ϵ���������������ղ���BEV_EVENT_EOF�¼�����ʱ��Ҫ�������߳�ʱ������
	if (what & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) 
	{
		std::cout << "connection closed by ip:"<< ip << " port:" << port << std::endl;
		bufferevent_free(bev); 
		MEM_RELEASE_DELETE(ctx);
	}
	//��ʱ�ж�
	else if (what & BEV_EVENT_TIMEOUT)
	{
		std::cout << "timeout for ip:" << ip << " port:" << port << std::endl;
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

void TcpTask::Init()
{
	std::string ip;
	unsigned short port = 0;
	GetPeerInfo(m_Sock,ip,port);
	std::cout << "tcp connction task init for ip:" << ip << " port:" << port << std::endl;

	m_BufferEv = bufferevent_socket_new(m_Base, m_Sock, BEV_OPT_CLOSE_ON_FREE);   //����bufferevent����socket���м���
	bufferevent_setcb(m_BufferEv, ReadCallBack, 0, EventCallBack, this);
	bufferevent_enable(m_BufferEv, EV_READ | EV_WRITE);

	timeval timeout = m_Timeout;   //��ʱ
	bufferevent_set_timeouts(m_BufferEv, &timeout, NULL);
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

	if (ret <= 0)
	{
		std::cout << "Activate error" << std::endl;
	}
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
	std::cout << m_Id << " thread start" << std::endl;
	event_base_dispatch(m_Base);

	event_base_free(m_Base);
	std::cout << m_Id << " thread end" << std::endl;
}

bool Thread::Setup()
{
	int fds[2];
	if (pipe(fds)) //0ֻ�ܶ���1ֻ��д
	{
		std::cout << "error for pipe" << std::endl;
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
		std::cout << "event_base_new_with_config error" << std::endl;
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
	task->SetConnFd(s,svr->m_Handler,svr->GetTimeout());  //������������socket
	svr->Dispatch(task);                                  //�ַ�
}

HTcpServer::HTcpServer() :m_Base(NULL), m_Listener(NULL), m_ThreadPool(NULL)
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

int HTcpServer::Init(short Port, int ThreadNums, ProcessHandler Handler, const struct timeval& Timeout)
{
	//����libevent������
	m_Base = event_base_new();
	if (!m_Base) return -1;

	//���ü�����ַ
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_port = htons(Port);
	addr.sin_family = AF_INET;

	//�����̳߳�
	if (ThreadNums > 0)
	{
		m_ThreadPool = ThreadPool::GetInstance();
		if (!m_ThreadPool) return -1;
		m_ThreadPool->Init(ThreadNums);
	}

	//�����û���������
	m_Handler = Handler;

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
	if (!m_Listener) return -1;

	return 0;
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


/**********************************************
* Tcp�ͻ���socket
*
*
**********************************************/
int SetNonBlocking(int sockfd)
{
	int OldFlag = fcntl(sockfd, F_GETFL);
	int NewFlag = OldFlag | O_NONBLOCK;
	fcntl(sockfd, F_SETFL, NewFlag);
	return OldFlag;
}

CTcpClient::CTcpClient()
{
	m_nCommLinkStatus = LINK_NOT_INIT;
	m_nSockFd = INVALID_SOCKET;
}

CTcpClient::~CTcpClient()
{
	CloseRunSocket();
}

/*
*
* return  0   OK
* return -1   Error
*/
int CTcpClient::InitSocket(const char* ip, const short& port)
{
	if (strlen(ip) + 1 > sizeof(m_TcpPara.IpAddr))
	{
		printf("InitSocket error, ip is too long, can't initialize socket\n");
		return -1;
	}

	memset(&m_TcpPara, 0, sizeof(m_TcpPara));
	m_TcpPara.port = port;
	strncpy(m_TcpPara.IpAddr, ip, sizeof(m_TcpPara.IpAddr) - 1);

	if (TrySetupAndMaintainLink() < 0)
	{
		return -1;
	}

	return 0;
}

// return -1  Error
// return  0  Success
int CTcpClient::CreateClientSocket()
{
	m_nSockFd = socket(AF_INET, SOCK_STREAM, 0);
	if (m_nSockFd < 0)
	{
		printf("socket create error, err info: %s\n", strerror(errno));
		return -1;
	}

	m_nCommLinkStatus = LINK_NOT_INIT;

	return 0;
}

// return -1  Error
// return  0  Success
int CTcpClient::TrySetupAndMaintainLink()
{
	if (m_nCommLinkStatus == LINK_OCCUR_ERROR)
	{
		CloseRunSocket();
		m_nCommLinkStatus = LINK_NOT_INIT;   //�ص�δ��ʼ��״̬
	}

	if (m_nSockFd == INVALID_SOCKET)
	{
		if (CreateClientSocket() < 0) return -1;
	}

	if (m_nCommLinkStatus == LINK_NOT_INIT)
	{
		sockaddr_in SvrAddr;
		memset(&SvrAddr, 0, sizeof(SvrAddr));
		SvrAddr.sin_family = AF_INET;
		SvrAddr.sin_port = htons(m_TcpPara.port);
		inet_pton(AF_INET, m_TcpPara.IpAddr, &SvrAddr.sin_addr);
		if (ConnectNonb(m_nSockFd, (sockaddr*)&SvrAddr, sizeof(SvrAddr), 1) < 0)  //����������
		{
			printf("ConnectNonb error,the connection for sockfd:%d\n", m_nSockFd);
			return -1;
		}

		m_nCommLinkStatus = LINK_WORK_RIGHT;
		return 0;
	}

	return 0;
}

// return -1  Error
// return  0  Success
int CTcpClient::CloseRunSocket()
{
	if (m_nSockFd == INVALID_SOCKET)
	{
		printf("CloseRunSocket error, the socket fd is invalid,socket fd=%d\n", m_nSockFd);
		return -1;
	}

	close(m_nSockFd);
	m_nSockFd = INVALID_SOCKET;
	m_nCommLinkStatus = LINK_NOT_INIT;  //�ص�δ��ʼ��״̬

	return 0;
}

long getTimeUsec()
{
	struct timeval t;
	gettimeofday(&t, 0);
	return (long)((long)t.tv_sec * 1000 * 1000 + t.tv_usec);
}

// return -1  Error
// return  0  Success
int CTcpClient::ConnectNonb(int sock, const struct sockaddr* saptr, socklen_t salen, int nsec)
{
	int Oldflags = SetNonBlocking(sock);
	int RetCode;
	fd_set wset;
	struct timeval tval;
	FD_ZERO(&wset);

	if ((RetCode = connect(sock, saptr, salen)) < 0)
	{
		if (errno == EISCONN)
		{
			//m_nCommLinkStatus = LINK_WORK_RIGHT;
			printf("sockfd:%d is already connected, don't connect repeatedly\n", sock);
			return 0;
		}
		else if ((errno != EINPROGRESS) && (errno != EWOULDBLOCK))
		{
			printf("sockfd:%d connect error, errno=%s\n", sock, strerror(errno));
			return -1;
		}

		FD_SET(sock, &wset);
		tval.tv_sec = 0;
		tval.tv_usec = 1000000;
		long start_time = getTimeUsec();
		RetCode = select(sock + 1, NULL, &wset, NULL, nsec ? &tval : NULL);

		if (RetCode < 0)
		{
			printf("select error for sockfd:%d\n", sock);
			return -1;
		}
		else if (RetCode == 0)
		{
			printf("select return 0, now connection can not established for sockfd:%d\n", sock);
			return -1;
		}
		else
		{
			if (FD_ISSET(sock, &wset))
			{
				int error = 0;
				socklen_t len = sizeof(error);
				if (getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&error, (socklen_t*)&len) < 0)
				{
					printf("getsockopt return -1,connection error for sockfd:%d\n", sock);
					return -1;
				}

				if (error != 0)
				{
					printf("getsockopt get error,the value is not 0,connection for sockfd:%d is not established success\n", sock);
					return -1;
				}
			}
			else
			{
				printf("timeout for connection for sockfd:%d when select, connection is not established\n", sock);
				return -1;
			}
		}
	}

	printf("connection for sockfd:%d is established\n", sock);

	//�ָ�Ϊ����ģʽ
	//fcntl(sock,F_SETFL,Oldflags);

	return 0;
}

// return  -1  Error
// return   0  not data
// return   1  have data to read
int CTcpClient::ready_to_recv_data(int sockfd, int tmp_sec, int tmp_usec)
{
	int RetCode = 0;
	fd_set rset;
	timeval timeout;
	timeout.tv_sec = tmp_sec;
	timeout.tv_usec = tmp_usec;

	FD_ZERO(&rset);
	FD_SET(sockfd, &rset);

	RetCode = select(sockfd + 1, &rset, NULL, NULL, &timeout);
	if (RetCode < 0)
	{
		printf("select error for sockfd:%d when ready_to_recv_data, error info:%s\n", sockfd, strerror(errno));
		return -1;
	}
	else if (RetCode == 0)
	{
		printf("no data to read for sockfd:%d when ready_to_recv_data\n", sockfd);
		return 0;
	}

	if (!FD_ISSET(sockfd, &rset))
	{
		printf("no data to read for sockfd:%d when ready_to_recv_data\n", sockfd);
		return 0;
	}

	return 1;
}

// return  -1  Error
// return   0  not ready to send data
// return   1  ready to send data 
int CTcpClient::ready_to_send_data(int sockfd, int tmp_sec, int tmp_usec)
{
	int RetCode = 0;
	fd_set wset;
	timeval timeout;
	timeout.tv_sec = tmp_sec;
	timeout.tv_usec = tmp_usec;

	FD_ZERO(&wset);
	FD_SET(sockfd, &wset);

	RetCode = select(sockfd + 1, NULL, &wset, NULL, &timeout);
	if (RetCode < 0)
	{
		printf("select error for sockfd:%d when ready_to_send_data, error info:%s\n", sockfd, strerror(errno));
		return -1;
	}
	else if (RetCode == 0)
	{
		printf("can't send data for sockfd:%d when ready_to_send_data\n", sockfd);
		return 0;
	}

	if (!FD_ISSET(sockfd, &wset))
	{
		printf("can't send data for sockfd:%d, FD_ISSET not set valid\n", sockfd);
		return 0;
	}

	return 1;
}

// return  -1  Error
// return   0  Busy now
// return   1  Send success
int  CTcpClient::SendMsg(char* buf, int len)
{
	TrySetupAndMaintainLink();
	if (m_nCommLinkStatus != LINK_WORK_RIGHT)
	{
		printf("try to send msg, but link is not work right for sockfd:%d\n", m_nSockFd);
		return -1;
	}

	int ret = ready_to_send_data(m_nSockFd, SELECT_TIMEOUT_S, SELECT_TIMEOUT_US);   //2s
	if (ret < 0)
	{
		printf("ready_to_send_data return error for sockfd:%d\n", m_nSockFd);
		m_nCommLinkStatus = LINK_OCCUR_ERROR;
		return -1;
	}
	else if (0 == ret)
	{
		m_nCommLinkStatus = LINK_OCCUR_ERROR;   //?
		printf("now sockfd:%d is busy when ready_to_send_data\n", m_nSockFd);
		return 0;
	}

	ret = send(m_nSockFd, buf, len, 0);
	if (ret < 0)
	{
		printf("send error for sockfd:%d\n", m_nSockFd);
		m_nCommLinkStatus = LINK_OCCUR_ERROR;
		return -1;
	}
	else if (ret != len)
	{
		printf("send %d bytes with send(),but need to send %d bytes for sockfd:%d\n", ret, len, m_nSockFd);
	}
	return 1;
}

int  CTcpClient::RecvMsg()
{
	TrySetupAndMaintainLink();
	if (m_nCommLinkStatus != LINK_WORK_RIGHT)
	{
		printf("try to recv msg, but link is not LINK_WORK_RIGHT for sockfd:%d\n", m_nSockFd);
		return -1;
	}

	int ret = ready_to_recv_data(m_nSockFd, SELECT_TIMEOUT_S, SELECT_TIMEOUT_US);  //2s
	if (ret < 0)
	{
		m_nCommLinkStatus = LINK_OCCUR_ERROR;
		printf("ready_to_recv_data return error for sockfd:%d\n", m_nSockFd);
		return -1;
	}
	else if (0 == ret)
	{
		m_nCommLinkStatus = LINK_OCCUR_ERROR;   //?
		printf("no data to recv now for sockfd:%d\n", m_nSockFd);
		return 0;
	}

	char buf[MaxBufSize];
	memset(buf, 0, MaxBufSize);

	int len = recv(m_nSockFd, buf, PER_REPORT_MAX_LENGTH, 0);
	if (-1 == len)
	{
		printf("recv error when try to RecvMsg for sockfd:%d\n", m_nSockFd);
		m_nCommLinkStatus = LINK_OCCUR_ERROR;
		return -1;
	}
	else if (0 == len)
	{
		printf("socket close by peer for sockfd:%d\n", m_nSockFd);
		m_nCommLinkStatus = LINK_OCCUR_ERROR;
		return -1;
	}

	m_QueueBuffer.PushBack((unsigned char*)buf, len);
	return len;
}

int CTcpClient::NotifyBufferIsComplete(MsgStatus msg_status, int len)
{
	if (msg_status == MSG_IS_COMPLETE)
	{
		m_QueueBuffer.RemoveData(len);
	}
}
