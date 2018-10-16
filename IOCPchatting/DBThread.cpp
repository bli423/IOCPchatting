#include "stdafx.h"
#include "DBThread.h"


DBThread::DBThread(TaskOperation* taskOperation)
{
	this->taskOperation = taskOperation;
}


DBThread::~DBThread()
{
	mysql_close(&mysqlRequest);
	mysql_close(&mysqlRequestWrite);
}


void DBThread::Run() {
	connectionRequest;
	connectionWrite;

	mysql_init(&mysqlRequest);
	mysql_init(&mysqlRequestWrite);

	connectionRequest = mysql_real_connect(&mysqlRequest, "127.0.0.1", "nam", "7989", "Message", 3306, NULL, 0);
	connectionWrite = mysql_real_connect(&mysqlRequestWrite, "127.0.0.1", "nam2", "7989", "Message", 3306, NULL, 0);

	if (connectionRequest == NULL) {
		std::cout << "db connectionRequest error\n";
	}
	if (connectionWrite == NULL) {
		std::cout << "db connectionWrite error\n";
	}

	_beginthreadex(NULL, 0, singRequestThread, (void*)this, 0, NULL);
	_beginthreadex(NULL, 0, messageWriteThread, (void*)this, 0, NULL);
}

unsigned int __stdcall DBThread::singRequestThread(void* dbThread) {
	DBThread* this_dbThread = static_cast<DBThread*>(dbThread);
	this_dbThread->singRequestRun();
	return 0;
}

void DBThread::singRequestRun() {
	while (true) {
		REQUEST_DATA* request;

		{
			std::unique_lock<std::mutex> lock(mutex_singRequestBuf);
			while (singRequestBuf.empty()) {
				cv_singRequestBuf.wait(lock);
			}
			request = singRequestBuf.front();
			singRequestBuf.pop();
		}
		
		int query_stat = mysql_query(connectionRequest, request->data->data());

		if (query_stat != 0) {
			std::cout << "Mysql connectionRequest query error \n";
		}
		else {
			int data_len = 0;
			MYSQL_RES* result = mysql_store_result(connectionRequest);
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

			taskOperation->completeDBJob((char*)request->user_id, (char*)log->c_str(),data_len);
			delete log;
			mysql_free_result(result);
		}
		
		//delete request->data;
		delete request;
		
	}
}

unsigned int __stdcall DBThread::messageWriteThread(void* dbThread) {
	DBThread* this_dbThread = static_cast<DBThread*>(dbThread);
	this_dbThread->messageWriteRun();
	return 0;
}
void DBThread::messageWriteRun() {
	while (true) {
				
		std::string* requset = new std::string("insert into messageLog( date , uid , roomid , message) VALUES");

		{
			std::unique_lock<std::mutex> lock(mutex_messageWriteBuf);
			while (messageWriteBuf.size()==0) {
				cv_messageWriteBuf.wait(lock);
			}

			MESSAGELOG* message_log;
			while (!messageWriteBuf.empty()) {
				message_log = messageWriteBuf.front();
				messageWriteBuf.pop();

				std::string sDate = std::to_string(message_log->date);
				std::string sUser_id((char*)message_log->user_id, USERID_LEN);
				std::string sRoom_id = std::to_string(message_log->room_id);

				requset->append(std::string("("));
				requset->append(sDate);
				requset->append(std::string(", \'"));
				requset->append(sUser_id.data());
				requset->append(std::string("\', "));
				requset->append(sRoom_id);
				requset->append(std::string(", \'"));
				requset->append(message_log->message->data());
				requset->append(std::string("\'),"));

				delete message_log->message;				
				delete message_log;				
			}
		}
		
		requset->pop_back();
		int query_stat = mysql_query(connectionWrite, requset->data());

		if (query_stat != 0){
			std::cout << "Mysql connectionWrite query error \n";
		}


		delete requset;
	}
}



void DBThread::addMessage(char* user_id, int room_id, std::string message) {

	MESSAGELOG* message_log = new MESSAGELOG;
	message_log->message = new std::string;

	message_log->date = get_now_time();
	memcpy(message_log->user_id, user_id, USERID_LEN);
	message_log->room_id = room_id;	
	message_log->message->append(message);
	
	{
		std::unique_lock<std::mutex> lock(mutex_messageWriteBuf);
		messageWriteBuf.push(message_log);
		cv_messageWriteBuf.notify_all();
	}
}

void DBThread::getRoomLog(char* reqest_user_id, int room_id){
	REQUEST_DATA*  request_data = new REQUEST_DATA;

	std::string* request = new std::string("SELECT * FROM messagelog WHERE roomid=");
	request->append(std::to_string(room_id));

	memcpy(request_data->user_id, reqest_user_id, USERID_LEN);
	request_data->data = request;
	{
		std::lock_guard<std::mutex> lock(mutex_singRequestBuf);
		singRequestBuf.push(request_data);
		cv_singRequestBuf.notify_all();
	}
	
}
void DBThread::getUserLog(char* reqest_user_id, char* user_id) {
	REQUEST_DATA*  request_data = new REQUEST_DATA;

	std::string* request = new std::string("SELECT * FROM messagelog WHERE uid=" );
	request->append(std::string(user_id).data());

	memcpy(request_data->user_id,reqest_user_id,USERID_LEN);
	request_data->data = request;

	{
		std::lock_guard<std::mutex> lock(mutex_singRequestBuf);
		singRequestBuf.push(request_data);
		cv_singRequestBuf.notify_all();
	}
}


time_t DBThread::get_now_time() {
	time_t now_time;

	std::chrono::system_clock::time_point tp = std::chrono::system_clock::now();
	std::chrono::duration<double> temp_time = tp.time_since_epoch();
	now_time = temp_time.count() * 1000;

	return now_time;
}