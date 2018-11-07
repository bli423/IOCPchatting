#include "stdafx.h"
#include "TaskOperation.h"



TaskOperation::TaskOperation()
{
	
}

TaskOperation::~TaskOperation()
{
	
}

void TaskOperation::Init(IOCPServer& _iocp) {
	iocp = &_iocp;
	m_UserTable.Init(_iocp);
	dbThread = new DBThread;
	dbThread->Init(*this);
	
}


void TaskOperation::Run() {
	_beginthreadex(NULL, 0, receiveThread, (void*)this, 0, NULL);
	for (int i = 0; i < 4; i++)
	{
		_beginthreadex(NULL, 0, messageThread, (void*)this, 0, NULL);
	}
	_beginthreadex(NULL, 0, timerThread, (void*)this, 0, NULL);
	dbThread->Run();
}


void TaskOperation::receivePacket(Packet& data) 
{
	
	std::lock_guard<std::mutex> lock(m_Mutex_ReceiveBuf);
	m_ReceiveBuf.push(&data);
	m_CV_ReceiveBuf.notify_one();
		

}

void TaskOperation::socketClose(SOCKADDR_IN& _client)
{
	User *user = m_UserTable.getUserOrNULL(_client);

	if (user != NULL)
	{
		m_UserTable.eixtUserToRoom(*user);
		m_UserTable.removeUser(*user);
	}
}
unsigned int __stdcall TaskOperation::timerThread(void* taskOperation)
{
	TaskOperation* this_taskOperation = static_cast<TaskOperation*>(taskOperation);
	this_taskOperation->timerRun();
	return 0;
}

void TaskOperation::timerRun()
{
	while (true)
	{
		Sleep(100);

		m_UserTable.sendChats();
	}
}


// ��Ŷ�� ���� �˻��ϴ� ������
unsigned int __stdcall TaskOperation::receiveThread(void* taskOperation) {
	TaskOperation* this_taskOperation = static_cast<TaskOperation*>(taskOperation);
	this_taskOperation->receiveRun();
	return 0;
}

void TaskOperation::receiveRun() {
	while (true) {

		Packet *packet;
		{
			std::unique_lock<std::mutex> lock(m_Mutex_ReceiveBuf);
			while (m_ReceiveBuf.empty()) {
				m_CV_ReceiveBuf.wait(lock);
			}
			packet = m_ReceiveBuf.front();
			m_ReceiveBuf.pop();
		}


		//// ��Ŷ �˻�
		/// �߷������ִ� ��Ŷ�� �˻�
		u_int64 packetId = packet->getIPandPORT();


		std::map<u_int64, Packet*>::iterator find = m_PacketBuffer.find(packetId);

		// ������ �߷��� ��Ŷ ����
		if (find != m_PacketBuffer.end()) {
			Packet& old_packet = *(*find).second;

			old_packet.merge(*packet);

			delete packet;
			packet = &old_packet;
		}
		
		unsigned short packetLen = *(unsigned short*)packet->getData();


		// ��Ŷ ���� ����

		if (packetLen == packet->getDataLen())
		{
			if (find != m_PacketBuffer.end()) {
				m_PacketBuffer.erase(find);
			}

			{
				std::lock_guard<std::mutex> lock(m_Mutex_MessageBuf);
				m_MessageBuf.push_back(packet);
				m_CV_MessageBuf.notify_one();
			}
		}
		else if (packetLen < packet->getDataLen())
		{
			while (packetLen < packet->getDataLen())
			{
				Packet& cut_packet = packet->cutInTwo(packetLen);
				{
					std::lock_guard<std::mutex> lock(m_Mutex_MessageBuf);
					m_MessageBuf.push_back(packet);
					m_CV_MessageBuf.notify_one();
				}

				packet = &cut_packet;

				packetLen = *(unsigned short*)packet->getData();
			}

			if (find != m_PacketBuffer.end())
			{
				m_PacketBuffer.erase(find);
			}

			if (packetLen > packet->getDataLen() )
			{				
				m_PacketBuffer.insert(std::map<u_int64, Packet*>::value_type(packetId, packet));				
			}
			else
			{
				{
					std::lock_guard<std::mutex> lock(m_Mutex_MessageBuf);
					m_MessageBuf.push_back(packet);
					m_CV_MessageBuf.notify_one();
				}
			}
		}
		else {
			//ó��
			if (find == m_PacketBuffer.end())
			{
				m_PacketBuffer.insert(std::map<u_int64, Packet*>::value_type(packetId, packet));
			}
		}	
	}
}



// ������ ó�� ������
unsigned int __stdcall TaskOperation::messageThread(void* taskOperation) {
	TaskOperation* this_taskOperation = static_cast<TaskOperation*>(taskOperation);
	this_taskOperation->messageRun();
	return 0;
}

void TaskOperation::messageRun() {
	while (true) {
		Packet *packet;
		{
			std::unique_lock<std::mutex> lock(m_Mutex_MessageBuf);
			while (m_MessageBuf.empty()) {
				m_CV_MessageBuf.wait(lock);
			}
			packet = m_MessageBuf.front();
			m_MessageBuf.pop_front();
		}


		Default_Header* default_header;
		default_header = (Default_Header*)packet->getData();
		std::string userid(default_header->id, USERID_LEN);


		// ���� ����� ���
		if (default_header->protocol == C_CONNECT) 
		{			
		
			User *user = new User(userid, packet->getSocket(), packet->getClientAddr());

			bool canLogin = m_UserTable.addUser(*user);

		
			///////////////��Ŷ ����/////////////////////
			WORD totalLen = sizeof(C_CONNECT_Header_Answer);
			char *data = new char[totalLen];

			C_CONNECT_Header_Answer* connect_answer = (C_CONNECT_Header_Answer*)data;
			connect_answer->totalLen = totalLen;
			connect_answer->protocol = C_CONNECT;
			memcpy(connect_answer->id, userid.c_str(), USERID_LEN);	
			connect_answer->canLogin = canLogin;

	
			iocp->sendData(user->getSocket(), data, totalLen);
		}

		//���� ���� ����� ����
		else if (default_header->protocol == C_CLOSE) 
		{
			User *user = m_UserTable.getUserOrNULL(userid);
			

			//////////////////��Ŷ ����///////////////////////
			WORD totalLen = sizeof(C_CLOSE_Header_Answer);
			char *data = new char[totalLen];

			C_CLOSE_Header_Answer* close_answer = (C_CLOSE_Header_Answer*)data;
			close_answer->totalLen = totalLen;
			close_answer->protocol = C_CLOSE;
			memcpy(close_answer->id, userid.c_str(), USERID_LEN);
			if (user == NULL)
			{
				close_answer->canLogout = false;
			
				iocp->sendData(packet->getSocket(), data, totalLen);
			}
			else 
			{
				close_answer->canLogout = true;

				iocp->sendData(packet->getSocket(), data, totalLen);
			}

					
			//user ���� ��ȯ
			if (user != NULL)
			{
				m_UserTable.removeUser(*user);
			}
		}

		//���� ä�ù濡 ����
		else if (default_header->protocol == C_ENTER_ROOM) 
		{

			User *user = m_UserTable.getUserOrNULL(userid);

			C_ENTER_ROOM_Header_Ask* enter_room_header;
			enter_room_header = (C_ENTER_ROOM_Header_Ask*)packet->getData();

			bool isEnter;
			WORD roomid = enter_room_header->roomid;

			if (user != NULL) {
				isEnter = m_UserTable.enterUserToRoom(*user, roomid);
			}
			else {
				isEnter = false;
			}


			if (isEnter) {
				WORD totalLen = sizeof(C_ENTER_ROOM_Header_Answer);
				char* data = new char[totalLen];

				C_ENTER_ROOM_Header_Answer* enter_room_answer = (C_ENTER_ROOM_Header_Answer*)data;
				enter_room_answer->totalLen = totalLen;
				enter_room_answer->protocol = C_ENTER_ROOM;
				memcpy(enter_room_answer->id, userid.c_str(), USERID_LEN);
				enter_room_answer->roomid = roomid;
				enter_room_answer->canEnter = isEnter;
				
				iocp->sendData(user->getSocket(), data, totalLen);
			}
			else {///���� �ź�
				WORD totalLen = sizeof(C_ENTER_ROOM_Header_Answer);
				char* data = new char[totalLen];

				C_ENTER_ROOM_Header_Answer* enter_room_answer = (C_ENTER_ROOM_Header_Answer*)data;
				enter_room_answer->totalLen = totalLen;
				enter_room_answer->protocol = C_ENTER_ROOM;
				memcpy(enter_room_answer->id, userid.c_str(), USERID_LEN);
				enter_room_answer->roomid = roomid;
				enter_room_answer->canEnter = isEnter;			

				iocp->sendData(user->getSocket(), data, totalLen);
			}	

			//user ���� ��ȯ
			if (user != NULL) {
				m_UserTable.returnUser(*user);
			}

		}

		//���� ä�ù� ����
		else if (default_header->protocol == C_EXIT_ROOM) 
		{
			User *user = m_UserTable.getUserOrNULL(userid);

			if (user != NULL) {
				C_EXIT_ROOM_Header_Ask* enter_room_header;
				enter_room_header = (C_EXIT_ROOM_Header_Ask*)packet->getData();

				int roomid = user->getmRoomID();

				WORD totalLen = sizeof(C_ENTER_ROOM_Header_Answer);
				char* data = new char[totalLen];


				C_EXIT_ROOM_Header_Answer* exit_room_answer = (C_EXIT_ROOM_Header_Answer*)data;
				exit_room_answer->totalLen = totalLen;
				exit_room_answer->protocol = C_EXIT_ROOM;
				memcpy(exit_room_answer->id, userid.c_str(), USERID_LEN);

				if (roomid != -1)
				{
					exit_room_answer->canEexit = true;

					int listSize;
					SOCKET* list = m_UserTable.getUserListInRoom(*user, &listSize);
					iocp->sendData(list, listSize, data, totalLen);
					delete [listSize]list;

					m_UserTable.eixtUserToRoom(*user);
				}
				else
				{
					exit_room_answer->canEexit = false;
					iocp->sendData(user->getSocket(), data, totalLen);
				}
			}			

			//user ���� ��ȯ
			if (user != NULL) {
				m_UserTable.returnUser(*user);
			}
		}

		//���� �޼��� ����
		else if (default_header->protocol == C_MESSAGE) 
		{
			User *user = m_UserTable.getUserOrNULL(userid);

			if (user != NULL) {
				C_MESSAGE_Header* message_header = (C_MESSAGE_Header*)packet->getData();

				WORD message_len = message_header->messageLen;

				///��Ŷ ����
				WORD totalLen = sizeof(C_MESSAGE_Header) + message_len;
				char* data = new char[totalLen];

				C_MESSAGE_Header* message_send = (C_MESSAGE_Header*)data;
				message_send->totalLen = totalLen;
				message_send->protocol = C_MESSAGE;
				memcpy(message_send->id, userid.c_str(), USERID_LEN);
				message_send->messageLen = message_len;
				memcpy(&data[sizeof(C_MESSAGE_Header)], &packet->getData()[sizeof(C_MESSAGE_Header)], message_len);
						

				m_UserTable.addChats(user->getmRoomID(), data, totalLen);

				//DB�� �α� ����
				/*string *message = new string(&packet->getData()[sizeof(C_MESSAGE_Header)], message_len);
				dbThread->addMessage(userid, user->getmRoomID(), *message);*/
			}

			//user ���� ��ȯ
			if (user != NULL) {
				m_UserTable.returnUser(*user);
			}
		}

		//���� �α� ��� ��û
		else if (default_header->protocol == C_REQUST_ROOMINFO) 
		{
			C_REQUST_ROOMINFO_Header * requst_roominfo_header = (C_REQUST_ROOMINFO_Header*)packet->getData();
			WORD roomid = requst_roominfo_header->roomid;

			dbThread->getRoomLog(userid, roomid);
		}	
		else {
			
		}

		delete packet;
	}
}

void TaskOperation::completeDBJob(string& _requstID, char* _data, int _data_len)
{
	User *user = m_UserTable.getUserOrNULL(_requstID);

	if (user != NULL) {
		char* send_data = new char[sizeof(C_RECEIVE_ROOMINFO_Header) + _data_len];
		WORD totalLen = sizeof(C_RECEIVE_ROOMINFO_Header) + _data_len;


		C_RECEIVE_ROOMINFO_Header* receive_roominfo_send = (C_RECEIVE_ROOMINFO_Header*)send_data;
		receive_roominfo_send->totalLen = totalLen;
		receive_roominfo_send->protocol = C_RECEIVE_ROOMINFO;
		memcpy(receive_roominfo_send->id, user->getID().c_str(), USERID_LEN);
		receive_roominfo_send->messageLen = _data_len;
		memcpy(&send_data[sizeof(C_RECEIVE_ROOMINFO_Header)], _data, _data_len);


		iocp->sendData(user->getSocket(), send_data, totalLen);
	}

	//user ���� ��ȯ
	if (user != NULL) {
		m_UserTable.returnUser(*user);
	}
}


int TaskOperation::getMessageBufSize()
{
	return m_MessageBuf.size();
}
int TaskOperation::getUserTableSize()
{
	return m_UserTable.getUserListSize();
}