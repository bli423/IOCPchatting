#pragma once


#ifndef PACKET_HEADER
#define PACKET_HEADER
#include <winsock2.h>
#include <mutex>
#include <list>

#define READ        4
#define WRITE       2

#define BUFSIZE     1024

#define C_CONNECT			9
#define C_CLOSE				11
#define C_ENTER_ROOM		13
#define C_EXIT_ROOM			14
#define C_SOCKET_ERROR		15
#define C_MESSAGE			4

#define BUFFER_WARING		50000


#define C_ROOMINFO			19

#define C_RECEIVE_ROOMINFO	20
#define C_REQUST_ROOMINFO	21

#define IN_LOBBY		0
#define IN_ROOM			1

class PacketData;
class Packet;


// 소켓 정보를 구조체화
struct CLIENT_DATA
{
	SOCKET      hClntSock;
	SOCKADDR_IN clntAddr;
};

// 소켓의 버퍼 정보를 구조체화
struct IO_DATA
{
	OVERLAPPED	overlapped;
	WSABUF		wsaBuf;
	Packet		*packet;
	int			rwMode;
	DWORD		len;
};



class PacketData
{
public:
	PacketData(char* _arr, int _len, int _refCount);
	~PacketData();

	bool		dispose();

	char		*m_arr;
	int			m_len;
	int			m_refCount;

	std::mutex			m_Mutex;
};



class Packet
{
public:
	Packet(
		SOCKET	 _clientSock,
		SOCKADDR_IN& _clientAddr,
		PacketData& _data);
	Packet(
		SOCKET	 _clientSock,
		PacketData& _data);
	~Packet();


	char*				getData();
	int					getDataLen();
	SOCKET				getSocket();
	SOCKADDR_IN&		getClientAddr();

	u_int64				getIPandPORT();
	void				merge(Packet& _additionalPacket);
	Packet&				cutInTwo(int _position);

	int					m_SendPoint;

private:

	SOCKET				m_Socket;
	SOCKADDR_IN			m_ClientAddr;
	PacketData			*m_Data;
	
};


#endif


#ifndef IOCP_CALLBACK_HEADER
#define IOCP_CALLBACK_HEADER

class IOCPCallback {
public:
	virtual void receivePacket(Packet& packet) = 0;
	virtual void socketClose(SOCKADDR_IN& _client) = 0;
};

#endif