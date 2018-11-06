#include "stdafx.h"
#include "Packet.h"




PacketData::PacketData(char* _arr, int _len, int _refCount)
{
	m_arr = _arr;
	m_len = _len;
	m_refCount = _refCount;
}
PacketData::~PacketData()
{
	delete[]m_arr;
}
bool PacketData::dispose()
{
	std::lock_guard<std::mutex> lock(m_Mutex);
	m_refCount -= 1;

	if (m_refCount <= 0)
	{		
		return true;
	}
	return false;
}




Packet::Packet(
	SOCKET	 _clientSock,
	SOCKADDR_IN& _clientAddr,
	PacketData& _data) : m_Data(_data)
{
	m_Socket = _clientSock;
	memcpy(&m_ClientAddr, &_clientAddr, sizeof(SOCKADDR_IN));
}
Packet::Packet(
	SOCKET	 _clientSock,
	PacketData& _data) : m_Data(_data)
{
	m_Socket = _clientSock;
}


Packet::~Packet()
{
	if (m_Data.dispose())
	{
		delete &m_Data;
	}
		
}

char* Packet::getData()
{
	return m_Data.m_arr;
}
int	Packet::getDataLen()
{
	return m_Data.m_len;
}
SOCKET Packet::getSocket()
{
	return m_Socket;
}
SOCKADDR_IN& Packet::getClientAddr()
{
	return m_ClientAddr;
}

u_int64	Packet::getIPandPORT()
{
	u_int64 result;

	result = m_ClientAddr.sin_addr.s_addr;
	result = result << 32;
	result = result | m_ClientAddr.sin_port;

	return result;
}

void Packet::merge(Packet& _additionalPacket)
{

	char	*tempArr = m_Data.m_arr;
	int		newDataLen = m_Data.m_len + _additionalPacket.getDataLen();
	char	*newArr = new char[newDataLen];

	memcpy(newArr, m_Data.m_arr, m_Data.m_len);
	memcpy(&newArr[m_Data.m_len], _additionalPacket.getData(), _additionalPacket.getDataLen());

	m_Data.m_arr = newArr;
	m_Data.m_len = newDataLen;

	delete tempArr;
}


Packet&	Packet::cutInTwo(int _position)
{
	Packet		*cutPacket;

	char	*cutArr = new char[m_Data.m_len - _position];
	memcpy(cutArr, &m_Data.m_arr[_position], m_Data.m_len - _position);

	PacketData	*cutData = new PacketData(cutArr, m_Data.m_len - _position, 1);

	m_Data.m_len = _position;

	cutPacket = new Packet(m_Socket, m_ClientAddr, *cutData);

	return *cutPacket;
}

