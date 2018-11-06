#pragma once

#ifndef TaskOperation_H
#define TaskOperation_H

#include <iostream>
#include <string>
#include <mutex>
#include <queue>
#include <deque>
#include <map>
#include <list>
#include <utility>
#include "IOCPServer.h"
#include "UserTable.h"
#include "User.h"
#include "DBThread.h"


#define C_CONNECT			9
#define C_CLOSE				11
#define C_ENTER_ROOM		13
#define C_EXIT_ROOM			14
#define C_SOCKET_ERROR		15
#define C_MESSAGE			4

#define BUFFER_WARING		50000


#define C_ROOMINFO			19

#define C_RECEIVE_ROOMINFO	20
#define C_REQUST_ROOMINFO	21

#define IN_LOBBY		0
#define IN_ROOM			1

using namespace std;

class DBThread;

class TaskOperation : public IOCPCallback
{

public:
	TaskOperation();
	~TaskOperation();
		

	virtual void receivePacket(Packet& packet);
	virtual void socketClose(SOCKADDR_IN& _client);
	
	/*void receivePacket(Packet* _data);
	void receiveSocketError(SOCKET _socket, SOCKADDR_IN _addr);*/

	void Run();
	void Init(IOCPServer& _iocp);


	void completeDBJob(string& requst_id, char* data, int data_len);

	int getMessageBufSize();
	int getUserTableSize();

private:
	IOCPServer	*iocp;	   	
	DBThread	*dbThread;

	std::queue<Packet*>			m_ReceiveBuf;
	std::mutex					m_Mutex_ReceiveBuf;
	std::condition_variable		m_CV_ReceiveBuf;

	std::deque<Packet*>			m_MessageBuf;
	std::mutex					m_Mutex_MessageBuf;
	std::condition_variable		m_CV_MessageBuf;

	std::map<u_int64, Packet*>	m_PacketBuffer;
	
	UserTable		m_UserTable;

	static unsigned int __stdcall receiveThread(void* taskOperation);
	void receiveRun();

	static unsigned int __stdcall messageThread(void* taskOperation);
	void messageRun();

	static unsigned int __stdcall timerThread(void* taskOperation);
	void timerRun();
	
};

#endif





struct TotalLen_Protocol {
	WORD totalLen;
	WORD protocol;
};
struct Default_Header {
	WORD totalLen;
	WORD protocol;
	char id[USERID_LEN];
};

struct C_CONNECT_Header_Ask {
	WORD totalLen;
	WORD protocol;
	char id[USERID_LEN];
};
struct C_CONNECT_Header_Answer {
	WORD totalLen;
	WORD protocol;
	char id[USERID_LEN];
	bool canLogin;
	char empty;
};
struct C_CLOSE_Header_Ask {
	WORD totalLen;
	WORD protocol;
	char id[USERID_LEN];
};
struct C_CLOSE_Header_Answer {
	WORD totalLen;
	WORD protocol;
	char id[USERID_LEN];
	bool canLogout;
	char empty;
};

struct C_ENTER_ROOM_Header_Ask {
	WORD totalLen;
	WORD protocol;
	char id[USERID_LEN];
	WORD roomid;
};

struct C_ENTER_ROOM_Header_Answer {
	WORD totalLen;
	WORD protocol;
	char id[USERID_LEN];
	WORD roomid;
	bool canEnter;
	char empty;
};
struct C_ROOMINFO_Header {
	WORD totalLen;
	WORD protocol;
	char id[USERID_LEN];
	WORD count;
};
struct C_EXIT_ROOM_Header_Ask {
	WORD totalLen;
	WORD protocol;
	char id[USERID_LEN];
};
struct C_EXIT_ROOM_Header_Answer {
	WORD totalLen;
	WORD protocol;
	char id[USERID_LEN];
	bool canEexit;
	char empty;
};

struct C_SOCKET_ERROR_Header {
	WORD totalLen;
	WORD protocol;
	char id[USERID_LEN];
};


struct C_MESSAGE_Header {
	WORD totalLen;
	WORD protocol;
	char id[USERID_LEN];
	WORD messageLen;
};
struct C_REQUST_ROOMINFO_Header {
	WORD totalLen;
	WORD protocol;
	char id[USERID_LEN];
	WORD roomid;
};



struct C_RECEIVE_ROOMINFO_Header {
	WORD totalLen;
	WORD protocol;
	char id[USERID_LEN];
	WORD messageLen;
};
