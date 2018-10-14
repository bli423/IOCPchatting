#pragma comment (lib,"ws2_32.lib")

#include <iostream>
#include <string.h>
#include <process.h>
#include <winsock2.h>
#include <queue>
#include <mutex>
#include "NetWorkStruct.h"

#ifndef IOCPServer_H
#define IOCPServer_H
class TaskOperation;

#define PORT        9999
#define READ        4
#define WRITE       2
#define BACKLOG		20


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
	TaskOperation* taskOperation;

	DWORD sendFlag;
	DWORD sendSize;

	// 패킷을 전송하기전 대기하는 큐
	std::queue<PACKET_DATA*> send_queue;
	std::mutex mutex_send_queue;	

};

#endif 