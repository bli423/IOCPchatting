#include "stdafx.h"
#include "IOCPServer.h"

IOCPServer::IOCPServer(IOCPCallback& _callback) : m_Callback(_callback)
{	
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

	//work thread 생성   cpu스레드수* 2 systemInfo.dwNumberOfProcessors * 8
	for (int i = 0; i < systemInfo.dwNumberOfProcessors * 2; i++) {
		_beginthreadex(NULL, 0, m_WorkThread, (void*)this, 0, NULL);
	}
	for (int i = 0; i < systemInfo.dwNumberOfProcessors * 2; i++) {
		_beginthreadex(NULL, 0, m_SendThread, (void*)this, 0, NULL);
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

		int n;
		if ((n = m_SendQue.size()) > BUFFER_WARING)
		{
			if (n % 1000 == 0) std::cout << n << "send_queue warnig" << std::endl;
		}

		//데이터 전송
		char* sendData = sendPacket->getData();
		int sendLen = sendPacket->getDataLen();


		if (send(sendPacket->getSocket(), sendData, sendLen, 0) == -1) {
		}

		delete sendPacket;
	}
	
	
	
}

UINT WINAPI IOCPServer::work() {
	CLIENT_DATA		*clientData;
	IO_DATA			*ioData;

	HANDLE			cp = (HANDLE)m_IOCP;
	DWORD			bytesTransferred;
	DWORD			dwFlags = 0;
	

	int dataLen = 0;
	char* arr = nullptr;
	ULONG  isNonBlocking = 1;

	while (true) {
		// 입출력 이벤트 대기
		GetQueuedCompletionStatus(cp, &bytesTransferred, (LPDWORD)&clientData, (LPOVERLAPPED*)&ioData, INFINITE);


		if (ioData->rwMode == READ) {
			arr = nullptr;

			//non-blocking IO 설정
			if (ioctlsocket(clientData->hClntSock, FIONBIO, &isNonBlocking) == 0) {}


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
			if (dataLen>0) {
				//패킷데이터 
				DATA *data = new DATA;
				data->m_arr = arr;
				data->m_len = dataLen;
				data->m_refCount = 1;

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
				m_Callback.socketClose(clientData->clntAddr);
				closesocket(clientData->hClntSock);
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
		m_CV_SendQue.notify_one();
		//WSASend(packet.getSocket(), &(sendPacket->getIoData().wsaBuf), 1, &m_SendSize, m_SendFlag, &(sendPacket->getIoData().overlapped), NULL);
	}
}

