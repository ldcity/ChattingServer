#ifndef __REDIS_WORKER_CLASS__
#define __REDIS_WORKER_CLASS__

#include "ThreadWorker.h"
#include "Redis.h"
#include "SerializingBuffer.h"
#include "TLSFreeList.h"
#include "LockFreeQueue.h"

class ChatServer;

class RedisWorkerThread : public ThreadWorker
{
public:
	enum REDISTYPE
	{
		SET,  // 레디스 set
		GET,  // 레디스 get
	};

private:
	// Redis Job 구조체
	struct Job
	{
		WORD type;
		uint64_t sessionID;			// Session 고유 ID
		INT64 accountNo;
		std::string sessionKey; // 세션 키 포인터

		CPacket* packet;
	};

	TLSObjectPool<Job> mJobPool = TLSObjectPool<Job>(100);
	LockFreeQueue<Job*> mJobQ = LockFreeQueue<Job*>(200);

	CRedis* mRedis;		// Redis Connector

	wchar_t redisIP[20];
	int redisPort;

	ChatServer* chatServer;

	friend class ChatServer;

public:
	RedisWorkerThread(ChatServer* _chatServer, const wchar_t* ip, const int port) :
		chatServer(_chatServer), mRedis(nullptr), redisPort(port)
	{
		wmemcpy_s(redisIP, 20, ip, 20);

		mRedis = new CRedis;
	}

	~RedisWorkerThread()
	{
		if (mRedis)
			delete mRedis;
	}

	void EnqueueJob(WORD type, uint64_t sessionID, INT64 accountNo, std::string sessionKey, CPacket* packet)
	{
		Job* job = mJobPool.Alloc();
		job->type = type;
		job->sessionID = sessionID;
		job->accountNo = accountNo;
		job->sessionKey = sessionKey;
		job->packet = packet;

		mJobQ.Enqueue(job);

		SignalEvent();
	}

	void Run() override;


public:
	void RedisGet(Job* job);
};


#endif 