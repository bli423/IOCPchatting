#include "stdafx.h"
#include "Room.h"




Room::Room(int _id)
{
	m_RoomID = _id;
}


Room::~Room()
{
}


void Room::insertUser(User& _user)
{
	m_UserList.insert(&_user);
}

void Room::deletetUser(User& _user)
{
	m_UserList.erase(&_user);
}

int	Room::getRoomID()
{
	return m_RoomID;
}
int	Room::getNumberOfUser()
{
	return m_UserList.size();
}

set<User*>::iterator Room::getUserIterator()
{
	return m_UserList.begin();

	
}

set<User*>::iterator Room::getIteratorEnd()
{
	return m_UserList.end();
}