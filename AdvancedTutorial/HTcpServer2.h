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
*供客户端使用
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
*供服务端使用
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
*互斥量封装
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
*队列缓冲，使用evbuffer实现
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

	struct evbuffer* QueBuffer;    //libevent自带自动扩容队列
};

/**********************************************
*Tcp任务对象
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
*线程对象
*
*
**********************************************/
class Thread
{
public:
	Thread() {}
	~Thread() {}

	void	SetId(const int id) { m_Id = id; }       //设置id
	bool	Setup();                                 //安装线程，初始化event_base和管道监听事件（用于激活线程）
	void	Start();                                 //启动线程
	void	Main();                                  //线程入口函数,使用std的线程库可以是成员函数
	void	Notify(evutil_socket_t s, short which);  //线程收到主线程发出的激活消息(线程池的分发)
	void	Activate();                              //线程激活，由其主线程调用，来激活这个工作线程
	void	AddTask(TcpTask*);                       //添加要处理的任务,一个线程可以同时处理多个任务,共用一个event_base，由主线程调用AddTask

private:
	static void* ThreadEntry(void* arg);

private:
	int                       m_Id;               //线程编号
	int                       m_NotifySendFd;     //用来激活工作线程的通道
	struct event_base*        m_Base;             //工作线程自己的libevent上下文
	std::list<TcpTask*>       m_Tasks;            //任务列表，工作线程需要处理的TCP连接对象
	Locker                    m_TasksMutex;       //互斥锁
	pthread_t                 m_Tid;              //线程pthread库的id
};

/**********************************************
*线程池对象
*
*
**********************************************/
class ThreadPool
{
public:
	static ThreadPool* GetInstance()
	{
		static ThreadPool _ThreadPool;    //C++0X 要求编译器保证内部静态变量的线程安全性
		return &_ThreadPool;
	}

	void   Init(int ThreadCount);        //初始化所有线程并启动线程
	void   Dispatch(TcpTask* task);      //分发给线程

private:
	ThreadPool() {} 
	~ThreadPool() {}

	int                      m_ThreadCount;    //线程数量
	int                      m_LastThread;     //用于分发
	std::vector<Thread*>     m_Threads;        //线程池
};

/**********************************************
*服务端
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
	void           Run();                            //Server开始事件循环
	void           Dispatch(TcpTask* task);          //分发任务
	struct timeval GetTimeout() { return m_Timeout; }

	ProcessHandler          m_Handler;          
	WriteProcessHandler     m_WriteHandler;
	ClosedProcessHandler    m_ClosedHandler;
	TimeoutProcessHandler   m_TimeoutHandler;

private:
	struct event_base*      m_Base;              //Server的libevent上下文
	struct evconnlistener*  m_Listener;          //监听对象
	ThreadPool*             m_ThreadPool;        //线程池
	struct timeval          m_Timeout;           //超时时间
	char                    m_ErrMsg[128];       //错误信息
};

/**********************************************
*客户端socket
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
	int  GetAllBuffer(unsigned char*& buff, int& size) { return m_QueueBuffer.GetAllBuffer(buff, size); } //获取所有读取到的字节，如果出现丢包/粘包现象，重新调用RecvMsg接口，并重新调用GetAllBuffer
	int  NotifyBufferIsComplete(MsgStatus msg_status, int len);//传入报文状态，如果报文完整，则传入MSG_IS_COMPLETE以及当前报文长度
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
	QueueBuffer		  m_QueueBuffer;	  //Client存储的Buffer队列
	char              m_ErrMsg[128];      //错误信息
};

#endif
