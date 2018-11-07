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
	//cp ����
	m_IOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

	GetSystemInfo(&systemInfo);

	//work thread ���� 
	for (int i = 0; i < systemInfo.dwNumberOfProcessors; i++) {
		_beginthreadex(NULL, 0, m_WorkThread, (void*)this, 0, NULL);
	}
	for (int i = 0; i < systemInfo.dwNumberOfProcessors*2; i++) {
		_beginthreadex(NULL, 0, m_SendThread, (void*)this, 0, NULL);
	}

	//���� listen ���� ����
	m_ServerSocket = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (m_ServerSocket == INVALID_SOCKET) {
		return false;
	}

	// ���� ���� ���ε�
	memset(&m_ServerAddr, 0, sizeof(SOCKADDR_IN));
	m_ServerAddr.sin_family = PF_INET;
	m_ServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	m_ServerAddr.sin_port = htons(PORT);
	if (bind(m_ServerSocket, (SOCKADDR*)&m_ServerAddr, sizeof(SOCKADDR_IN)) == SOCKET_ERROR) {
		return false;
	}

	// ���� ���� listen ����
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

		//non-blocking IO ����
		if (ioctlsocket(clientSocket, FIONBIO, &isNonBlocking) == 0) {}

		//�ڵ������� Ŭ���̾�Ʈ ���������� ����
		perHandleData = new CLIENT_DATA;
		perIoData = new IO_DATA;

		perHandleData->hClntSock = clientSocket;
		memcpy(&(perHandleData->clntAddr), &clientAddr, addrLen);

		//iocp�� ���� ����
		CreateIoCompletionPort((HANDLE)perHandleData->hClntSock, m_IOCP, (DWORD)perHandleData, 0);


		//Ŭ���̾�Ʈ ����		
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
	ULONG  isNonBlocking = 1;

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


		//������ ����
		char* sendData = sendPacket->getData();
		int sendLen = sendPacket->getDataLen();

		if (send(sendPacket->getSocket(), sendData, sendLen, 0) <= 0) {
			closesocket(sendPacket->getSocket());
		}
		delete sendPacket;

		count_send++;
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
		// ����� �̺�Ʈ ���
		GetQueuedCompletionStatus(cp, &bytesTransferred, (LPDWORD)&clientData, (LPOVERLAPPED*)&ioData, INFINITE);

		int	rwMode;

		{
			std::lock_guard<std::mutex> lock(test);
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


			//�񵿱�� �����͸� �����Ѵ�.
			while (true) {
				len = recv(clientData->hClntSock, buffer, BUFSIZE, 0);

				if (len <= 0) { // ���۳�
					break;
				}
				else {
					// ���� �����Ϳ� ��ģ��.
					char* tempBuf = arr;
					arr = new char[dataLen + len];
					memcpy(arr, tempBuf, dataLen);
					memcpy(&arr[dataLen], buffer, len);

					dataLen += len;

					delete tempBuf;
				}
			}

			//���� ���� �ʱ�ȭ
			delete buffer;

			//������ ���̰� 0�̻��̸� ������ ���� ���� 
			if (dataLen > 0) {
				//��Ŷ������ 
				PacketData *data = new PacketData(arr, dataLen, 1);

				Packet *packet = new Packet(clientData->hClntSock, clientData->clntAddr, *data);

				//��Ŷ ó��ť�� ����
				m_Callback.receivePacket(*packet);


				dataLen = 0;

				ioData->wsaBuf.len = 0;
				ioData->wsaBuf.buf = NULL;
				ioData->rwMode = READ;
				dwFlags = 0;

				WSARecv(clientData->hClntSock, &(ioData->wsaBuf), 1, &bytesTransferred, &dwFlags, &(ioData->overlapped), NULL);
			}
			//������ ���̰� 0�̸� ���� ���� ����
			else
			{
				
				{
					std::lock_guard<std::mutex> lock(test);

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
	// ����ť�� push �� �۽� �̹�Ʈ
	{
		std::lock_guard<std::mutex> lock(m_Mutex_SendQue);
		m_SendQue.push(&packet);
		m_CV_SendQue.notify_one();
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
			m_CV_SendQue.notify_one();
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
		m_CV_SendQue.notify_one();
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