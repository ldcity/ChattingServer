#include "PCH.h"
#include "ThreadWorker.h"

bool ThreadWorker::StartThread(unsigned(__stdcall* threadFunc)(void*), LPVOID param)
{
    _eventHandle = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (_eventHandle == NULL)
    {
        return false;
    }

    _threadHandle = (HANDLE)_beginthreadex(NULL, 0, threadFunc, param, 0, NULL);
    if (_threadHandle == NULL)
    {
        return false;
    }

    return true;
}

void ThreadWorker::StopThread()
{
    _runFlag = false;

    if (_threadHandle)
    {
        WaitForSingleObject(_threadHandle, INFINITE);

        if (_threadHandle)
            CloseHandle(_threadHandle);

        if (_eventHandle)
            CloseHandle(_eventHandle);
    }
}