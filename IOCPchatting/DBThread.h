#pragma once

#include "NetWorkStruct.h"
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


struct MESSAGELOG
{
	char* user_id[USERID_LEN];
	int room_id;
	u_int64 date;
	std::string* message;
};

struct REQUEST_DATA {
	char* user_id[USERID_LEN];
	std::string* data;
};

#ifndef DBThread_H
#define DBThread_H

class TaskOperation;
#include "TaskOperation.h"

class DBThread
{
private:
	TaskOperation* taskOperation;

	MYSQL* connectionRequest;
	MYSQL*	connectionWrite;
	MYSQL mysqlRequest;
	MYSQL mysqlRequestWrite;	

	time_t get_now_time();

	std::queue<REQUEST_DATA*> singRequestBuf;
	std::mutex mutex_singRequestBuf;
	std::condition_variable cv_singRequestBuf;

	std::queue<MESSAGELOG*> messageWriteBuf;
	std::mutex mutex_messageWriteBuf;
	std::condition_variable cv_messageWriteBuf;

	static unsigned int __stdcall singRequestThread(void* dbThread);
	void singRequestRun();

	static unsigned int __stdcall messageWriteThread(void* dbThread);
	void messageWriteRun();

public:
	DBThread(TaskOperation * taskOperation);
	~DBThread();

	void Run();

	void addMessage(char* user_id, int room_id, std::string message);
	void getRoomLog(char* reqest_user_id, int room_id);
	void getUserLog(char* reqest_user_id, char* user_id);
};

#endif