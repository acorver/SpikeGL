#include "stdafx.h"
#include "Thread.h"


Thread::Thread()
    : running(false), threadId(0), threadHandle(0)
{
}


Thread::~Thread()
{
    if (threadHandle) CloseHandle(threadHandle);
    threadHandle = 0; threadId = 0;
}

bool Thread::wait(DWORD timeout)
{
    if (threadHandle && running) {
        DWORD r = WaitForSingleObject(threadHandle, timeout);
        return r == WAIT_OBJECT_0;
    }
    return true;
}

void Thread::kill()
{
    if (threadHandle && running) {
        if (TerminateThread(threadHandle, -1)) {
            running = false;
        }
    }
}

DWORD WINAPI Thread::threadRoutine(LPVOID param)
{
    Thread *thiz = (Thread *)param;
    thiz->running = true;
    thiz->threadFunc();
    thiz->running = false;
    return 0;
}

bool Thread::start()
{
    if (running) return false;

    threadHandle = 
        CreateThread(
        NULL,       // default security attributes
        0,          // default stack size
        (LPTHREAD_START_ROUTINE)threadRoutine,
        this,       // thread function argument
        0,          // default creation flags
        &threadId); // receive thread identifier
    return !!threadHandle;
}

Mutex::Mutex()
{
    h = CreateMutex(NULL, FALSE, NULL);
}

Mutex::~Mutex()
{
    CloseHandle(h);
}

bool Mutex::lock(DWORD timeout)
{
    DWORD res = WaitForSingleObject(h, timeout);
    return (res == WAIT_OBJECT_0);
}

void Mutex::unlock()
{
    ReleaseMutex(h);
}