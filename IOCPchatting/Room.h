#pragma once


#ifndef ROOM_HEADER
#define ROOM_HEADER

#include <list>
#include "User.h"

struct CHAT_NODE
{
	char*	data;
	int		len;
};


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

	void	addChat(CHAT_NODE& _chat);
	char*	getChat(int* _len);

private:
	int		m_RoomID;
	list<User*>		m_UserList;
	SOCKET*			m_SocketList;
	mutex			m_Mutex;
	list<CHAT_NODE*> m_Chat;
};

#endif 