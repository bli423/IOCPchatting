#pragma once

#ifndef IOCPServer_H
#define IOCPServer_H

#pragma comment (lib,"ws2_32.lib")

#include <iostream>
#include <process.h>
#include <winsock2.h>
#include <queue>
#include <mutex>
#include "Packet.h"

#define PORT        9999

#define BACKLOG		20



class IOCPServer {
public:
	IOCPServer(IOCPCallback& _callback);
	~IOCPServer();

	void Run();
	bool Init();

	static unsigned int __stdcall acceptThread(void* p_this);
	void acceptRun();

	static unsigned int __stdcall m_WorkThread(void *p_this);
	UINT WINAPI work();

	static unsigned int __stdcall m_SendThread(void *p_this);
	void sendWork();

	void sendData(Packet& packet);

private:
	HANDLE			m_IOCP;
	SOCKET			m_ServerSocket;
	SOCKADDR_IN		m_ServerAddr;

	

	IOCPCallback	&m_Callback;

	DWORD m_SendFlag;
	DWORD m_SendSize;

	// 패킷을 전송하기전 대기하는 큐
	std::queue<Packet*>		m_SendQue;
	std::mutex				m_Mutex_SendQue;
	std::condition_variable		m_CV_SendQue;

};

#endif 