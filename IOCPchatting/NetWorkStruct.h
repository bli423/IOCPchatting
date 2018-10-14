#pragma once
#include <winsock2.h>

#define BUFSIZE     1024
#define USERID_LEN	16




// 소켓 정보를 구조체화
struct PER_HANDLE_DATA
{
	SOCKET      hClntSock;
	SOCKADDR_IN clntAddr;
};

// 소켓의 버퍼 정보를 구조체화
struct PER_IO_DATA
{
	OVERLAPPED overlapped;
	WSABUF     wsaBuf;
	int        rwMode;
};

// 실제 데이터 구조체
struct DATA_INFO {
	char*	arr;
	int		reference_count; //참조 카운트가 0이면 delete 가능하다.
};

// 패킷 구조체
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