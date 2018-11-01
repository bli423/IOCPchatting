#pragma once


#ifndef ROOM_HEADER
#define ROOM_HEADER

#include <list>
#include "User.h"

class Room
{
public:
	Room(int _id);
	~Room();

	void	insertUser(User& user);
	void	deletetUser(User& user);

	int		getRoomID();
	int		getNumberOfUser();	

	SOCKET*		getSocketList();

private:
	int m_RoomID;
	list<User*> m_UserList;

	SOCKET*		m_SocketList;
};

#endif 