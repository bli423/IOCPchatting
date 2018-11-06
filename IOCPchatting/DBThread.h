#pragma once




#ifndef DBThread_H
#define DBThread_H


#include "TaskOperation.h"

#include <WinSock2.h>
#include <iostream>
#include <string>
#include <mysql.h>
#include <queue>
#include <utility>
#include <process.h>
#include <mutex>
#include <chrono>


#pragma comment(lib,"libmysql.lib")
#pragma comment (lib,"ws2_32.lib")

using namespace std;

class TaskOperation;

struct MESSAGELOG
{
	string user_id;
	int room_id;
	u_int64 date;
	string message;
};

struct REQUEST_DATA {
	string* user_id;
	string* data;
};



class DBThread
{

public:
	DBThread();
	~DBThread();

	void Run();
	void Init(TaskOperation& taskOperation);

	void addMessage(string& _userID, int _roomID, string& message);
	void getRoomLog(string& _reqestUserID , int _roomID);


private:
	TaskOperation* m_TaskOperation;

	MYSQL*	m_ConnectionRequest;
	MYSQL*	m_ConnectionWrite;
	MYSQL	m_MysqlRequest;
	MYSQL	m_MysqlRequestWrite;

	time_t			getNowTime();

	queue<REQUEST_DATA*>	m_SingRequestBuf;
	mutex					m_Mutex_SingRequestBuf;
	condition_variable		m_CV_SingRequestBuf;

	queue<MESSAGELOG*>		m_MessageWriteBuf;
	mutex					m_Mutex_messageWriteBuf;
	condition_variable		m_CV_messageWriteBuf;

	static unsigned int __stdcall singRequestThread(void* dbThread);
	void singRequestRun();

	static unsigned int __stdcall messageWriteThread(void* dbThread);
	void messageWriteRun();


};

#endif