#ifndef __NET_CHATSERVER_CLASS__
#define __NET_CHATSERVER_CLASS__

#include "PCH.h"
#include "NetServer.h"
#include "MonitoringLanClient.h"

class ChatServer : public NetServer
{
private:
	//--------------------------------------------------------------------------------------
	// Job Info
	//--------------------------------------------------------------------------------------
	enum JobType
	{
		NEW_CONNECT,			// �� ����
		DISCONNECT,				// ���� ����
		MSG_PACKET				// ��Ŷ
	};

	// Job ����ü
	struct ChatJob
	{
		// Session ���� ID
		uint64_t sessionID;

		// Job Type (�� ����, ��Ŷ �޽���, ���� ���� ��)
		WORD type;

		// ��Ŷ ������
		CPacket* packet;
	};

	struct RedisJob
	{
		// Session ���� ID
		uint64_t sessionID;

		// �񵿱� redis ��û ����� ���� future ��ü
		std::future<cpp_redis::reply> redisFuture;
	};

public:
	ChatServer();
	~ChatServer();

	bool ChatServerStart();
	bool ChatServerStop();

	bool OnConnectionRequest(const wchar_t* IP, unsigned short PORT);
	void OnClientJoin(uint64_t sessionID);
	void OnClientLeave(uint64_t sessionID);
	void OnRecv(uint64_t sessionID, CPacket* packet);
	void OnTimeout(uint64_t sessionID);
	void OnError(int errorCode, const wchar_t* msg);

	//--------------------------------------------------------------------------------------
	// player ���� �Լ�
	//--------------------------------------------------------------------------------------
	Player* FindPlayer(uint64_t sessionID)							// player �˻�
	{
		Player* player = nullptr;

		AcquireSRWLockShared(&playerMapLock);
		auto iter = m_mapPlayer.find(sessionID);

		if (iter == m_mapPlayer.end())
		{
			ReleaseSRWLockShared(&playerMapLock);
			return nullptr;
		}

		ReleaseSRWLockShared(&playerMapLock);
		return iter->second;
	}
	bool CreatePlayer(uint64_t sessionID);									// player ����
	bool DeletePlayer(uint64_t sessionID);									// player ����
	

	// player �ߺ� üũ
	bool CheckPlayer(Player* player, INT64 accountNo)
	{
		// accountNo �ߺ�üũ
		AcquireSRWLockShared(&accountNoMapLock);
		auto accountIter = m_accountNo.find(accountNo);
		if (accountIter != m_accountNo.end())
		{
			Player* dupPlayer = FindPlayer(accountIter->second);

			if (dupPlayer == nullptr)
			{
				return false;
			}

			m_accountNo.erase(accountIter);
			ReleaseSRWLockShared(&accountNoMapLock);

			DisconnectSession(dupPlayer->sessionID);

			return true;
		}

		ReleaseSRWLockShared(&accountNoMapLock);
		return true;
	}

	bool Authentication(Player* player);		// ���� get ���� ��û										// ���� ���� ��û
	void AsyncAuthentication(Player* player);	// �񵿱� get ���� ��û
	bool AsyncLogin(RedisJob* redisJob);		// �񵿱� get ���� ��û ����� ���� ó��		


	//--------------------------------------------------------------------------------------
	// Packet Proc
	//--------------------------------------------------------------------------------------
	bool PacketProc(uint64_t sessionID, CPacket* packet);					// Packet Handler
	void netPacketProc_Login(Player* player, CPacket* packet);			// �α��� ��û
	void netPacketProc_SectorMove(Player* player, CPacket* packet);		// ���� �̵� ��û
	void netPacketProc_Chatting(Player* player, CPacket* packet);		// ä�� ������
	void netPacketProc_HeartBeat(Player* player, CPacket* packet);		// ��Ʈ��Ʈ

private:
	Log* chatLog;

	int m_userMAXCnt;														// �ִ� player ��
	int m_timeout;															// Ÿ�Ӿƿ� �ð�

	HANDLE m_redisJobHandle;
	HANDLE m_redisJobEvent;

	HANDLE m_moniteringThread;							// Monitering Thread

	HANDLE m_moniterEvent;								// Monitering Event
	HANDLE m_runEvent;									// Thread Start Event

	TLSObjectPool<Player> playerPool = TLSObjectPool<Player>(200);
	TLSObjectPool<RedisJob> redisJobPool = TLSObjectPool<RedisJob>(50);

	LockFreeQueue<RedisJob*> redisJobQ = LockFreeQueue<RedisJob*>(10000);

	std::unordered_map<uint64_t, Player*> m_mapPlayer;							// ��ü Player ��ü
	Sector m_Sector[dfSECTOR_Y_MAX][dfSECTOR_X_MAX];						// ����

	// �ߺ� account Ȯ���� ���� �ߺ��� ����ϴ� multimap���� ���
	// -> �̷��� ���� ������ �ߺ� account �ΰ� Ȯ���Ͽ� ���� ������ disconnect �� ��,
	// m_accountNo�� ���Ӱ� �Ҵ�� player�� accountNo(���� ��ȣ)�� insert �Ϸ��� �ص�
	// onLeave���� �ش� accountNo�� �����ϱ� �����̸� insert���� ����
	// ����� �ð������� �ߺ��� ����ؾ� ��
	std::unordered_multimap<int64_t, uint64_t> m_accountNo;

	SRWLOCK playerMapLock;
	SRWLOCK accountNoMapLock;

	friend unsigned __stdcall RedisJobWorkerThread(PVOID param);			// Redis Job �� ó�� ������
	friend unsigned __stdcall MoniteringThread(void* param);

	bool RedisJobWorkerThread_serv();
	bool MoniterThread_serv();

	// ����͸� ���� ������
private:
	__int64 m_totalPlayerCnt;												// player total
	__int64 m_loginPlayerCnt;
	
	__int64 m_jobUpdatecnt;													// job ����
	__int64 m_jobThreadUpdateCnt;											// job thread update Ƚ��
	
	__int64 m_redisJobUpdatecnt;											// redis job ����
	__int64 m_redisJobThreadUpdateCnt;										// redisjob thread update Ƚ��

	__int64 m_redisGetCnt;
	__int64 m_redisGetTPS;

	__int64 m_UpdateTPS;													// job thread update Ƚ��

	__int64 m_loginPacketTPS;
	__int64 m_sectorMovePacketTPS;
	__int64 m_chattingReqTPS;
	__int64 m_chattingResTPS;
	
	__int64 m_timeoutTotalCnt;
	__int64 m_timeoutCntTPS;
	
	__int64 m_deletePlayerCnt;
	__int64 m_deletePlayerTPS;

	bool startFlag;

private:
	PerformanceMonitor performMoniter;
	MonitoringLanClient lanClient;

	std::wstring m_tempIp;
	std::string m_ip;

	// multi thread�̱� ������ tls ���
	CRedis_TLS* redis_TLS;
};




#endif