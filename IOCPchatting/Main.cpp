#pragma comment (lib,"ws2_32.lib")
#include "stdafx.h"
#include "IOCPServer.h"
#include "TaskOperation.h"
int main()
{
	int a = sizeof(C_RECEIVE_ROOMINFO_Header);
	TaskOperation* t = new TaskOperation();
	IOCPServer* s = new IOCPServer(t);
	s->Init();
	t->setIOCP(s);


	t->Run();
	s->Run();

	delete s;
	delete t;
	return 0;
}