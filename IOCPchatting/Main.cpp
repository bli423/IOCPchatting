
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
		getline(cin, str);
	}
	return 0;
}