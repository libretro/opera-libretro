#ifndef __4DO_WORKER_HEADER
#define __4DO_WORKER_HEADER

#include <windows.h>

class Worker
{
public:
	Worker(void (*workerFunction)(void*), void* workerFunctionArgument);
	~Worker();

	void Run();
	void Wait();

	void _ThreadFunction();

private:
	HANDLE threadHandle;
	unsigned int threadId;
	HANDLE startEvent;
	void (*internalWorkerFunction)(void*);
	void* internalWorkerFunctionArgument;
};

#endif 