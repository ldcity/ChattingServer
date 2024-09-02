#ifndef __THREAD_WORKER_CLASS__
#define __THREAD_WORKER_CLASS__

#include <process.h>
#include <Windows.h>

class ThreadWorker
{
protected:
    HANDLE mThreadHandle;
    HANDLE mEventHandle;
    bool mRunFlag;

public:
    ThreadWorker() : mRunFlag(true), mThreadHandle(nullptr), mEventHandle(nullptr),
        mJobThreadUpdateCnt(0) {}

    virtual ~ThreadWorker()
    {
        StopThread();
    }

    bool StartThread(unsigned(__stdcall* threadFunc)(void*), LPVOID param);
    void StopThread();

protected:
    virtual void Run() = 0;

    // 잡큐 이벤트 발생
    void SignalEvent()
    {
        SetEvent(mEventHandle);
    }

public:
    static unsigned __stdcall ThreadFunction(LPVOID param)
    {
        ThreadWorker* worker = reinterpret_cast<ThreadWorker*>(param);
        worker->Run();

        return 0;
    }


    // 모니터링 데이터
public:
    __int64 mJobThreadUpdateCnt;
    __int64 mJobThreadUpdateTPS;

};

#endif