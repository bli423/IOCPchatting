#include "stdafx.h"
#include "User.h"


User::User(string& _id, SOCKET _socket, SOCKADDR_IN& _clientAddr)
{
	m_ID.append(_id);
	m_Socket = _socket;
	memcpy(&m_ClientAddr, &_clientAddr, sizeof(IO_DATA));

	m_RoomID = -1;
	m_Status = DISCONNECT_USER;
	m_refCount = 0;
}


User::~User()
{
}


bool User::isEqual(SOCKADDR_IN& _clientAddr)
{
	if (m_ClientAddr.sin_port == _clientAddr.sin_port
		&& m_ClientAddr.sin_addr.s_addr == _clientAddr.sin_addr.s_addr)
	{
		return true;
	}
	return false;
}


string& User::getID()
{
	return m_ID;
}

SOCKET User::getSocket()
{
	return m_Socket;
}

SOCKADDR_IN& User::getClientAddr()
{
	return m_ClientAddr;
}

int	User::getmRoomID()
{
	return m_RoomID;
}


void User::setRoomID(int _roomID)
{
	m_RoomID = _roomID;
}

void User::Connect()
{
	m_Status = CONNECT_USER;
}
void User::Disconnect()
{
	m_Status = DISCONNECT_USER;
}
bool User::canAccess()
{
	if (m_Status == CONNECT_USER)
	{
		return true;
	}		
	else
	{
		return false;
	}
}
void User::getAccess()
{
	if (m_Status == CONNECT_USER)
	{
		m_refCount += 1;
	}
	
}
void User::returnAccess()
{
	m_refCount -= 1;
}

bool User::isRemove()
{
	if (m_Status == DISCONNECT_USER && m_refCount <= 0)
	{
		return true;
	}
	else
	{
		return false;
	}
}