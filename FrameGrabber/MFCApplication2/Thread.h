#pragma once
#include <windows.h>

class Thread
{
public:
    Thread();
    virtual ~Thread();

    bool isRunning() const { return running;  }
    bool start();
    bool wait(DWORD timeout_ms);

protected:
    volatile bool running;
    DWORD threadId; HANDLE threadHandle;
    static DWORD WINAPI threadRoutine(LPVOID param);
    virtual void threadFunc() = 0;
};

class Mutex
{
public:
    Mutex();
    ~Mutex();

    bool lock(DWORD timeout_ms = INFINITE);
    void unlock();

private:
    HANDLE h;
};