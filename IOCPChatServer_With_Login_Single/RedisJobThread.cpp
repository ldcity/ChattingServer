#include "PCH.h"
#include "RedisJobThread.h"
#include "Profiling.h"
#include "ChatServer.h"

void RedisWorkerThread::Run()
{
	mRedis->Connect(redisIP, redisPort);

	while (mRunFlag)
	{
		WaitForSingleObject(mEventHandle, INFINITE);

		Job* job = nullptr;

		// Queue에 Job이 없을 때까지 update 수행
		while (mJobQ.GetSize() > 0)
		{
			if (mJobQ.Dequeue(job))
			{
				switch (job->type)
				{
					// Redis DB에서 value 얻어옴
				case REDISTYPE::GET:
					RedisGet(job);
					break;

				default:
					break;
				}

				mJobPool.Free(job);

				InterlockedIncrement64(&mJobThreadUpdateCnt);
				InterlockedIncrement64(&mJobThreadUpdateTPS);
			}
		}
	}
}

void RedisWorkerThread::RedisGet(Job* job)
{
	std::string accountNoStr = std::to_string(job->accountNo);

	// 비동기 redis get 요청
	mRedis->AsyncGet(accountNoStr, [=](const cpp_redis::reply& reply) {

		BYTE status = en_PACKET_CS_MONITOR_TOOL_RES_LOGIN::dfMONITOR_TOOL_LOGIN_OK;

		// redis에 인증 키가 없으면 실패!
		if (reply.is_null())
		{
			status = en_PACKET_CS_MONITOR_TOOL_RES_LOGIN::dfMONITOR_TOOL_LOGIN_ERR_NOSERVER;
		}
		// redis에 인증 키가 있고, 클라이언트가 갖고 있는 인증 키와 같다면 성공!
		else
		{	// 인증 토큰의 문자열 얻어옴
			std::string redisSessionKey = reply.as_string();

			// 인증 토큰이 다르면 로그인 실패!
			if (redisSessionKey.compare(job->sessionKey) != 0)
			{
				status = en_PACKET_CS_MONITOR_TOOL_RES_LOGIN::dfMONITOR_TOOL_LOGIN_ERR_NOSERVER;
			}
		}

		// 로그인 응답 패킷 전송
		CPacket* resLoginPacket = CPacket::Alloc();

		// 로그인 응답 패킷 Setting
		MPResLogin(resLoginPacket, status, job->accountNo);

		chatServer->SendJob(job->sessionID, ChatServer::JobType::LOGIN_RES, resLoginPacket);
	});
}