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

	TLSObjectPool<Job> _jobPool = TLSObjectPool<Job>(100);
	LockFreeQueue<Job*> _jobQ = LockFreeQueue<Job*>(200);

	CRedis* _redis;		// Redis Connector

	wchar_t _redisIP[20];
	int _redisPort;

	ChatServer* _chatServer;

	friend class ChatServer;

public:
	RedisWorkerThread(ChatServer* chatServer, const wchar_t* ip, const int port) :
		_chatServer(chatServer), _redis(nullptr), _redisPort(port)
	{
		wmemcpy_s(_redisIP, 20, ip, 20);

		_redis = new CRedis;
	}

	~RedisWorkerThread()
	{
		if (_redis)
			delete _redis;
	}

	void EnqueueJob(WORD type, uint64_t sessionID, INT64 accountNo, std::string sessionKey, CPacket* packet)
	{
		Job* job = _jobPool.Alloc();
		job->type = type;
		job->sessionID = sessionID;
		job->accountNo = accountNo;
		job->sessionKey = sessionKey;
		job->packet = packet;

		_jobQ.Enqueue(job);

		SignalEvent();
	}

	void Run() override;


public:
	void RedisGet(Job* job);
};


#endif 