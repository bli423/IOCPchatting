#pragma comment (lib,"ws2_32.lib")

#include <iostream>
#include <string.h>
#include <process.h>
#include <winsock2.h>
#include <queue>
#include <mutex>

#ifndef IOCPServer_H
#define IOCPServer_H
class TaskOperation;

#define PORT        9999
#define BUFSIZE     1024
#define READ        4
#define WRITE       2
#define BACKLOG		20


struct PER_HANDLE_DATA       // 소켓 정보를 구조체화
{
	SOCKET      hClntSock;
	SOCKADDR_IN clntAddr;
};

struct PER_IO_DATA       // 소켓의 버퍼 정보를 구조체화
{
	OVERLAPPED overlapped;
	WSABUF     wsaBuf;
	int        rwMode;
};

struct DATA_INFO {
	char*	arr;
	int		reference_count;
};

struct PACKET_DATA
{
	SOCKET      hClntSock;
	SOCKADDR_IN clntAddr;
	PER_IO_DATA* perIoData;
	DATA_INFO* data;
	WORD dataLen;
};



#include "TaskOperation.h"

class IOCPServer {
public:
	IOCPServer(TaskOperation* taskOperation);
	~IOCPServer();

	void Run();
	bool Init();

	static unsigned int __stdcall m_WorkThread(void *p_this);
	UINT WINAPI work();

	void sendData(PACKET_DATA* packet);

private:
	HANDLE  m_IOCP;
	SOCKET m_ServerSocket;
	SOCKADDR_IN m_serverAddr;
	std::mutex m_Mutex;
	TaskOperation* taskOperation;

	DWORD sendFlag;
	DWORD sendSize;
	std::mutex mutex_send_queue;
	std::queue<PACKET_DATA*> send_queue;	

};



#endif 