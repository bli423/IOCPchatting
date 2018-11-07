#include "stdafx.h"
#include "IOCPServer.h"

IOCPServer::IOCPServer(IOCPCallback& _callback) : m_Callback(_callback)
{
	c = 0;
}
IOCPServer::~IOCPServer()
{
	closesocket(m_ServerSocket);
	WSACleanup();
}

bool IOCPServer::Init()
{
	WSAData wsaData;
	SYSTEM_INFO systemInfo;

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		return false;
	}
	//cp 생성
	m_IOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

	GetSystemInfo(&systemInfo);

	//work thread 생성 
	for (int i = 0; i < systemInfo.dwNumberOfProcessors; i++) {
		_beginthreadex(NULL, 0, m_WorkThread, (void*)this, 0, NULL);
	}
	for (int i = 0; i < systemInfo.dwNumberOfProcessors; i++) {
		_beginthreadex(NULL, 0, m_SendThread, (void*)this, 0, NULL);
	}
	for (int i = 0; i < systemInfo.dwNumberOfProcessors; i++) {
		_beginthreadex(NULL, 0, m_DelaySendThread, (void*)this, 0, NULL);
	}

	//서버 listen 소켓 생성
	m_ServerSocket = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (m_ServerSocket == INVALID_SOCKET) {
		return false;
	}

	// 서버 소켓 바인드
	memset(&m_ServerAddr, 0, sizeof(SOCKADDR_IN));
	m_ServerAddr.sin_family = PF_INET;
	m_ServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	m_ServerAddr.sin_port = htons(PORT);
	if (bind(m_ServerSocket, (SOCKADDR*)&m_ServerAddr, sizeof(SOCKADDR_IN)) == SOCKET_ERROR) {
		return false;
	}

	// 서버 소켓 listen 설정
	if (listen(m_ServerSocket, BACKLOG) == SOCKET_ERROR) {
		return false;
	}

	return true;
}

void IOCPServer::Run() {
	_beginthreadex(NULL, 0, acceptThread, (void*)this, 0, NULL);
}

unsigned int __stdcall IOCPServer::acceptThread(void* p_this)
{
	IOCPServer* this_IOCPServer = static_cast<IOCPServer*>(p_this);
	this_IOCPServer->acceptRun();

	return 0;
}
unsigned int __stdcall IOCPServer::m_WorkThread(void* p_this) {
	IOCPServer* this_IOCPServer = static_cast<IOCPServer*>(p_this);
	this_IOCPServer->work();

	return 0;
}
unsigned int __stdcall IOCPServer::m_SendThread(void *p_this)
{
	IOCPServer* this_IOCPServer = static_cast<IOCPServer*>(p_this);
	this_IOCPServer->sendWork();

	return 0;
} 
unsigned int __stdcall IOCPServer::m_DelaySendThread(void *p_this)
{
	IOCPServer* this_IOCPServer = static_cast<IOCPServer*>(p_this);
	this_IOCPServer->delaySendWork();

	return 0;
}


void IOCPServer::acceptRun()
{
	DWORD receiveBytes;
	DWORD dwFlags;
	ULONG  isNonBlocking = 1;


	while (true) {
		IO_DATA		*perIoData;
		CLIENT_DATA *perHandleData;
		SOCKET			clientSocket;
		SOCKADDR_IN		clientAddr;
		int addrLen = sizeof(clientAddr);

		clientSocket = accept(m_ServerSocket, (SOCKADDR*)&clientAddr, &addrLen);
		if (clientSocket == INVALID_SOCKET) {
			break;
		}

		//non-blocking IO 설정
		if (ioctlsocket(clientSocket, FIONBIO, &isNonBlocking) == 0) {}

		//핸들정보를 클라이언트 소켓정보로 설정
		perHandleData = new CLIENT_DATA;
		perIoData = new IO_DATA;

		perHandleData->hClntSock = clientSocket;
		memcpy(&(perHandleData->clntAddr), &clientAddr, addrLen);

		//iocp와 소켓 연결
		CreateIoCompletionPort((HANDLE)perHandleData->hClntSock, m_IOCP, (DWORD)perHandleData, 0);


		//클라이언트 설정		
		memset(&perIoData->overlapped, 0, sizeof(OVERLAPPED));
		perIoData->wsaBuf.len = 0;
		perIoData->wsaBuf.buf = NULL;
		perIoData->rwMode = READ;
		dwFlags = 0;

		WSARecv(perHandleData->hClntSock, &(perIoData->wsaBuf), 1, &receiveBytes, &dwFlags, &(perIoData->overlapped), NULL);
	}
}

void IOCPServer::sendWork()
{
	Packet* sendPacket;

	int &count_send = count[c++];

	while (true)
	{
		{
			std::unique_lock<std::mutex> lock(m_Mutex_SendQue);
			while (m_SendQue.empty()) {
				m_CV_SendQue.wait(lock);
			}
			sendPacket = m_SendQue.front();
			m_SendQue.pop();
		}


		//데이터 전송
		char* sendData = sendPacket->getData();
		int sendLen = sendPacket->getDataLen();

		int result = send(sendPacket->getSocket(), sendData, sendLen, 0);

		if (result <= 0) {
			
			int error = WSAGetLastError();

			if(error == WSAEWOULDBLOCK)
			{				
				std::lock_guard<std::mutex> lock(m_Mutex_DelaySendQue);
				m_DelaySendQue.push(sendPacket);
				m_CV_DelaySendQue.notify_all();
			}
			else
			{
				closesocket(sendPacket->getSocket());
				delete sendPacket;
			}		
		}
		else if (result < sendPacket->getDataLen()) 
		{			
			///////////////////////////////////////////
			////    
			////     패킷이 적게 보내질 경우 
			////
			///////////////////////////////////////////
		}
		else 
		{
			delete sendPacket;
		}		

		count_send++;
	}
}


void IOCPServer::delaySendWork()
{
	Packet* sendPacket;

	while (true)
	{
		{
			std::unique_lock<std::mutex> lock(m_Mutex_DelaySendQue);
			while (m_DelaySendQue.empty()) {
				m_CV_DelaySendQue.wait(lock);
			}
			sendPacket = m_DelaySendQue.front();
			m_DelaySendQue.pop();
		}

		char* sendData = sendPacket->getData();
		int sendLen = sendPacket->getDataLen();
		bool disConnect = true;

		for (int i = 0; i <10; i++)
		{
			int result = send(sendPacket->getSocket(), sendData, sendLen, 0);
			if (result <= 0) 
			{
				int error = WSAGetLastError();

				if (error != WSAEWOULDBLOCK)
				{
					disConnect = true;
					break;
				}
			}
			else
			{
				disConnect = false;
				break;
			}
			disConnect = true;
			Sleep(10);
		}

		///// 지연되는 사용자 연결 제거
		if (disConnect)
		{
			closesocket(sendPacket->getSocket());		
		}
		else
		{
			std::cout << m_DelaySendQue.size() << "딜레이 " << std::endl;
		}

		delete sendPacket;
	}
	
}

UINT WINAPI IOCPServer::work() {


	HANDLE			cp = (HANDLE)m_IOCP;
	DWORD			bytesTransferred;
	DWORD			dwFlags = 0;


	int dataLen = 0;
	char* arr = nullptr;
	ULONG  isNonBlocking = 1;

	while (true) {
		CLIENT_DATA		*clientData;
		IO_DATA			*ioData;
		// 입출력 이벤트 대기
		GetQueuedCompletionStatus(cp, &bytesTransferred, (LPDWORD)&clientData, (LPOVERLAPPED*)&ioData, INFINITE);

		int	rwMode;

		{
			std::lock_guard<std::mutex> lock(syn);
			if (ioData->len == 0xddddddddL)
			{
				rwMode = 0;
			}
			else
			{
				rwMode = ioData->rwMode;
			}			
		}
		
			

		if (rwMode == READ) {
			arr = nullptr;


			char* buffer = new char[BUFSIZE];
			int len;


			//비동기로 데이터를 수신한다.
			while (true) {
				len = recv(clientData->hClntSock, buffer, BUFSIZE, 0);

				if (len <= 0) { // 전송끝
					break;
				}
				else {
					// 이전 데이터와 합친다.
					char* tempBuf = arr;
					arr = new char[dataLen + len];
					memcpy(arr, tempBuf, dataLen);
					memcpy(&arr[dataLen], buffer, len);

					dataLen += len;

					delete tempBuf;
				}
			}

			//수신 버퍼 초기화
			delete buffer;

			//데이터 길이가 0이상이면 데이터 수신 성공 
			if (dataLen > 0) {
				//패킷데이터 
				PacketData *data = new PacketData(arr, dataLen, 1);

				Packet *packet = new Packet(clientData->hClntSock, clientData->clntAddr, *data);

				//패킷 처리큐에 전송
				m_Callback.receivePacket(*packet);


				dataLen = 0;

				ioData->wsaBuf.len = 0;
				ioData->wsaBuf.buf = NULL;
				ioData->rwMode = READ;
				dwFlags = 0;

				WSARecv(clientData->hClntSock, &(ioData->wsaBuf), 1, &bytesTransferred, &dwFlags, &(ioData->overlapped), NULL);
			}
			//데이터 길이가 0이면 소켓 연결 종료
			else
			{
				
				{
					std::lock_guard<std::mutex> lock(syn);

					SOCKET deleteSocket = clientData->hClntSock;
					if (deleteSocket != 0xddddddddL)
					{
						m_Callback.socketClose(clientData->clntAddr);
						delete clientData;						
						delete ioData;
						closesocket(deleteSocket);						
					}										

					continue;
				}
				
			}
		}
		

	}


	return 0;
}

void IOCPServer::sendData(Packet& packet) {
	// 전송큐에 push 및 송신 이밴트
	{
		std::lock_guard<std::mutex> lock(m_Mutex_SendQue);
		m_SendQue.push(&packet);
		m_CV_SendQue.notify_all();
	}
}

void IOCPServer::sendData(SOCKET* _targetList, int _listSize, char* _data, int _dataSize)
{
	PacketData *send_data = new PacketData(_data, _dataSize, _listSize);

	for (int i = 0; i < _listSize; i++)
	{
		Packet *sendPacket = new Packet(_targetList[i], *send_data);
		{
			std::lock_guard<std::mutex> lock(m_Mutex_SendQue);
			m_SendQue.push(sendPacket);
			m_CV_SendQue.notify_all();
		}
	}
}
void IOCPServer::sendData(SOCKET _target, char* _data, int _dataSize)
{
	PacketData *send_data = new PacketData(_data, _dataSize, 1);

	Packet *sendPacket = new Packet(_target, *send_data);
	{
		std::lock_guard<std::mutex> lock(m_Mutex_SendQue);
		m_SendQue.push(sendPacket);
		m_CV_SendQue.notify_all();
	}
}






int IOCPServer::getSendQueueSize()
{
	return m_SendQue.size();
}

int IOCPServer::getBufferFilledQueSize()
{
	return -1;
}

int IOCPServer::getCount()
{
	int result = 0;
	for (int i = 0; i < 8; i++)
	{
		result += count[i];
	}

	return result;
}

void IOCPServer::setCount()
{
	for (int i = 0; i < 8; i++)
	{
		count[i] = 0;
	}
}