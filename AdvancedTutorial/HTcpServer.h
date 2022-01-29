#ifndef __H_TCP_SERVER_H__
#define __H_TCP_SERVER_H__

#include <vector>
#include <pthread.h>
#include <exception>
#include <list>
#include <string>
#include <time.h>
#include "event2/event.h"
#include "event2/buffer.h"

/**********************************************
*���ͻ���ʹ��
*
*
**********************************************/
#define  socket_t              int
#define  INVALID_SOCKET        -1
#define  SELECT_TIMEOUT_S          5
#define  SELECT_TIMEOUT_US         0

enum LinkStatus
{
	LINK_NOT_INIT,
	LINK_WORK_RIGHT,
	LINK_OCCUR_ERROR
};

enum MsgStatus
{
	MSG_IS_COMPLETE,
	MSG_IS_NOT_COMPLETE
};

/**********************************************
*�������ʹ��
*
*
**********************************************/
class Thread;
class TcpTask;
struct event_base;
typedef int (*ProcessHandler)(TcpTask* task, unsigned char* DataBuffer, int size);
typedef int (*WriteProcessHandler)(TcpTask* task);
typedef int (*ClosedProcessHandler)(TcpTask* task);
typedef int (*TimeoutProcessHandler)(TcpTask* task);
extern int  Send(TcpTask* task, unsigned char* data, int size); 
extern void Close(TcpTask* task);
extern int  GetPeerInfo(int sockfd, std::string& ip, unsigned short& port);

/**********************************************
*��������װ
*
*
**********************************************/
class Locker
{
public:
	Locker()
	{
		if (pthread_mutex_init(&m_mutex, NULL) != 0)   
		{
			throw std::exception();
		}
	}

	~Locker()
	{
		if (pthread_mutex_destroy(&m_mutex) != 0)   
		{
			throw std::exception();
		}
	}

	bool lock()
	{
		return pthread_mutex_lock(&m_mutex) == 0;  
	}

	bool unlock()
	{
		return pthread_mutex_unlock(&m_mutex) == 0;
	}

	pthread_mutex_t* get_mutex()
	{
		return &m_mutex;
	}

private:
	pthread_mutex_t m_mutex;  
};


/**********************************************
*���л��壬ʹ��evbufferʵ��
*
*
**********************************************/
struct QueueBuffer
{
	QueueBuffer() :QueBuffer(NULL) { QueBuffer = evbuffer_new(); }
	~QueueBuffer() { if (QueBuffer) evbuffer_free(QueBuffer); }

	int PushBack(unsigned char* buff, int size);
	int RemoveData(const int size);
	int GetAllBuffer(unsigned char* &buff, int &size);
	int GetAllBufferLen();

	struct evbuffer* QueBuffer;    //libevent�Դ��Զ����ݶ���
};

/**********************************************
*Tcp�������
*
*
**********************************************/
class TcpTask
{
public:
	TcpTask():m_Base(NULL),m_BufferEv(NULL){}
	~TcpTask() { }

	int  PushBackToBuffer(unsigned char* buffer, int size);
	int  GetAllData(unsigned char*& buff, int& size) { return m_Buffer.GetAllBuffer(buff,size); }
	int  RemoveData(const int size) { return m_Buffer.RemoveData(size); }
	//void SetConnFd(int Sock, ProcessHandler Handler, const struct timeval& Timeout) { m_Sock = Sock; m_Handler = Handler; m_Timeout = Timeout; }
	void SetConnFd(int Sock, ProcessHandler Handler, WriteProcessHandler WriteHandler, ClosedProcessHandler ClosedHandler, TimeoutProcessHandler TimeoutHandler, const struct timeval& Timeout) { m_Sock = Sock; m_Handler = Handler; m_WriteHandler = WriteHandler; m_ClosedHandler = ClosedHandler; m_TimeoutHandler = TimeoutHandler; m_Timeout = Timeout; }
	int  SendData(unsigned char* data, int size);
	int	 GetPeerConnInfo();
	std::string GetPeerIp();
	int  GetPeerPort();
	void Init();                            
	void Close();
	struct bufferevent* GetBufferEv() { return m_BufferEv; }

	struct event_base*    m_Base;
	ProcessHandler        m_Handler;
	WriteProcessHandler   m_WriteHandler;
	ClosedProcessHandler  m_ClosedHandler; 
	TimeoutProcessHandler m_TimeoutHandler;

private:
	int                  m_Sock;
	struct bufferevent*  m_BufferEv;
	int                  m_ThreadId;
	struct QueueBuffer   m_Buffer;
	struct timeval       m_Timeout;

	std::string			m_PeerIp;
	int					m_PeerPort;
};

/**********************************************
*�̶߳���
*
*
**********************************************/
class Thread
{
public:
	Thread() {}
	~Thread() {}

	void	SetId(const int id) { m_Id = id; }       //����id
	bool	Setup();                                 //��װ�̣߳���ʼ��event_base�͹ܵ������¼������ڼ����̣߳�
	void	Start();                                 //�����߳�
	void	Main();                                  //�߳���ں���,ʹ��std���߳̿�����ǳ�Ա����
	void	Notify(evutil_socket_t s, short which);  //�߳��յ����̷߳����ļ�����Ϣ(�̳߳صķַ�)
	void	Activate();                              //�̼߳���������̵߳��ã���������������߳�
	void	AddTask(TcpTask*);                       //���Ҫ���������,һ���߳̿���ͬʱ����������,����һ��event_base�������̵߳���AddTask

private:
	static void* ThreadEntry(void* arg);

private:
	int                       m_Id;               //�̱߳��
	int                       m_NotifySendFd;     //����������̵߳�ͨ��
	struct event_base*        m_Base;             //�����߳��Լ���libevent������
	std::list<TcpTask*>       m_Tasks;            //�����б������߳���Ҫ�����TCP���Ӷ���
	Locker                    m_TasksMutex;       //������
	pthread_t                 m_Tid;              //�߳�pthread���id
};

/**********************************************
*�̳߳ض���
*
*
**********************************************/
class ThreadPool
{
public:
	static ThreadPool* GetInstance()
	{
		static ThreadPool _ThreadPool;    //C++0X Ҫ���������֤�ڲ���̬�������̰߳�ȫ��
		return &_ThreadPool;
	}

	void   Init(int ThreadCount);        //��ʼ�������̲߳������߳�
	void   Dispatch(TcpTask* task);      //�ַ����߳�

private:
	ThreadPool() {} 
	~ThreadPool() {}

	int                      m_ThreadCount;    //�߳�����
	int                      m_LastThread;     //���ڷַ�
	std::vector<Thread*>     m_Threads;        //�̳߳�
};

/**********************************************
*�����
*
*
**********************************************/
class HTcpServer
{
public:
	HTcpServer();
	~HTcpServer();

	int            Init(short Port,int ThreadNums, ProcessHandler Handler,const struct timeval& Timeout); 
	int            Init(short Port, ProcessHandler Handler, WriteProcessHandler WriteHandler, ClosedProcessHandler ClosedHandler, TimeoutProcessHandler TimeoutHandler, const struct timeval& Timeout, const char** ErrMsg, int ThreadNums=3);
	void           Run();                            //Server��ʼ�¼�ѭ��
	void           Dispatch(TcpTask* task);          //�ַ�����
	struct timeval GetTimeout() { return m_Timeout; }

	ProcessHandler          m_Handler;          
	WriteProcessHandler     m_WriteHandler;
	ClosedProcessHandler    m_ClosedHandler;
	TimeoutProcessHandler   m_TimeoutHandler;

private:
	struct event_base*      m_Base;              //Server��libevent������
	struct evconnlistener*  m_Listener;          //��������
	ThreadPool*             m_ThreadPool;        //�̳߳�
	struct timeval          m_Timeout;           //��ʱʱ��
	char                    m_ErrMsg[128];       //������Ϣ
};

/**********************************************
*�ͻ���socket
*
*
**********************************************/
class CTcpClient
{
	typedef struct _Tcp_Parameter
	{
		char  IpAddr[16];
		short port;
	}TcpPara;

public:
	CTcpClient();
	~CTcpClient();

	int  Close();
	int  Close(const char** errmsg);
	int  InitSocket(const char* ip, const short& port);
	int  InitSocket(const char* ip, const short& port, const char** errmsg);
	int  GetClientFd() const { return m_nSockFd; }
	int	 SetClientFd(int Fd) { m_nSockFd = Fd; }
	int  GetLinkStatus() const { return m_nCommLinkStatus; }
	int  SendMsg(char* buf, int len);
	int  SendMsg(char* buf, int len, const char** errmsg, int tmp_sec = SELECT_TIMEOUT_S, int tmp_usec = SELECT_TIMEOUT_US);
	int  RecvMsg();
	int  RecvMsg(const char** errmsg, int tmp_sec = SELECT_TIMEOUT_S, int tmp_usec = SELECT_TIMEOUT_US);
	int  GetAllBuffer(unsigned char*& buff, int& size) { return m_QueueBuffer.GetAllBuffer(buff, size); } //��ȡ���ж�ȡ�����ֽڣ�������ֶ���/ճ���������µ���RecvMsg�ӿڣ������µ���GetAllBuffer
	int  NotifyBufferIsComplete(MsgStatus msg_status, int len);//���뱨��״̬�������������������MSG_IS_COMPLETE�Լ���ǰ���ĳ���
	char* GetIp() { return m_TcpPara.IpAddr; }
	short GetPort() { return m_TcpPara.port; }

private:
	int  CreateClientSocket();
	int  ConnectNonb(int sock, const struct sockaddr* saptr, socklen_t salen, int nsec);
	int  CloseRunSocket();
	int  TrySetupAndMaintainLink();

	int  ready_to_recv_data(int sockfd, int tmp_sec, int tmp_usec);
	int  ready_to_send_data(int sockfd, int tmp_sec, int tmp_usec);

private:
	socket_t		  m_nSockFd;
	LinkStatus		  m_nCommLinkStatus;
	TcpPara			  m_TcpPara;
	QueueBuffer		  m_QueueBuffer;	  //Client�洢��Buffer����
	char              m_ErrMsg[128];      //������Ϣ
};

#endif
