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

void UserTable::sendMessageToAllRoomUser(User& _user, char* _data, int _dataLen)
{
	unique_lock<mutex> lock(m_Mutex);
	int roomID = _user.getmRoomID();
	int refSize = m_RoomList[roomID]->getNumberOfUser();

	PacketData *data = new PacketData(_data, _dataLen, refSize);
	data->m_arr = _data;
	data->m_len = _dataLen;
	data->m_refCount = refSize;

/*	list<User*>::iterator itor = m_RoomList[roomID]->getUserIterator();
	list<User*>::iterator end = m_RoomList[roomID]->getIteratorEnd();

	for (;itor != end; itor++)
	{		
		Packet *packet = new Packet((*itor)->getSocket(), *data);
		m_IOCP->sendData(*packet);
	}*/
}


int UserTable::getUserListSize()
{
	return m_UserList.size();
}