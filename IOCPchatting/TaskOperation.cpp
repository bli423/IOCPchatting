#include "stdafx.h"
#include "TaskOperation.h"



TaskOperation::TaskOperation()
{
	room_counter = 0;
	for (int i = 0; i < ROOM_COUNT; i++) {
		room[i].rommId = i;
		roomList[room[i].rommId] = room[i];
	}
}

TaskOperation::~TaskOperation()
{

}

void TaskOperation::setIOCP(IOCPServer* iocp) {
	this->iocp = iocp;
}


void TaskOperation::Run() {
	_beginthreadex(NULL, 0, receiveThread, (void*)this, 0, NULL);
	_beginthreadex(NULL, 0, controlThread, (void*)this, 0, NULL);
}


// ��Ŷ�� ���� �˻��ϴ� ������
unsigned int __stdcall TaskOperation::receiveThread(void* taskOperation) {
	TaskOperation* this_taskOperation = static_cast<TaskOperation*>(taskOperation);
	this_taskOperation->receiveRun();
	return 0;
}

void TaskOperation::receiveRun() {
	while (true) {
		PACKET_DATA* packet;
		{

			std::unique_lock<std::mutex> lock(mutex_receiveBuf);
			while (receiveBuf.empty()) {
				cv_receiveBuf.wait(lock);
			}
			packet = receiveBuf.front();
			receiveBuf.pop();
		}


		//// ��Ŷ �˻�
		/// �߷������ִ� ��Ŷ�� �˻�

		_int64 packetId = packet->clntAddr.sin_addr.s_addr;
		packetId = packetId << (sizeof(packet->clntAddr.sin_port) * 8);
		packetId = packetId | packet->clntAddr.sin_port;

		std::map<_int64, PACKET_DATA*>::iterator find = packetBuffer.find(packetId);

		// ������ �߷��� ��Ŷ ����
		if (find != packetBuffer.end()) {
			PACKET_DATA* temp_packet = (PACKET_DATA*)(*find).second;
			int merge_data_len = temp_packet->dataLen + packet->dataLen;
			char* merge_data = new char[merge_data_len];

			memcpy(merge_data, temp_packet->data->arr, temp_packet->dataLen);
			memcpy(&merge_data[temp_packet->dataLen], packet->data->arr, packet->dataLen);

			delete packet->data->arr;


			packet->data->arr = merge_data;
			packet->dataLen = merge_data_len;
		}

		WORD* packet_tLen = (WORD*)packet->data->arr;

		// ��Ŷ ���� ����
		if (*packet_tLen > packet->dataLen) {
			
			//���̰� ª���� ��Ŷ�� ��ġ�� �ٽ� MAP�� ����
			if (find != packetBuffer.end()) {
				char* temp_data = new char[packet->dataLen];
				memcpy(temp_data, packet->data->arr, packet->dataLen);

				delete (*find).second->data->arr;

				(*find).second->data->arr = temp_data;
				(*find).second->dataLen = packet->dataLen;

			}
			else {
				PACKET_DATA* temp_packet = new PACKET_DATA;
				DATA_INFO* data_info = new DATA_INFO;
				temp_packet->data = data_info;

				temp_packet->data->arr = new char[packet->dataLen];
				temp_packet->data->reference_count = packet->data->reference_count;
				memcpy(&temp_packet->clntAddr, &packet->clntAddr, sizeof(SOCKADDR_IN));
				memcpy(temp_packet->data->arr, packet->data->arr, packet->dataLen);
				temp_packet->dataLen = packet->dataLen;
				temp_packet->hClntSock = packet->hClntSock;
				temp_packet->perIoData = packet->perIoData;

				packetBuffer.insert(std::map<_int64, PACKET_DATA*>::value_type(packetId, temp_packet));

			}
			receivePacketClear(packet);
			continue;
		}


		// packetBuffer���� ����
		if (find != packetBuffer.end()) {
			delete (*find).second->data->arr;
			delete (*find).second->data;
			delete (*find).second;
			packetBuffer.erase(find);
		}


		// �������� Ȯ�� 
		if (!(packet->data->arr[2] <= C_SOCKET_ERROR && packet->data->arr[2] >= 2)) {
			std::cout << "��Ŷ ����\n";
			continue;
		}


		// �������� ��Ŷ ó��
		Data_Header* header;
		header = (Data_Header*)packet->data->arr;

		// �켱ó�� ��Ŷ (���α׷� ����, ���Ͽ���)
		if (header->protocol == C_CLOSE || header->protocol == C_SOCKET_ERROR) {
			{
				std::lock_guard<std::mutex> lock(mutex_messageBuf);
				messageBuf.push_front(packet);
				cv_messageBuf.notify_all();
			}
		}
		else {
			{
				std::lock_guard<std::mutex> lock(mutex_messageBuf);
				messageBuf.push_back(packet);
				cv_messageBuf.notify_all();
			}
		}
	}
}

void TaskOperation::dbRun() {

}


// ������ ó�� ������
unsigned int __stdcall TaskOperation::controlThread(void* taskOperation) {
	TaskOperation* this_taskOperation = static_cast<TaskOperation*>(taskOperation);
	this_taskOperation->controlRun();
	return 0;
}

void TaskOperation::controlRun() {
	while (true) {
		PACKET_DATA* packet;
		{
			std::unique_lock<std::mutex> lock(mutex_messageBuf);
			while (messageBuf.empty()) {
				cv_messageBuf.wait(lock);
			}
			packet = messageBuf.front();
			messageBuf.pop_front();
		}

		if (messageBuf.size() > BUFFER_WARING) std::cout << "messageBuf warnig\n";

		Data_Header* header;
		header = (Data_Header*)packet->data->arr;
		std::string userid(header->id, USERID_LEN);


		if (header->protocol == C_CONNECT) {
			//DB���� id pass �� �ϴ� ���� �����ؾ���	
			mUSER* user = registeUser(userid, packet->hClntSock, packet->clntAddr, packet->perIoData);


			///��Ŷ ����
			WORD totalLen = header->totalLen + 1;
			char* data = new char[totalLen];
			headerSet(data, totalLen, C_CONNECT, (char*)userid.c_str());
			Data_Header* send_header = (Data_Header*)data;
			DATA_INFO* data_info = new DATA_INFO;
			data_info->arr = data;
			data_info->reference_count = 1;
			/////



			if (user != NULL) {
				data[20] = 1;

				PACKET_DATA* send_packet = makePacket(user->hClntSock, user->perIoData, data_info, send_header->totalLen);
				iocp->sendData(send_packet);
			}
			else {
				data[20] = 0;

				PACKET_DATA* send_packet = makePacket(packet->hClntSock, packet->perIoData, data_info, send_header->totalLen);
				iocp->sendData(send_packet);
			}

		}
		else if (header->protocol == C_CLOSE) {
			mUSER* user = findUser(userid);


			///��Ŷ ����
			WORD totalLen = header->totalLen + 1;
			char* data = new char[totalLen];
			headerSet(data, totalLen, C_CLOSE, (char*)userid.c_str());
			Data_Header* send_header = (Data_Header*)data;
			DATA_INFO* data_info = new DATA_INFO;
			data_info->arr = data;
			data_info->reference_count = 1;
			///////

			data[20] = removeUser(user);	



			PACKET_DATA* send_packet = makePacket(user->hClntSock, user->perIoData, data_info, send_header->totalLen);
			iocp->sendData(send_packet);
		}
		else if (header->protocol == C_ENTER_ROOM) {
			WORD* roomid = (WORD*)&packet->data->arr[20];
			mUSER* user = findUser(userid);
			bool isEnter;


			if (user != NULL) {
				if (addRoomUser(user, *roomid)) {
					isEnter = true;
				}
				else {
					isEnter = false;
				}
			}
			else {
				isEnter = false;
			}

			if (isEnter) {
				std::map<WORD, mROOM>::iterator now_room = roomList.find(user->roomId);
				std::list<mUSER*>::iterator itor = (*now_room).second.userList.begin();

				{
					
					///��Ŷ ����
					WORD totalLen = sizeof(Data_Header) + 3;
					char* data = new char[totalLen];
					headerSet(data, totalLen, C_ENTER_ROOM, (char*)userid.c_str());
					Data_Header* send_header = (Data_Header*)data;
					memcpy(&data[20], roomid, sizeof(WORD));
					data[22] = isEnter;
					int size = (*now_room).second.userList.size();
					DATA_INFO* data_info = new DATA_INFO;
					data_info->arr = data;
					data_info->reference_count = size;
					//////////


					//������ ����ڵ鿡�� �˸�
					for (; itor != (*now_room).second.userList.end(); itor++) {
						PACKET_DATA* send_packet = makePacket((*itor)->hClntSock, (*itor)->perIoData, data_info, send_header->totalLen);
						iocp->sendData(send_packet);
					}

				}
				{
					///��Ŷ ����
					WORD numberOfRoom = (*now_room).second.userList.size();
					WORD totalLen = sizeof(Data_Header) + (USERID_LEN* numberOfRoom) + 2;
					char* data = new char[totalLen];
					headerSet(data, totalLen, C_ROOMINFO, (char*)userid.c_str());
					Data_Header* send_header = (Data_Header*)data;
					memcpy(&data[20], &numberOfRoom, sizeof(WORD));
					DATA_INFO* data_info = new DATA_INFO;
					data_info->arr = data;
					data_info->reference_count = 1;
					///////////////


					//������ �����ϴ� ����� ����
					itor = (*now_room).second.userList.begin();
					int n = 0;
					for (; itor != (*now_room).second.userList.end(); itor++) {
						memcpy(&data[sizeof(Data_Header) + 2 + (n*USERID_LEN)], (*itor)->id, USERID_LEN);
						n++;
					}
					

					PACKET_DATA* send_packet = makePacket(user->hClntSock, user->perIoData, data_info, send_header->totalLen);
					iocp->sendData(send_packet);

				}
			}
			else {

				///��Ŷ ����
				WORD totalLen = sizeof(Data_Header);
				char* data = new char[totalLen];
				headerSet(data, totalLen, C_ENTER_ROOM, (char*)userid.c_str());
				Data_Header* send_header = (Data_Header*)data;
				DATA_INFO* data_info = new DATA_INFO;
				data_info->arr = data;
				data_info->reference_count = 1;
				///////////


				memcpy(&data[20], roomid, sizeof(WORD));
				data[22] = isEnter;


				

				PACKET_DATA* send_packet = makePacket(user->hClntSock, user->perIoData, data_info, send_header->totalLen);
				iocp->sendData(send_packet);

			}

		}
		else if (header->protocol == C_EXIT_ROOM) {
			mUSER* user = findUser(userid);
			bool isExit;
			int roomid = user->roomId;
			isExit = eixtUser(user);

			if (isExit) {
				std::map<WORD, mROOM>::iterator now_room = roomList.find(roomid);

				if ((*now_room).second.userList.size() > 0) {
					std::list<mUSER*>::iterator itor = (*now_room).second.userList.begin();

					////��Ŷ ����
					WORD totalLen = sizeof(Data_Header) + 1;
					char* data = new char[totalLen];
					headerSet(data, totalLen, C_EXIT_ROOM, (char*)userid.c_str());
					Data_Header* send_header = (Data_Header*)data;
					DATA_INFO* data_info = new DATA_INFO;					
					int size = (*now_room).second.userList.size();
					data_info->arr = data;
					data_info->reference_count = size;
					///////////


					data[20] = isExit;
					

					for (; itor != (*now_room).second.userList.end(); itor++) {
						PACKET_DATA* send_packet = makePacket((*itor)->hClntSock, (*itor)->perIoData, data_info, send_header->totalLen);
						iocp->sendData(send_packet);
					}
				}
			}
			////��Ŷ ����
			WORD totalLen = header->totalLen + 1;
			char* data = new char[totalLen];
			headerSet(data, totalLen, C_EXIT_ROOM, (char*)userid.c_str());
			Data_Header* send_header = (Data_Header*)data;
			DATA_INFO* data_info = new DATA_INFO;
			data_info->arr = data;
			data_info->reference_count = 1;
			//////////

			data[20] = isExit;		


			PACKET_DATA* send_packet = makePacket(user->hClntSock, user->perIoData, data_info, send_header->totalLen);
			iocp->sendData(send_packet);

		}
		else if (header->protocol == C_SOCKET_ERROR) {
			mUSER* socket_error_user = findUserId(packet->clntAddr);
			char uid[USERID_LEN];

			/// ����� ���� �ʱ�ȭ
			if (socket_error_user == NULL)continue;
			memcpy(uid, socket_error_user->id, USERID_LEN);

			if (socket_error_user == NULL) continue;
			int roomid = socket_error_user->roomId;

			eixtUser(socket_error_user);
			removeUser(socket_error_user);
		}
		else if (header->protocol == C_MESSAGE) {
			WORD* message_len = (WORD*)&packet->data->arr[20];

			mUSER* user = findUser(userid);

			if (user == NULL) {
				receivePacketClear(packet);
				continue;
			}
			std::map<WORD, mROOM>::iterator now_room = roomList.find(user->roomId);
			std::list<mUSER*>::iterator itor = (*now_room).second.userList.begin();


			///��Ŷ ����
			WORD totalLen = *message_len + sizeof(Data_Header) + 2;
			char* data = new char[totalLen];
			headerSet(data, totalLen, C_MESSAGE, (char*)userid.c_str());
			Data_Header* send_header = (Data_Header*)data;
			int size = (*now_room).second.userList.size();
			DATA_INFO* data_info = new DATA_INFO;
			data_info->arr = data;
			data_info->reference_count = size;
			////////////


			memcpy(&data[20], message_len, sizeof(WORD));
			memcpy(&data[22], &packet->data->arr[22], *message_len);
			

			for (; itor != (*now_room).second.userList.end(); itor++) {
				PACKET_DATA* send_packet = makePacket((*itor)->hClntSock, (*itor)->perIoData, data_info, send_header->totalLen);
				iocp->sendData(send_packet);
			}

		}
		else {
			std::cout << "????? warning\n";
		}


		receivePacketClear(packet);
	}
}




//iocp���� ���Ź��� ��Ŷ�� ���ť�� ����
void TaskOperation::receivePacket(PACKET_DATA* data) {
	{
		std::lock_guard<std::mutex> lock(mutex_receiveBuf);
		receiveBuf.push(data);
		cv_receiveBuf.notify_all();
	}
}

//���Ͻ������ �۵��ؼ� ó���ϴ� ���� ��Ŷ�� lock�� �� ������ ����
void TaskOperation::receivePacketClear(PACKET_DATA*  packet) {

	if (packet != NULL) {
		if (packet->data->arr != NULL) {
			packet->data->reference_count -= 1;
			if ((packet->data->reference_count <= 0)) {
				delete packet->data->arr;
				delete packet->data;
			}
		}
		delete packet;
	}
}

//�������� 0 �̸� ��Ŷ ����
void TaskOperation::sendPacketClear(PACKET_DATA*  packet) {

	{
		std::unique_lock<std::mutex> lock(m_Mutex);
		if (packet != NULL) {
			if (packet->data->arr != NULL) {
				packet->data->reference_count -= 1; // ������ ����
				if (packet->data->reference_count <= 0) {
					delete packet->data->arr;
					delete packet->data;
				}

			}
			delete packet;

		}
	}
}

//��Ŷ ��� ����
void TaskOperation::headerSet(char* data, WORD totalLen, WORD protocol, char* id) {
	Data_Header* header = (Data_Header*)data;
	memcpy(&data[4], id, USERID_LEN);
	header->totalLen = totalLen;
	header->protocol = protocol;
}

// ��Ŷ ����
PACKET_DATA* TaskOperation::makePacket(SOCKET hClntSock, PER_IO_DATA* perIoData, DATA_INFO* data, WORD dataLen) {
	PACKET_DATA* send_packet = new PACKET_DATA;
	send_packet->perIoData = perIoData;
	send_packet->hClntSock = hClntSock;
	send_packet->data = data;
	send_packet->dataLen = dataLen;

	return send_packet;
}

// ����� ���
mUSER* TaskOperation::registeUser(std::string id, SOCKET hClntSock, SOCKADDR_IN addr, PER_IO_DATA* perIoData) {
	std::map<std::string, mUSER*>::iterator itor = userList.find(id);

	if (itor == userList.end()) {
		mUSER* t_user = new mUSER;
		t_user->perIoData = new PER_IO_DATA;

		memset(t_user->id, 0, USERID_LEN);
		memcpy(t_user->id, id.c_str(), id.size());
		t_user->hClntSock = hClntSock;
		memcpy(&t_user->addr, &addr, sizeof(SOCKADDR_IN));
		memcpy(t_user->perIoData, perIoData, sizeof(PER_IO_DATA));
		t_user->perIoData->rwMode = WRITE;
		t_user->roomId = -1;
		t_user->status = IN_LOBBY;

		userList.insert(std::map<std::string, mUSER*>::value_type(id, t_user));
	}
	else {
		return NULL;
	}
	itor = userList.find(id);

	return (mUSER*)(*itor).second;
}

// ip, port�� ����� �˻�
mUSER* TaskOperation::findUserId(SOCKADDR_IN addr) {
	std::map<std::string, mUSER*>::iterator itor = userList.begin();

	for (; itor != userList.end(); itor++) {
		mUSER* temp_user = (*itor).second;

		if (addr.sin_addr.s_addr == temp_user->addr.sin_addr.s_addr && addr.sin_port == temp_user->addr.sin_port) {
			return temp_user;
		}
	}
	return NULL;
}

// id�� ����� �˻�
mUSER* TaskOperation::findUser(std::string id) {
	std::map<std::string, mUSER*>::iterator itor = userList.find(id);

	if (itor != userList.end()) {
		return (mUSER*)(*itor).second;
	}
	else {
		return NULL;
	}

}

// ä�ù� ��ȣ�� äƼ�� �˻�
mROOM* TaskOperation::findRoom(WORD id) {
	std::map<WORD, mROOM>::iterator itor = roomList.find(id);

	if (itor != roomList.end()) {
		return (mROOM*)&(*itor).second;
	}
	else {
		return NULL;
	}
}


//����� ����
bool TaskOperation::removeUser(mUSER* user) {
	if (user->roomId >= 0) return false;
	std::string userid(user->id, USERID_LEN);
	userList.erase(userid);

	if (user != NULL) {
		if (user->perIoData != NULL) {
			delete user->perIoData;
		}
		delete user;
	}
	return true;
}

// ����� ä�ù� ����
bool TaskOperation::eixtUser(mUSER* user) {
	if (user->roomId < 0) return false;

	mROOM* room = findRoom(user->roomId);
	if (room == NULL) return false;

	room->userList.remove(user);
	user->roomId = -1;

	return true;

}

// ����� ä�ù� ����
bool  TaskOperation::addRoomUser(mUSER* user, WORD roomid) {
	mROOM* room = findRoom(roomid);

	if (room != NULL) {
		user->roomId = roomid;
		room->userList.push_back(user);
		return true;
	}
	else {
		return false;
	}

}