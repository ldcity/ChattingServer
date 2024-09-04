#include "PCH.h"
#include "RedisJobThread.h"
#include "Profiling.h"
#include "ChatServer.h"

void RedisWorkerThread::Run()
{
	_redis->Connect(_redisIP, _redisPort);

	while (_runFlag)
	{
		WaitForSingleObject(_eventHandle, INFINITE);

		Job* job = nullptr;

		// Queue�� Job�� ���� ������ update ����
		while (_jobQ.GetSize() > 0)
		{
			if (_jobQ.Dequeue(job))
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

				_jobPool.Free(job);

				InterlockedIncrement64(&_jobThreadUpdateCnt);
				InterlockedIncrement64(&_jobThreadUpdateTPS);
			}
		}
	}
}

void RedisWorkerThread::RedisGet(Job* job)
{
	std::string accountNoStr = std::to_string(job->accountNo);

	// �񵿱� redis get ��û
	_redis->AsyncGet(accountNoStr, [=](const cpp_redis::reply& reply) {

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

		_chatServer->SendJob(job->sessionID, ChatServer::JobType::LOGIN_RES, resLoginPacket);
	});
}