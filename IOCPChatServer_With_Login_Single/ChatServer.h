#ifndef __NET_CHATSERVER_CLASS__
#define __NET_CHATSERVER_CLASS__

#include "PCH.h"
#include "NetServer.h"
#include "MonitoringLanClient.h"
#include <variant>

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
		MSG_PACKET,				// ��Ŷ
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

		// �񵿱� redis ��û ����� ���� ��ü (set�� ��� bool, get�� ��� future ��ü)
		//std::variant<std::future<cpp_redis::reply>, std::future<bool>> redisFuture;
		std::future<cpp_redis::reply> redisFuture;
	};

public:
	ChatServer();
	~ChatServer();

	bool ChatServerStart();
	bool ChatServerStop();

	bool PacketProc(uint64_t sessionID, CPacket* packet);

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

		auto iter = m_mapPlayer.find(sessionID);

		if (iter == m_mapPlayer.end())
		{
			//chatLog->logger(dfLOG_LEVEL_DEBUG, __LINE__, L"FindPlayer # Player Not Found!");
			return nullptr;
		}

		return iter->second;
	}

	bool CreatePlayer(uint64_t sessionID);									// player ����
	bool DeletePlayer(uint64_t sessionID);									// player ����

	// player �ߺ� üũ
	bool CheckPlayer(Player* player, INT64 accountNo)
	{
		// accountNo �ߺ�üũ
		auto accountIter = m_accountNo.find(accountNo);
		if (accountIter != m_accountNo.end())
		{
			Player* dupPlayer = FindPlayer(accountIter->second);

			if (dupPlayer == nullptr)
			{
				return false;
			}

			m_accountNo.erase(accountIter);

			DisconnectSession(dupPlayer->sessionID);

			return true;
		}

		return true;
	}

	bool Authentication(Player* player);		// ���� ���� ��û
	void AsyncAuthentication(Player* player);	// �񵿱� ���� ��û
	bool AsyncLogin(RedisJob* redisJob);		// �񵿱� ���� ��û ��� ó��		

	//--------------------------------------------------------------------------------------
	// Packet Proc
	//--------------------------------------------------------------------------------------
	void netPacketProc_Login(uint64_t sessionID, CPacket* packet);			// �α��� ��û
	void netPacketProc_SectorMove(uint64_t sessionID, CPacket* packet);		// ���� �̵� ��û
	void netPacketProc_Chatting(uint64_t sessionID, CPacket* packet);		// ä�� ������
	void netPacketProc_HeartBeat(uint64_t sessionID, CPacket* packet);		// ��Ʈ��Ʈ

private:
	Log* chatLog;

	int m_userMAXCnt;														// �ִ� player ��
	int m_timeout;															// Ÿ�Ӿƿ� �ð�

	HANDLE m_jobHandle;
	HANDLE m_jobEvent;

	HANDLE m_redisJobHandle;
	HANDLE m_redisJobEvent;

	HANDLE m_moniteringThread;							// Monitering Thread

	HANDLE m_moniterEvent;								// Monitering Event
	HANDLE m_runEvent;									// Thread Start Event

	TLSObjectPool<Player> playerPool = TLSObjectPool<Player>(200);
	TLSObjectPool<ChatJob> jobPool = TLSObjectPool<ChatJob>(300);
	TLSObjectPool<RedisJob> redisJobPool = TLSObjectPool<RedisJob>(50);

	LockFreeQueue<ChatJob*> chatJobQ = LockFreeQueue<ChatJob*>(20000);
	LockFreeQueue<RedisJob*> redisJobQ = LockFreeQueue<RedisJob*>(10000);

	std::unordered_map<uint64_t, Player*> m_mapPlayer;							// ��ü Player ��ü
	std::unordered_set<Player*> m_Sector[dfSECTOR_Y_MAX][dfSECTOR_X_MAX];		// �� ���Ϳ� �����ϴ� Player ��ü
	
	// �ߺ� account Ȯ���� ���� �ߺ��� ����ϴ� multimap���� ���
	// -> �̷��� ���� ������ �ߺ� account �ΰ� Ȯ���Ͽ� ���� ������ disconnect �� ��,
	// m_accountNo�� ���Ӱ� �Ҵ�� player�� accountNo(���� ��ȣ)�� insert �Ϸ��� �ص�
	// onLeave���� �ش� accountNo�� �����ϱ� �����̸� insert���� ����
	// ����� �ð������� �ߺ��� ����ؾ� ��
	std::unordered_multimap<int64_t, uint64_t> m_accountNo;

	friend unsigned __stdcall JobWorkerThread(PVOID param);					// Job �� ó�� ������
	friend unsigned __stdcall RedisJobWorkerThread(PVOID param);			// Redis Job �� ó�� ������
	friend unsigned __stdcall MoniteringThread(void* param);

	bool JobWorkerThread_serv();
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

	__int64 m_loginPacketTPS;
	__int64 m_sectorMovePacketTPS;
	__int64 m_chattingReqTPS;
	__int64 m_chattingResTPS;
	
	__int64 m_timeoutTotalCnt;
	__int64 m_timeoutCntTPS;
	
	__int64 m_deletePlayerCnt;
	__int64 m_deletePlayerTPS;

	__int64 m_redisGetCnt;
	__int64 m_redisGetTPS;

	bool startFlag;

private:
	PerformanceMonitor performMoniter;
	MonitoringLanClient lanClient;

	std::wstring m_tempIp;
	std::string m_ip;

	CRedis* redis;
};


#endif