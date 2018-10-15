#pragma once
#include <winsock2.h>

#define BUFSIZE     1024
#define USERID_LEN	16

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

#define ROOM_COUNT		100


// ���� ������ ����üȭ
struct PER_HANDLE_DATA
{
	SOCKET      hClntSock;
	SOCKADDR_IN clntAddr;
};

// ������ ���� ������ ����üȭ
struct PER_IO_DATA
{
	OVERLAPPED overlapped;
	WSABUF     wsaBuf;
	int        rwMode;
};

// ���� ������ ����ü
struct DATA_INFO {
	char*	arr;
	WORD dataLen;
	int		reference_count; //���� ī��Ʈ�� 0�̸� delete �����ϴ�.
};

// ��Ŷ ����ü
struct PACKET_DATA
{
	SOCKET      hClntSock;
	SOCKADDR_IN clntAddr;
	PER_IO_DATA* perIoData;
	DATA_INFO* data;
};

struct Data_Header {
	WORD totalLen;
	WORD protocol;
	char		 id[USERID_LEN];
};
