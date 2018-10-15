#pragma once

#include <iostream>
#include <string>
#include <mutex>
#include <queue>
#include <deque>
#include <map>
#include <list>
#include <utility>

#include "NetWorkStruct.h"


#ifndef TaskOperation_H
#define TaskOperation_H
class IOCPServer;
class DBThread;

#include "IOCPServer.h"
#include "DBThread.h"

struct PACKET_DATA;

struct mUSER {
	char		 id[USERID_LEN];
	SOCKET       hClntSock;
	SOCKADDR_IN  addr;
	PER_IO_DATA* perIoData;
	int			 roomId;
	char		 status;
};

struct mROOM {
	int rommId;
	std::list<mUSER*> userList;

};



class TaskOperation
{
private:
	IOCPServer* iocp;
	DBThread* dbThread;

	std::mutex m_Mutex;

	std::queue<PACKET_DATA*> receiveBuf;
	std::mutex mutex_receiveBuf;
	std::condition_variable cv_receiveBuf;

	std::deque<PACKET_DATA*> messageBuf;
	std::mutex mutex_messageBuf;
	std::condition_variable cv_messageBuf;

	std::map<_int64, PACKET_DATA*> packetBuffer;
	std::map<std::string, mUSER*> userList;
	std::map<WORD, mROOM> roomList;

	static unsigned int __stdcall receiveThread(void* taskOperation);
	void receiveRun();


	static unsigned int __stdcall controlThread(void* taskOperation);
	void controlRun();


	void headerSet(char* data, WORD totalLen, WORD protocol, char* id);
	PACKET_DATA* makePacket(SOCKET hClntSock, PER_IO_DATA* perIoData, DATA_INFO* data, WORD dataLen);

public:
	TaskOperation();
	~TaskOperation();


	mROOM room[ROOM_COUNT];

	mUSER* findUserId(SOCKADDR_IN addr);
	mUSER* findUser(std::string id);
	mROOM* findRoom(WORD id);
	mUSER* registeUser(std::string id, SOCKET hClntSock, SOCKADDR_IN addr, PER_IO_DATA* perIoData);

	bool removeUser(mUSER* user);
	bool eixtUser(mUSER* user);
	bool addRoomUser(mUSER* user, WORD roomid);

	void receivePacketClear(PACKET_DATA*  packet);
	void sendPacketClear(PACKET_DATA*  packet);
	void setIOCP(IOCPServer* iocp);
	void receivePacket(PACKET_DATA* data);
	void sendClearJob(char* requst_id,char* data, DWORD data_len) ;
	void Run();
};

#endif