#include "PCH.h"
#include "ThreadWorker.h"

bool ThreadWorker::StartThread(unsigned(__stdcall* threadFunc)(void*), LPVOID param)
{
    mEventHandle = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (mEventHandle == NULL)
    {
        return false;
    }

    mThreadHandle = (HANDLE)_beginthreadex(NULL, 0, threadFunc, param, 0, NULL);
    if (mThreadHandle == NULL)
    {
        return false;
    }

    return true;
}

void ThreadWorker::StopThread()
{
    mRunFlag = false;

    if (mThreadHandle)
    {
        WaitForSingleObject(mThreadHandle, INFINITE);

        if (mThreadHandle)
            CloseHandle(mThreadHandle);

        if (mEventHandle)
            CloseHandle(mEventHandle);
    }
}