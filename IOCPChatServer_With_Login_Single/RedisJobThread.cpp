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

		// Queue�� Job�� ���� ������ update ����
		while (mJobQ.GetSize() > 0)
		{
			if (mJobQ.Dequeue(job))
			{
				switch (job->type)
				{
					// Redis DB���� value ����
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

	// �񵿱� redis get ��û
	mRedis->AsyncGet(accountNoStr, [=](const cpp_redis::reply& reply) {

		BYTE status = en_PACKET_CS_MONITOR_TOOL_RES_LOGIN::dfMONITOR_TOOL_LOGIN_OK;

		// redis�� ���� Ű�� ������ ����!
		if (reply.is_null())
		{
			status = en_PACKET_CS_MONITOR_TOOL_RES_LOGIN::dfMONITOR_TOOL_LOGIN_ERR_NOSERVER;
		}
		// redis�� ���� Ű�� �ְ�, Ŭ���̾�Ʈ�� ���� �ִ� ���� Ű�� ���ٸ� ����!
		else
		{	// ���� ��ū�� ���ڿ� ����
			std::string redisSessionKey = reply.as_string();

			// ���� ��ū�� �ٸ��� �α��� ����!
			if (redisSessionKey.compare(job->sessionKey) != 0)
			{
				status = en_PACKET_CS_MONITOR_TOOL_RES_LOGIN::dfMONITOR_TOOL_LOGIN_ERR_NOSERVER;
			}
		}

		// �α��� ���� ��Ŷ ����
		CPacket* resLoginPacket = CPacket::Alloc();

		// �α��� ���� ��Ŷ Setting
		MPResLogin(resLoginPacket, status, job->accountNo);

		chatServer->SendJob(job->sessionID, ChatServer::JobType::LOGIN_RES, resLoginPacket);
	});
}