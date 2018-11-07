#pragma once

#ifndef IOCPServer_H
#define IOCPServer_H

#pragma comment (lib,"ws2_32.lib")

#include <iostream>
#include <process.h>
#include <winsock2.h>
#include <queue>
#include <mutex>
#include <unordered_map>
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
	void sendData(SOCKET* _targetList, int _listSize, char* _data, int _dataSize);
	void sendData(SOCKET _target, char* _data, int _dataSize);

	int getSendQueueSize();
	int getBufferFilledQueSize();


	int getCount();
	void setCount();

private:
	HANDLE			m_IOCP;
	SOCKET			m_ServerSocket;
	SOCKADDR_IN		m_ServerAddr;


	IOCPCallback	&m_Callback;


	// 패킷을 전송하기전 대기하는 큐
	std::queue<Packet*>			m_SendQue;
	std::mutex					m_Mutex_SendQue;
	std::condition_variable		m_CV_SendQue;

	std::mutex					syn;
	std::mutex					readError;

	int c;
	int count[8];
};

#endif 