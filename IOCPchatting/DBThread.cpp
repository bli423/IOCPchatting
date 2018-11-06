#include "stdafx.h"
#include "DBThread.h"


DBThread::DBThread()
{
}


DBThread::~DBThread()
{
	mysql_close(m_ConnectionRequest);
	mysql_close(m_ConnectionWrite);
}

void DBThread::Init(TaskOperation& _taskOperation)
{
	m_TaskOperation = &_taskOperation;

	mysql_init(&m_MysqlRequest);
	mysql_init(&m_MysqlRequestWrite);

	m_ConnectionRequest = mysql_real_connect(&m_MysqlRequest, "127.0.0.1", "nam", "7989", "Message", 3306, NULL, 0);
	m_ConnectionWrite = mysql_real_connect(&m_MysqlRequestWrite, "127.0.0.1", "nam2", "7989", "Message", 3306, NULL, 0);

	if (m_ConnectionRequest == NULL) {
		std::cout << "db connectionRequest error\n";
	}
	if (m_ConnectionWrite == NULL) {
		std::cout << "db connectionWrite error\n";
	}

}

void DBThread::Run() {

	_beginthreadex(NULL, 0, singRequestThread, (void*)this, 0, NULL);
	_beginthreadex(NULL, 0, messageWriteThread, (void*)this, 0, NULL);
}

unsigned int __stdcall DBThread::singRequestThread(void* dbThread) {
	DBThread* this_dbThread = static_cast<DBThread*>(dbThread);
	this_dbThread->singRequestRun();
	std::cout << " singRequestRun " << std::endl;
	return 0;
}

void DBThread::singRequestRun() {
	while (true) {
		REQUEST_DATA* request;

		{
			std::unique_lock<std::mutex> lock(m_Mutex_SingRequestBuf);
			while (m_SingRequestBuf.empty()) {
				m_CV_SingRequestBuf.wait(lock);
			}
			request = m_SingRequestBuf.front();
			m_SingRequestBuf.pop();
		}

		int query_stat = mysql_query(m_ConnectionRequest, request->data->data());

		if (query_stat != 0) {
			std::cout << "Mysql connectionRequest query error \n";
		}
		else {
			int data_len = 0;
			MYSQL_RES* result = mysql_store_result(m_ConnectionRequest);
			MYSQL_ROW result_row;

			std::string* log = new std::string;

			if (result->row_count == 0) {
				log->append("there is no log");
				data_len = log->size();
			}

			while ((result_row = mysql_fetch_row(result)) != NULL)
			{
				if (data_len > 60000) break; //길이제한 -길이 제한을 올릴려면 패킷 구조를 바꿔야한다

				for (int i = 0; i < result->field_count; i++) {
					log->append(result_row[i]);
					log->append(" ");
				}
				log->append("\n");
				data_len = log->size();
			}

			m_TaskOperation->completeDBJob(*request->user_id, (char*)log->c_str(), data_len);
			delete log;
			mysql_free_result(result);
		}

		delete request->data;
		delete request;

	}
}

unsigned int __stdcall DBThread::messageWriteThread(void* dbThread) {
	DBThread* this_dbThread = static_cast<DBThread*>(dbThread);
	this_dbThread->messageWriteRun();
	std::cout << " messageWriteRun " << std::endl;
	return 0;
}
void DBThread::messageWriteRun() {

	while (true) {

		std::string* requset = new std::string("insert into messageLog( date , uid , roomid , message) VALUES");

		{
			std::unique_lock<std::mutex> lock(m_Mutex_messageWriteBuf);
			while (m_MessageWriteBuf.size() < 0) {
				m_CV_messageWriteBuf.wait(lock);
			}

			MESSAGELOG* message_log;
			while (!m_MessageWriteBuf.empty()) {
				message_log = m_MessageWriteBuf.front();
				m_MessageWriteBuf.pop();

				string sDate = std::to_string(message_log->date);	
				string sRoom_id = std::to_string(message_log->room_id);

				*requset += string("(");
				*requset += (sDate);
				*requset += (string(", \'"));
				*requset += ((message_log->user_id).data());
				*requset += (string("\', "));
				*requset += (sRoom_id);
				*requset += (string(", \'"));
				*requset += (message_log->message);
				*requset += (string("\'),"));
		
				delete message_log;
			}
		}

		requset->pop_back();
		int query_stat = mysql_query(m_ConnectionWrite, requset->data());

		if (query_stat != 0) {
			//std::cout << "Mysql connectionWrite query error \n";
		}
		delete requset;
	}
}



void DBThread::addMessage(string& _userID, int _roomID, string& message) {

	MESSAGELOG* message_log = new MESSAGELOG;

	message_log->date = getNowTime();
	message_log->user_id = _userID;
	message_log->room_id = _roomID;
	message_log->message = message;

	delete &message;
	{
		std::unique_lock<std::mutex> lock(m_Mutex_messageWriteBuf);
		m_MessageWriteBuf.push(message_log);
		m_CV_messageWriteBuf.notify_one();
	}

	
}

void DBThread::getRoomLog(string& _reqestUserID, int _roomID) {
	REQUEST_DATA*  request_data = new REQUEST_DATA;

	std::string* request = new std::string("SELECT * FROM messagelog WHERE roomid=");
	request->append(std::to_string(_roomID));
	request_data->user_id = new string(_reqestUserID);
	request_data->data = request;

	{
		std::lock_guard<std::mutex> lock(m_Mutex_SingRequestBuf);
		m_SingRequestBuf.push(request_data);
		m_CV_SingRequestBuf.notify_one();
	}

}



time_t DBThread::getNowTime() {
	time_t now_time;

	std::chrono::system_clock::time_point tp = std::chrono::system_clock::now();
	std::chrono::duration<double> temp_time = tp.time_since_epoch();
	now_time = temp_time.count();

	return now_time;
}