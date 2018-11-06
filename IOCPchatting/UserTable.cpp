#include "stdafx.h"
#include "UserTable.h"


UserTable::UserTable()
{
	for(int i=0; i<100; i++)
	{
		m_RoomList[i] = new Room(i);
		
	}
}


UserTable::~UserTable()
{
}
void UserTable::Init(IOCPServer& _iocp)
{
	m_IOCP = &_iocp;
}


User* UserTable::getUserOrNULL(string& _id)
{
	unique_lock<mutex> lock(m_Mutex);
	map<string, User*>::iterator itor = m_UserList.find(_id);

	if (itor == m_UserList.end())
	{
		return NULL;
	}
	else 
	{
		User* user = (*itor).second;
		if (user->canAccess())
		{
			user->getAccess();
			return user;
		}
		else
		{
			return NULL;
		}
	}
}

User* UserTable::getUserOrNULL(SOCKADDR_IN& _clientAddr)
{
	unique_lock<mutex> lock(m_Mutex);
	map<string, User*>::iterator itor = m_UserList.begin();

	for (; itor != m_UserList.end(); itor++)
	{
		if ((*itor).second->isEqual(_clientAddr))
		{			
			User* user = (*itor).second;
			if (user->canAccess())
			{
				user->getAccess();
				return user;
			}
			else
			{
				break;
			}
		}
	}
	return NULL;
}

void UserTable::returnUser(User& _user)
{
	unique_lock<mutex> lock(m_Mutex);
	_user.returnAccess();
	if (_user.isRemove()) {
		m_UserList.erase(_user.getID());
		delete &_user;
	}
}

bool UserTable::addUser(User& _user)
{
	bool result;
	unique_lock<mutex> lock(m_Mutex);
	if (m_UserList.find(_user.getID()) == m_UserList.end())
	{
		result = true;
		m_UserList.insert(map<string, User*>::value_type(_user.getID(), &_user));		
		_user.Connect();
	}
	else
	{
		result = false;
	}
	return result;
}
bool UserTable::removeUser(User& _user)
{
	
	unique_lock<mutex> lock(m_Mutex);
	bool result;
	if (m_UserList.find(_user.getID()) != m_UserList.end())
	{
		result = true;
		_user.Disconnect();
		_user.returnAccess();		
		if (_user.isRemove())
		{
			m_UserList.erase(_user.getID());
			delete &_user;
		}
		
	}
	else
	{
		result = false;
	}
	return result;
}

bool UserTable::enterUserToRoom(User& _user, int _roomID)
{
	unique_lock<mutex> lock(m_Mutex);

	m_RoomList[_roomID]->insertUser(_user);
	_user.setRoomID(_roomID);

	return true;
}
bool UserTable::eixtUserToRoom(User& _user)
{
	unique_lock<mutex> lock(m_Mutex);

	int roomID = _user.getmRoomID();
	if (!(roomID < 0))
	{
		m_RoomList[roomID]->deletetUser(_user);
	}

	

	return true;
}

SOCKET* UserTable::getUserListInRoom(User& _user, int* _size)
{
	unique_lock<mutex> lock(m_Mutex);

	
	int roomID = _user.getmRoomID();
	*_size = m_RoomList[roomID]->getNumberOfUser();

	SOCKET* result = new SOCKET[*_size];
	memcpy(result, m_RoomList[roomID]->getSocketList(), sizeof(SOCKET)*(*_size));

	return  result;
}



int UserTable::getUserListSize()
{
	return m_UserList.size();
}


void UserTable::addChats(int _roomID, char* _data, int _len)
{
	CHAT_NODE *chat = new CHAT_NODE;
	chat->data = _data;
	chat->len = _len;

	m_RoomList[_roomID]->addChat(*chat);
}


void UserTable::sendChats()
{
	for (int i = 0; i < NUM_OF_ROOM; i++)
	{
		int len = 0;
		char* data = m_RoomList[i]->getChat(&len);
		if (len > 0)
		{
			int size = 0;
			SOCKET* socketList;
			{
				unique_lock<mutex> lock(m_Mutex);
				size = m_RoomList[i]->getNumberOfUser();

				socketList = new SOCKET[size];
				memcpy(socketList, m_RoomList[i]->getSocketList(), sizeof(SOCKET)*(size));
			}
			m_IOCP->sendData(socketList, size, data, len);

			delete []socketList;
		}
		else
		{
			delete data;
		}
	}
}