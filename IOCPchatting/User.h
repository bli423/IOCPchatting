#pragma once


#ifndef USER_HEADER
#define USER_HEADER

#include "Packet.h"
#include <string>
#include <mutex>

#define USERID_LEN	16
#define CONNECT_USER	1
#define DISCONNECT_USER 2

using namespace std;

class User
{
public:
	User(string& _id, SOCKET _socket,SOCKADDR_IN& _clientAddr);
	~User();

	string&				getID();
	SOCKET				getSocket();
	SOCKADDR_IN&		getClientAddr();
	int					getmRoomID();

	void				setRoomID(int _roomID);

	bool				isEqual(SOCKADDR_IN& _clientAddr);

	void				Connect();
	void				Disconnect();

	bool				canAccess();
	void				getAccess();
	void				returnAccess();
	
	bool				isRemove();

private:
	string				m_ID;
	SOCKET				m_Socket;
	SOCKADDR_IN			m_ClientAddr;
	int					m_RoomID;
	int					m_Status;
	int					m_refCount;

	mutex				m_Mutex;
};

#endif