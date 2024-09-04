#ifndef __THREAD_WORKER_CLASS__
#define __THREAD_WORKER_CLASS__

#include <process.h>
#include <Windows.h>

class ThreadWorker
{
protected:
    HANDLE _threadHandle;
    HANDLE _eventHandle;
    bool _runFlag;

public:
    ThreadWorker() : _runFlag(true), _threadHandle(nullptr), _eventHandle(nullptr),
        _jobThreadUpdateCnt(0) {}

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
        SetEvent(_eventHandle);
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
    __int64 _jobThreadUpdateCnt;
    __int64 _jobThreadUpdateTPS;

};

#endif