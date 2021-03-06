
#include "stdafx.h"
#include "IOCPServer.h"
#include "TaskOperation.h"
#include "IOCPCallback.h"

using namespace std;

int main()
{

	TaskOperation task;
	IOCPServer *s = new IOCPServer(task);
	s->Init();

	task.Init(*s);

	task.Run();
	s->Run();

	string str;
	while (true) {
		Sleep(1000);
		cout << "sendQue: " << s->getSendQueueSize() << endl;
		cout << "messageQue: " << task.getMessageBufSize() << endl;
		cout << "userSize: " << task.getUserTableSize() << endl;
		cout << "\n\n" << endl;
	}

	return 0;
}