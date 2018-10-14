#include "stdafx.h"
#include "IOCPServer.h"

IOCPServer::IOCPServer(TaskOperation* taskOperation)
{
	this->taskOperation = taskOperation;
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

	//work thread 생성   cpu스레드수* 2
	for (int i = 0; i < systemInfo.dwNumberOfProcessors * 8; i++) {
		_beginthreadex(NULL, 0, m_WorkThread, (void*)this, 0, NULL);
	}

	//서버 listen 소켓 생성
	m_ServerSocket = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (m_ServerSocket == INVALID_SOCKET) {		
		return false;
	}

	// 서버 소켓 바인드
	memset(&m_serverAddr, 0, sizeof(SOCKADDR_IN));
	m_serverAddr.sin_family = PF_INET;
	m_serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	m_serverAddr.sin_port = htons(PORT);
	if (bind(m_ServerSocket, (SOCKADDR*)&m_serverAddr, sizeof(SOCKADDR_IN)) == SOCKET_ERROR) {		
		return false;
	}

	// 서버 소켓 listen 설정
	if (listen(m_ServerSocket, BACKLOG) == SOCKET_ERROR) {	
		return false;
	}

	return true;
}

void IOCPServer::Run() {

	DWORD receiveBytes;
	DWORD dwFlags;
	ULONG  isNonBlocking = 1;

	
	while (true) {
		PER_IO_DATA* perIoData;
		PER_HANDLE_DATA* perHandleData;
		SOCKET clientSocket;
		SOCKADDR_IN clientAddr;
		int addrLen = sizeof(clientAddr);

		clientSocket = accept(m_ServerSocket, (SOCKADDR*)&clientAddr, &addrLen);
		if (clientSocket == INVALID_SOCKET) {
			break;
		}
		
		//핸들정보를 클라이언트 소켓정보로 설정
		perHandleData = new PER_HANDLE_DATA;


		perHandleData->hClntSock = clientSocket;
		memcpy(&(perHandleData->clntAddr), &clientAddr, addrLen);
		//iocp와 소켓 연결
		CreateIoCompletionPort((HANDLE)perHandleData->hClntSock, m_IOCP, (DWORD)perHandleData, 0);
		//클라이언트 설정
		perIoData = new PER_IO_DATA;
		memset(&perIoData->overlapped, 0, sizeof(OVERLAPPED));
		perIoData->wsaBuf.len = 0;
		perIoData->wsaBuf.buf = NULL;
		perIoData->rwMode = READ;
		dwFlags = 0;

		WSARecv(perHandleData->hClntSock, &(perIoData->wsaBuf), 1, &receiveBytes, &dwFlags, &(perIoData->overlapped), NULL);

	}
}

unsigned int __stdcall IOCPServer::m_WorkThread(void* server) {
	IOCPServer* this_IOCPServer = static_cast<IOCPServer*>(server);

	this_IOCPServer->work();

	return 0;
}

UINT WINAPI IOCPServer::work() {

	HANDLE cp = (HANDLE)m_IOCP;
	SOCKET clientSocket;
	DWORD bytesTransferred;
	DWORD dwFlags = 0;
	PER_HANDLE_DATA* perHandleData;
	PER_IO_DATA* perIoData;

	int dataLen = 0;
	char* data = nullptr;
	ULONG  isNonBlocking = 1;

	while (true) {
		// 입출력 이벤트 대기
		GetQueuedCompletionStatus(cp, &bytesTransferred, (LPDWORD)&perHandleData, (LPOVERLAPPED*)&perIoData, INFINITE);


		if (perIoData->rwMode == READ) {
			data = nullptr;

			//non-blocking IO 설정
			if (ioctlsocket(perHandleData->hClntSock, FIONBIO, &isNonBlocking) == 0) {}


			char* buffer = new char[BUFSIZE];
			int len;


			//비동기로 데이터를 수신한다.
			while (true) {
				
				len = recv(perHandleData->hClntSock, buffer, BUFSIZE, 0);

				if (len <= 0) { // 전송끝
					break;
				}
				else {
					// 이전 데이터와 합친다.
					char* tempBuf = data;
					data = new char[dataLen + len];
					memcpy(data, tempBuf, dataLen);
					memcpy(&data[dataLen], buffer, len);

					dataLen += len;

					delete tempBuf;
				}			
			}

			//수신 버퍼 초기화
			delete buffer;

			//데이터 길이가 0이상이면 데이터 수신 성공 
			if (dataLen>0) {
				//패킷데이터 
				PACKET_DATA* packet = new PACKET_DATA;
				DATA_INFO* data_info = new DATA_INFO;
				packet->data = data_info;
				packet->perIoData = perIoData;
				packet->hClntSock = perHandleData->hClntSock;
				memcpy(&packet->clntAddr, &perHandleData->clntAddr, sizeof(perHandleData->clntAddr));

				packet->data->arr = data;
				packet->data->reference_count = 1;
				packet->dataLen = dataLen;

				//패킷 처리큐에 전송
				taskOperation->receivePacket(packet);


				// 수신 이벤트 
				perIoData->wsaBuf.len = 0;
				perIoData->wsaBuf.buf = NULL;
				perIoData->rwMode = READ;
				dwFlags = 0;
				WSARecv(perHandleData->hClntSock, &(perIoData->wsaBuf), 1, &bytesTransferred, &dwFlags, &(perIoData->overlapped), NULL);

				dataLen = 0;
			}
			//데이터 길이가 0이면 소켓 연결 종료
			else {				
				std::cout << "rev error\n";

				//에러 패킷 생성 및 처리큐 전송
				char* socket_error_buf = new char[sizeof(Data_Header)];
				memset(socket_error_buf, 0, sizeof(Data_Header));
				Data_Header* error_header = (Data_Header*)socket_error_buf;
				error_header->totalLen = sizeof(Data_Header);

				error_header->protocol = C_SOCKET_ERROR;

				PACKET_DATA* socket_error_packet = new PACKET_DATA;
				DATA_INFO* data_info = new DATA_INFO;
				socket_error_packet->data = data_info;
				socket_error_packet->perIoData = perIoData;
				socket_error_packet->hClntSock = perHandleData->hClntSock;
				memcpy(&socket_error_packet->clntAddr, &perHandleData->clntAddr, sizeof(perHandleData->clntAddr));
				socket_error_packet->data->arr = socket_error_buf;
				socket_error_packet->data->reference_count = 1;
				socket_error_packet->dataLen = sizeof(Data_Header);

				taskOperation->receivePacket(socket_error_packet);

				// 소켓 종료
				closesocket(perHandleData->hClntSock);
			}
		}
		else if (perIoData->rwMode == WRITE) {
			PACKET_DATA* packet;

			// 전송큐에서 데이터를 받는다
			{
				std::unique_lock<std::mutex> lock(mutex_send_queue);
				if (send_queue.empty()) {
					continue;
				}

				packet = send_queue.front();
				send_queue.pop();
			}

			
			if (send_queue.size() > BUFFER_WARING) std::cout << "send_queue warnig\n";

			//데이터 전송
			if (packet->data != NULL) {
				if (send(packet->hClntSock, packet->data->arr, packet->dataLen, 0) == -1) {
					std::cout << "send error\n";
				}

				//패킷 데이터 참조값 조사후 delete
				taskOperation->sendPacketClear(packet);
			}


			//송신 이벤트 
			perIoData->wsaBuf.len = 0;
			perIoData->wsaBuf.buf = NULL;
			perIoData->rwMode = WRITE;

			WSASend(perHandleData->hClntSock, &(perIoData->wsaBuf), 1, &sendSize, sendFlag, &(perIoData->overlapped), NULL);
		}
		else {
		
		}
	}
	return 0;
}

void IOCPServer::sendData(PACKET_DATA* packet) {

	// 전송큐에 push 및 송신 이밴트
	{
		std::lock_guard<std::mutex> lock(mutex_send_queue);
		packet->perIoData->wsaBuf.len = 0;
		packet->perIoData->wsaBuf.buf = NULL;
		packet->perIoData->rwMode = WRITE;
		send_queue.push(packet);

		WSASend(packet->hClntSock, &(packet->perIoData->wsaBuf), 1, &sendSize, sendFlag, &(packet->perIoData->overlapped), NULL);
	}

}

