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

void UserTable::sendMessageToAllRoomUser(User& _user, char* _data, int _dataLen)
{
	unique_lock<mutex> lock(m_Mutex);
	int roomID = _user.getmRoomID();
	int refSize = m_RoomList[roomID]->getNumberOfUser();

	DATA *data = new DATA;
	data->m_arr = _data;
	data->m_len = _dataLen;
	data->m_refCount = refSize;

	set<User*>::iterator itor = m_RoomList[roomID]->getUserIterator();

	for (; itor != m_RoomList[roomID]->getIteratorEnd(); itor++)
	{		
		Packet *packet = new Packet((*itor)->getSocket(), (*itor)->getClientAddr(), *data);
		m_IOCP->sendData(*packet);
	}

}
void UserTable::sendMessageToUser(User& _user, char* _data, int _dataLen)
{
	lock_guard<mutex> lock(m_Mutex);

	DATA *data = new DATA;
	data->m_arr = _data;
	data->m_len = _dataLen;
	data->m_refCount = 1;

	Packet *packet = new Packet(_user.getSocket(), _user.getClientAddr(), *data);
	m_IOCP->sendData(*packet);
}