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
	//cp ����
	m_IOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

	GetSystemInfo(&systemInfo);

	//work thread ����   cpu�������* 2
	for (int i = 0; i < systemInfo.dwNumberOfProcessors * 8; i++) {
		_beginthreadex(NULL, 0, m_WorkThread, (void*)this, 0, NULL);
	}

	//���� listen ���� ����
	m_ServerSocket = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (m_ServerSocket == INVALID_SOCKET) {		
		return false;
	}

	// ���� ���� ���ε�
	memset(&m_serverAddr, 0, sizeof(SOCKADDR_IN));
	m_serverAddr.sin_family = PF_INET;
	m_serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	m_serverAddr.sin_port = htons(PORT);
	if (bind(m_ServerSocket, (SOCKADDR*)&m_serverAddr, sizeof(SOCKADDR_IN)) == SOCKET_ERROR) {		
		return false;
	}

	// ���� ���� listen ����
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
		
		//�ڵ������� Ŭ���̾�Ʈ ���������� ����
		perHandleData = new PER_HANDLE_DATA;


		perHandleData->hClntSock = clientSocket;
		memcpy(&(perHandleData->clntAddr), &clientAddr, addrLen);
		//iocp�� ���� ����
		CreateIoCompletionPort((HANDLE)perHandleData->hClntSock, m_IOCP, (DWORD)perHandleData, 0);
		//Ŭ���̾�Ʈ ����
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
		// ����� �̺�Ʈ ���
		GetQueuedCompletionStatus(cp, &bytesTransferred, (LPDWORD)&perHandleData, (LPOVERLAPPED*)&perIoData, INFINITE);


		if (perIoData->rwMode == READ) {
			data = nullptr;

			//non-blocking IO ����
			if (ioctlsocket(perHandleData->hClntSock, FIONBIO, &isNonBlocking) == 0) {}


			char* buffer = new char[BUFSIZE];
			int len;


			//�񵿱�� �����͸� �����Ѵ�.
			while (true) {
				
				len = recv(perHandleData->hClntSock, buffer, BUFSIZE, 0);

				if (len <= 0) { // ���۳�
					break;
				}
				else {
					// ���� �����Ϳ� ��ģ��.
					char* tempBuf = data;
					data = new char[dataLen + len];
					memcpy(data, tempBuf, dataLen);
					memcpy(&data[dataLen], buffer, len);

					dataLen += len;

					delete tempBuf;
				}			
			}

			//���� ���� �ʱ�ȭ
			delete buffer;

			//������ ���̰� 0�̻��̸� ������ ���� ���� 
			if (dataLen>0) {
				//��Ŷ������ 
				PACKET_DATA* packet = new PACKET_DATA;
				DATA_INFO* data_info = new DATA_INFO;
				packet->data = data_info;
				packet->perIoData = perIoData;
				packet->hClntSock = perHandleData->hClntSock;
				memcpy(&packet->clntAddr, &perHandleData->clntAddr, sizeof(perHandleData->clntAddr));

				packet->data->arr = data;
				packet->data->reference_count = 1;
				packet->data->dataLen = dataLen;

				//��Ŷ ó��ť�� ����
				taskOperation->receivePacket(packet);


				// ���� �̺�Ʈ 
				perIoData->wsaBuf.len = 0;
				perIoData->wsaBuf.buf = NULL;
				perIoData->rwMode = READ;
				dwFlags = 0;
				WSARecv(perHandleData->hClntSock, &(perIoData->wsaBuf), 1, &bytesTransferred, &dwFlags, &(perIoData->overlapped), NULL);

				dataLen = 0;
			}
			//������ ���̰� 0�̸� ���� ���� ����
			else {				
				std::cout << "rev error\n";

				//���� ��Ŷ ���� �� ó��ť ����
				char* socket_error_buf = new char[sizeof(C_SOCKET_ERROR_Header)];
				memset(socket_error_buf, 0, sizeof(C_SOCKET_ERROR_Header));
				C_SOCKET_ERROR_Header* error_header = (C_SOCKET_ERROR_Header*)socket_error_buf;
				error_header->totalLen = sizeof(C_SOCKET_ERROR_Header);

				error_header->protocol = C_SOCKET_ERROR;

				PACKET_DATA* socket_error_packet = new PACKET_DATA;
				DATA_INFO* data_info = new DATA_INFO;
				socket_error_packet->data = data_info;
				socket_error_packet->perIoData = perIoData;
				socket_error_packet->hClntSock = perHandleData->hClntSock;
				memcpy(&socket_error_packet->clntAddr, &perHandleData->clntAddr, sizeof(perHandleData->clntAddr));
				socket_error_packet->data->arr = socket_error_buf;
				socket_error_packet->data->reference_count = 1;
				socket_error_packet->data->dataLen = sizeof(C_SOCKET_ERROR_Header);

				taskOperation->receivePacket(socket_error_packet);

				// ���� ����
				closesocket(perHandleData->hClntSock);
			}
		}
		else if (perIoData->rwMode == WRITE) {
			PACKET_DATA* packet;

			// ����ť���� �����͸� �޴´�
			{
				std::unique_lock<std::mutex> lock(mutex_send_queue);
				if (send_queue.empty()) {
					continue;
				}

				packet = send_queue.front();
				send_queue.pop();
			}

			
			if (send_queue.size() > BUFFER_WARING) std::cout << "send_queue warnig\n";

			//������ ����
			if (packet->data != NULL) {
				if (send(packet->hClntSock, packet->data->arr, packet->data->dataLen, 0) == -1) {
					std::cout << "send error\n";
				}

				//��Ŷ ������ ������ ������ delete
				taskOperation->sendPacketClear(packet);
			}

			//�۽� �̺�Ʈ 
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

	// ����ť�� push �� �۽� �̹�Ʈ
	{
		std::lock_guard<std::mutex> lock(mutex_send_queue);
		packet->perIoData->wsaBuf.len = 0;
		packet->perIoData->wsaBuf.buf = NULL;
		packet->perIoData->rwMode = WRITE;
		send_queue.push(packet);

		WSASend(packet->hClntSock, &(packet->perIoData->wsaBuf), 1, &sendSize, sendFlag, &(packet->perIoData->overlapped), NULL);
	}

}

