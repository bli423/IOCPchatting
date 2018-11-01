#include "stdafx.h"
#include "Room.h"




Room::Room(int _id)
{
	m_RoomID = _id;
	m_SocketList = new SOCKET[0];
}


Room::~Room()
{
}


void Room::insertUser(User& _user)
{	
	m_UserList.push_back(&_user);


	int size = m_UserList.size();
	delete m_SocketList;
	m_SocketList = new SOCKET[size];
	
	list<User*>::iterator itor = m_UserList.begin();
	list<User*>::iterator end = m_UserList.end();

	int n = 0;
	for (; itor != end; itor++)
	{
		m_SocketList[n] = (*itor)->getSocket();
		n++;
	}

}

void Room::deletetUser(User& _user)
{

	m_UserList.remove(&_user);

	int size = m_UserList.size();
	delete m_SocketList;
	m_SocketList = new SOCKET[size];

	list<User*>::iterator itor = m_UserList.begin();
	list<User*>::iterator end = m_UserList.end();

	int n = 0;
	for (; itor != end; itor++)
	{
		m_SocketList[n] = (*itor)->getSocket();
		n++;
	}
}

int	Room::getRoomID()
{
	return m_RoomID;
}
int	Room::getNumberOfUser()
{
	return m_UserList.size();
}

SOCKET*	Room::getSocketList()
{
	return m_SocketList;
}
