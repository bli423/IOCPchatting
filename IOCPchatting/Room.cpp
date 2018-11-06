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


void Room::addChat(CHAT_NODE& _chat)
{
	lock_guard<mutex> lock(m_Mutex);
	m_Chat.push_back(&_chat);
}
char* Room::getChat(int* _len)
{
	int len = 0;
	int size;

	CHAT_NODE**	list;

	if (m_Chat.size() > 0)
	{
		{
			lock_guard<mutex> lock(m_Mutex);
			size = m_Chat.size();
			list = new CHAT_NODE*[size];

			int n = 0;
			while (!m_Chat.empty()) {
				list[n] = m_Chat.front();
				m_Chat.pop_front();

				len += list[n]->len;
				n++;
			}
		}


		char* data = new char[len];
		int position_len = 0;
		for (int i = 0; i < size; i++)
		{
			memcpy(&data[position_len], list[i]->data, list[i]->len);

			position_len += list[i]->len;

			delete []list[i]->data;
			delete list[i];
		}

		delete []list;

		*_len = len;
		return data;
	}
	else
	{
		*_len = 0;
		return NULL;
	}
}