#pragma once


#ifndef ROOM_HEADER
#define ROOM_HEADER

#include <set>
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
	set<User*>::iterator getUserIterator();
	set<User*>::iterator getIteratorEnd();

private:
	int m_RoomID;
	std::set<User*> m_UserList;
};

#endif 