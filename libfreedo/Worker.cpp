#include "Worker.h"
#include <windows.h>
#include <process.h>

unsigned int __stdcall ThreadEntry(void* classInstance);

Worker::Worker(void (*workerFunction)(void*), void* workerFunctionArgument)
{
	this->startEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	this->threadHandle  = (HANDLE)_beginthreadex( NULL, 0, ThreadEntry, this, 0, &this->threadId );
	this->internalWorkerFunction = workerFunction;
	this->internalWorkerFunctionArgument = workerFunctionArgument;
}

Worker::~Worker()
{
}

void Worker::Run()
{
	SetEvent(this->startEvent);
}

void Worker::Wait()
{
	WaitForSingleObject(this->threadHandle, INFINITE);
}

////////////////////////////

void Worker::_ThreadFunction()
{
	// Wait until we're told to start.
	WaitForSingleObject(this->startEvent, INFINITE);

	this->internalWorkerFunction(this->internalWorkerFunctionArgument);
}

unsigned int __stdcall ThreadEntry(void* classInstance)
{
	((Worker*)classInstance)->_ThreadFunction();
	return 0;
}