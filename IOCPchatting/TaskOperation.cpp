#include "stdafx.h"
#include "TaskOperation.h"



TaskOperation::TaskOperation()
{
	for (int i = 0; i < ROOM_COUNT; i++) {
		room[i].rommId = i;
		roomList[room[i].rommId] = room[i];
	}
}

TaskOperation::~TaskOperation()
{
	delete dbThread;
}

void TaskOperation::setIOCP(IOCPServer* iocp) {
	this->iocp = iocp;
}


void TaskOperation::Run() {
	_beginthreadex(NULL, 0, receiveThread, (void*)this, 0, NULL);
	_beginthreadex(NULL, 0, controlThread, (void*)this, 0, NULL);

	dbThread = new DBThread(this);
	dbThread->Run();

	
}


// 패킷의 길이 검사하는 스래드
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


		//// 패킷 검사
		/// 잘려질수있는 패킷을 검사

		_int64 packetId = packet->clntAddr.sin_addr.s_addr;
		packetId = packetId << (sizeof(packet->clntAddr.sin_port) * 8);
		packetId = packetId | packet->clntAddr.sin_port;

		std::map<_int64, PACKET_DATA*>::iterator find = packetBuffer.find(packetId);

		// 기존에 잘려진 패킷 조사
		if (find != packetBuffer.end()) {
			PACKET_DATA* temp_packet = (PACKET_DATA*)(*find).second;
			int merge_data_len = temp_packet->data->dataLen + packet->data->dataLen;
			char* merge_data = new char[merge_data_len];

			memcpy(merge_data, temp_packet->data->arr, temp_packet->data->dataLen);
			memcpy(&merge_data[temp_packet->data->dataLen], packet->data->arr, packet->data->dataLen);

			delete packet->data->arr;


			packet->data->arr = merge_data;
			packet->data->dataLen = merge_data_len;
		}

		WORD* packet_tLen = (WORD*)packet->data->arr;

		// 패킷 길이 조사
		if (*packet_tLen > packet->data->dataLen) {
			
			//길이가 짧으면 패킷을 합치고 다시 MAP에 저장
			if (find != packetBuffer.end()) {
				char* temp_data = new char[packet->data->dataLen];
				memcpy(temp_data, packet->data->arr, packet->data->dataLen);

				delete (*find).second->data->arr;

				(*find).second->data->arr = temp_data;
				(*find).second->data->dataLen = packet->data->dataLen;

			}
			else {
				PACKET_DATA* temp_packet = new PACKET_DATA;
				DATA_INFO* data_info = new DATA_INFO;
				temp_packet->data = data_info;

				temp_packet->data->arr = new char[packet->data->dataLen];
				temp_packet->data->reference_count = packet->data->reference_count;
				memcpy(&temp_packet->clntAddr, &packet->clntAddr, sizeof(SOCKADDR_IN));
				memcpy(temp_packet->data->arr, packet->data->arr, packet->data->dataLen);
				temp_packet->data->dataLen = packet->data->dataLen;
				temp_packet->hClntSock = packet->hClntSock;
				temp_packet->perIoData = packet->perIoData;

				packetBuffer.insert(std::map<_int64, PACKET_DATA*>::value_type(packetId, temp_packet));

			}
			receivePacketClear(packet);
			continue;
		}


		// packetBuffer에서 제거
		if (find != packetBuffer.end()) {
			delete (*find).second->data->arr;
			delete (*find).second->data;
			delete (*find).second;
			packetBuffer.erase(find);
		}


		// 프로토콜 확인 
		if (!(packet->data->arr[2] <= C_REQUST_ROOMINFO && packet->data->arr[2] >= 2)) {
			std::cout << "패킷 꼬임\n";
			continue;
		}


		// 정상적이 패킷 처리
		Data_Header* header;
		header = (Data_Header*)packet->data->arr;

		// 우선처리 패킷 (프로그램 종료, 소켓에러)
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



// 데이터 처리 스레드
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
			//DB에서 id pass 비교 하는 로직 구현해야함	
			mUSER* user = registeUser(userid, packet->hClntSock, packet->clntAddr, packet->perIoData);


			///패킷 설정
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


			///패킷 설정
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
					
					///패킷 설정
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


					//같은방 사용자들에게 알림
					for (; itor != (*now_room).second.userList.end(); itor++) {
						PACKET_DATA* send_packet = makePacket((*itor)->hClntSock, (*itor)->perIoData, data_info, send_header->totalLen);
						iocp->sendData(send_packet);
					}

				}
				{
					///패킷 설정
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


					//기존에 존재하는 사용자 정보
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

				///패킷 설정
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

					////패킷 설정
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
			////패킷 설정
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

			/// 사용자 정보 초기화
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



			///패킷 설정
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
			
			//DB에 메세지 저장
			dbThread->addMessage((char*)userid.c_str(), user->roomId, std::string(&packet->data->arr[22], *message_len));

			for (; itor != (*now_room).second.userList.end(); itor++) {
				PACKET_DATA* send_packet = makePacket((*itor)->hClntSock, (*itor)->perIoData, data_info, send_header->totalLen);
				iocp->sendData(send_packet);
			}

		}
		else if (header->protocol == C_REQUST_ROOMINFO) {
			WORD* roomid = (WORD*)&packet->data->arr[20];

			dbThread->getRoomLog((char*)userid.c_str(),*roomid);
		}
		else if (header->protocol == C_RECEIVE_ROOMINFO) {
			WORD* message_len = (WORD*)&packet->data->arr[20];
			mUSER* user = findUser(userid);

			WORD totalLen = *message_len + sizeof(Data_Header) + 2;
			char* data = new char[totalLen];
			headerSet(data, totalLen, C_RECEIVE_ROOMINFO, (char*)userid.c_str());
			Data_Header* send_header = (Data_Header*)data;			
			DATA_INFO* data_info = new DATA_INFO;
			data_info->arr = data;
			data_info->reference_count = 1;

			memcpy(&data[20], message_len, sizeof(WORD));
			memcpy(&data[22], &packet->data->arr[22], *message_len);


			PACKET_DATA* send_packet = makePacket(user->hClntSock, user->perIoData, data_info, send_header->totalLen);
			iocp->sendData(send_packet);
		}
		else {
			std::cout << "????? warning\n";
		}


		receivePacketClear(packet);
	}
}




//iocp에서 수신받은 패킷을 대기큐에 저장
void TaskOperation::receivePacket(PACKET_DATA* data) {
	{
		std::lock_guard<std::mutex> lock(mutex_receiveBuf);
		receiveBuf.push(data);
		cv_receiveBuf.notify_all();
	}
}

//단일스레드로 작동해서 처리하는 수신 패킷을 lock를 할 이유가 없음
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

//참조값이 0 이면 패킷 삭제
void TaskOperation::sendPacketClear(PACKET_DATA*  packet) {

	{
		std::lock_guard<std::mutex> lock(m_Mutex);
		if (packet != NULL) {
			if (packet->data->arr != NULL) {
				packet->data->reference_count -= 1; // 참조값 감소
				if (packet->data->reference_count <= 0) {
					delete packet->data->arr;
					delete packet->data;
				}

			}
			delete packet;

		}
	}
}

//패킷 헤더 설정
void TaskOperation::headerSet(char* data, WORD totalLen, WORD protocol, char* id) {
	Data_Header* header = (Data_Header*)data;
	memcpy(&data[4], id, USERID_LEN);
	header->totalLen = totalLen;
	header->protocol = protocol;
}

// 패킷 설정
PACKET_DATA* TaskOperation::makePacket(SOCKET hClntSock, PER_IO_DATA* perIoData, DATA_INFO* data, WORD dataLen) {
	PACKET_DATA* send_packet = new PACKET_DATA;
	send_packet->perIoData = perIoData;
	send_packet->hClntSock = hClntSock;
	send_packet->data = data;
	send_packet->data->dataLen = dataLen;

	return send_packet;
}

// 사용자 등록
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

// ip, port로 사용자 검색
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

// id로 사용자 검색
mUSER* TaskOperation::findUser(std::string id) {
	std::map<std::string, mUSER*>::iterator itor = userList.find(id);

	if (itor != userList.end()) {
		return (mUSER*)(*itor).second;
	}
	else {
		return NULL;
	}

}

// 채팅방 번호로 채티방 검색
mROOM* TaskOperation::findRoom(WORD id) {
	std::map<WORD, mROOM>::iterator itor = roomList.find(id);

	if (itor != roomList.end()) {
		return (mROOM*)&(*itor).second;
	}
	else {
		return NULL;
	}
}


//사용자 제거
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

// 사용자 채팅방 나감
bool TaskOperation::eixtUser(mUSER* user) {
	if (user->roomId < 0) return false;

	mROOM* room = findRoom(user->roomId);
	if (room == NULL) return false;

	room->userList.remove(user);
	user->roomId = -1;

	return true;

}

// 사용자 채팅방 입장
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


void TaskOperation::sendClearJob(char* requst_id, char* data, DWORD data_len) {
	char* send_data = new char[sizeof(Data_Header) +2+ data_len];

	memcpy(&send_data[sizeof(Data_Header)], &data_len, 2);
	memcpy(&send_data[sizeof(Data_Header) + 2], data, data_len);

	PACKET_DATA* packet = new PACKET_DATA;
	DATA_INFO* data_info = new DATA_INFO;
	packet->data = data_info;
	Data_Header* header;
	header = (Data_Header*)send_data;
	header->protocol = C_RECEIVE_ROOMINFO;
	memcpy(header->id, requst_id,USERID_LEN);
	data_info->dataLen = sizeof(Data_Header) + 2 + data_len;
	data_info->arr = send_data;
	data_info->reference_count = 1;

	{
		std::lock_guard<std::mutex> lock(mutex_messageBuf);
		messageBuf.push_back(packet);
		cv_messageBuf.notify_all();
	}
}