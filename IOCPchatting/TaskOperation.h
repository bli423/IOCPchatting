
#include <iostream>
#include <string>
#include <mutex>
#include <queue>
#include <deque>
#include <map>
#include <list>
#include <utility>
#include <vector>


#ifndef TaskOperation_H
#define TaskOperation_H
class IOCPServer;

#include "IOCPServer.h"
#define USERID_LEN			16

#define C_CONNECT			9
#define C_CLOSE				11
#define C_ENTER_ROOM		13
#define C_EXIT_ROOM			14
#define C_SOCKET_ERROR		15
#define C_MESSAGE			4

#define BUFFER_WARING		50000


#define C_ROOMINFO	19


#define IN_LOBBY		0
#define IN_ROOM			1

#define ROOM_COUNT		100

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
struct Packet_Header {
	WORD totalLen;
	WORD protocol;
	char		 id[USERID_LEN];
};




class TaskOperation
{
private:
	IOCPServer * iocp;
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

	int room_counter;

	static unsigned int __stdcall receiveThread(void* taskOperation);
	void receiveRun();


	static unsigned int __stdcall controlThread(void* taskOperation);
	void controlRun();

	static unsigned int __stdcall dbThread(void* taskOperation);
	void dbRun();


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
	void Run();
};

#endif