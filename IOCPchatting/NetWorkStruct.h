#pragma once
#include <winsock2.h>

#define BUFSIZE     1024
#define USERID_LEN	16




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
	int		reference_count; //���� ī��Ʈ�� 0�̸� delete �����ϴ�.
};

// ��Ŷ ����ü
struct PACKET_DATA
{
	SOCKET      hClntSock;
	SOCKADDR_IN clntAddr;
	PER_IO_DATA* perIoData;
	DATA_INFO* data;
	WORD dataLen;
};

struct Data_Header {
	WORD totalLen;
	WORD protocol;
	char		 id[USERID_LEN];
};