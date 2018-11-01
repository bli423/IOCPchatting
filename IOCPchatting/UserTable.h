#pragma once

#ifndef USERTABLE_HEADER
#define USERTABLE_HEADER

#include "Packet.h"
#include "User.h"
#include "Room.h"
#include "IOCPServer.h"
#include <string>
#include <map>

#define ROOM_COUNT		100

using namespace std;

class UserTable
{
public:
	UserTable();
	~UserTable();

	void Init(IOCPServer& iocp);

	User*	getUserOrNULL(string& _id);
	User*	getUserOrNULL(SOCKADDR_IN& _clientAddr);
	void	returnUser(User& _user);


	bool addUser(User& _user);
	bool removeUser(User& _user);

	bool enterUserToRoom(User& user,int _roomID);
	bool eixtUserToRoom(User& _user);

	SOCKET* getUserListInRoom(User& _user, int* _size);

	void sendMessageToAllRoomUser(User& _user,char* _data, int _dataLen);
	//void sendMessageToUser(User& _user, char* _data, int _dataLen);

	int getUserListSize();

private:
	IOCPServer				*m_IOCP;

	map<string, User*>		m_UserList;
	Room					*m_RoomList[100];

	mutex					m_Mutex;
};

#endif 