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
	WORD dataLen;
	int		reference_count; //참조 카운트가 0이면 delete 가능하다.
};

// 패킷 구조체
struct PACKET_DATA
{
	SOCKET      hClntSock;
	SOCKADDR_IN clntAddr;
	PER_IO_DATA* perIoData;
	DATA_INFO* data;
};

struct TotalLen_Protocol {
	WORD totalLen;
	WORD protocol;
};
struct Default_Header {
	WORD totalLen;
	WORD protocol;
	char id[USERID_LEN];
};

struct C_CONNECT_Header_Ask {
	WORD totalLen;
	WORD protocol;
	char id[USERID_LEN];
};
struct C_CONNECT_Header_Answer {
	WORD totalLen;
	WORD protocol;
	char id[USERID_LEN];
	bool canLogin;
	char empty;
};
struct C_CLOSE_Header_Ask {
	WORD totalLen;
	WORD protocol;
	char id[USERID_LEN];
};
struct C_CLOSE_Header_Answer {
	WORD totalLen;
	WORD protocol;
	char id[USERID_LEN];	
	bool canLogout;
	char empty;
};

struct C_ENTER_ROOM_Header_Ask {
	WORD totalLen;
	WORD protocol;
	char id[USERID_LEN];
	WORD roomid;
};

struct C_ENTER_ROOM_Header_Answer {
	WORD totalLen;
	WORD protocol;
	char id[USERID_LEN];
	WORD roomid;
	bool canEnter;
	char empty;
};
struct C_ROOMINFO_Header {
	WORD totalLen;
	WORD protocol;
	char id[USERID_LEN];
	WORD count;
};
struct C_EXIT_ROOM_Header_Ask {
	WORD totalLen;
	WORD protocol;
	char id[USERID_LEN];
};
struct C_EXIT_ROOM_Header_Answer {
	WORD totalLen;
	WORD protocol;
	char id[USERID_LEN];
	bool canEexit;
	char empty;
};

struct C_SOCKET_ERROR_Header {
	WORD totalLen;
	WORD protocol;
	char id[USERID_LEN];
};


struct C_MESSAGE_Header {
	WORD totalLen;
	WORD protocol;
	char id[USERID_LEN];
	WORD messageLen;
};
struct C_REQUST_ROOMINFO_Header {
	WORD totalLen;
	WORD protocol;
	char id[USERID_LEN];
	WORD roomid;
};



struct C_RECEIVE_ROOMINFO_Header {
	WORD totalLen;
	WORD protocol;
	char id[USERID_LEN];
	WORD messageLen;
};





