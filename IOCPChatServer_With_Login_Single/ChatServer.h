#ifndef __NET_CHATSERVER_CLASS__
#define __NET_CHATSERVER_CLASS__

#include "PCH.h"
#include "NetServer.h"
#include "MonitoringLanClient.h"
#include "RedisJobThread.h"

class ChatServer : public NetServer
{
public:
	//--------------------------------------------------------------------------------------
	// Job Info
	//--------------------------------------------------------------------------------------
	enum JobType
	{
		NEW_CONNECT,	// �� ����
		DISCONNECT,		// ���� ����
		MSG_PACKET,		// ���ŵ� ��Ŷ ó��
		LOGIN_RES,		// �α��� ���� ��Ŷ ó��
		TIMEOUT			// Ÿ�Ӿƿ�
	};

	enum ErrorCode
	{
		REDISSETERROR,
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

public:
	void SendJob(uint64_t sessionID, WORD type, CPacket* packet);

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

		auto iter = m_mapPlayer.find(sessionID);

		if (iter == m_mapPlayer.end())
		{
			return nullptr;
		}

		return iter->second;
	}

	bool CreatePlayer(uint64_t sessionID);									// player ����
	bool DeletePlayer(uint64_t sessionID);									// player ����

	// player �ߺ� üũ
	bool CheckPlayer(uint64_t sessionID, INT64 accountNo)
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

			DisconnectSession(dupPlayer->sessionID);
		}

		m_accountNo.insert({ accountNo, sessionID });

		return true;
	}
	//--------------------------------------------------------------------------------------
	// Packet Proc
	//--------------------------------------------------------------------------------------
	bool PacketProc(uint64_t sessionID, CPacket* packet);
	void NetPacketProc_Login(uint64_t sessionID, CPacket* packet);			// �α��� ��û
	void NetPacketProc_ResLogin(uint64_t sessionID, CPacket* packet);	// �α��� ����
	void NetPacketProc_SectorMove(uint64_t sessionID, CPacket* packet);		// ���� �̵� ��û
	void NetPacketProc_Chatting(uint64_t sessionID, CPacket* packet);		// ä�� ������
	void NetPacketProc_HeartBeat(uint64_t sessionID, CPacket* packet);		// ��Ʈ��Ʈ

private:
	RedisWorkerThread* redisWorkerThread;

private:
	Log* chatLog;

	int m_userMAXCnt;														// �ִ� player ��
	int m_timeout;															// Ÿ�Ӿƿ� �ð�

	HANDLE m_jobHandle;
	HANDLE m_jobEvent;

	HANDLE m_moniteringThread;							// Monitering Thread

	HANDLE m_moniterEvent;								// Monitering Event
	HANDLE m_runEvent;									// Thread Start Event

	TLSObjectPool<Player> playerPool = TLSObjectPool<Player>(190);

	TLSObjectPool<ChatJob> jobPool = TLSObjectPool<ChatJob>(290);
	LockFreeQueue<ChatJob*> chatJobQ = LockFreeQueue<ChatJob*>(15000);

	std::unordered_map<uint64_t, Player*> m_mapPlayer;							// ��ü Player ��ü
	std::unordered_set<Player*> m_Sector[dfSECTOR_Y_MAX][dfSECTOR_X_MAX];		// �� ���Ϳ� �����ϴ� Player ��ü
	std::unordered_map<int64_t, uint64_t> m_accountNo;

	friend unsigned __stdcall JobWorkerThread(PVOID param);					// Job �� ó�� ������
	friend unsigned __stdcall MoniteringThread(void* param);

	bool JobWorkerThread_serv();
	bool MoniterThread_serv();

	// ����͸� ���� ������
private:
	__int64 _totalPlayerCnt;												// player total
	__int64 m_loginPlayerCnt;

	__int64 m_loginCount;
	__int64 m_loginTPS;

	__int64 m_jobUpdatecnt;													// job ����
	__int64 m_jobUpdateTPS;											// job thread update Ƚ��

	__int64 m_loginPacketTPS;
	__int64 m_sectorMovePacketTPS;
	__int64 m_chattingReqTPS;
	__int64 m_chattingResTPS;
	
	__int64 m_timeoutTotalCnt;
	__int64 m_timeoutCntTPS;
	
	__int64 m_deletePlayerCnt;
	__int64 m_deletePlayerTPS;

	__int64 m_redisJobThreadUpdateTPS;
	__int64 m_jobThreadUpdateCnt;

	__int64 m_loginResJobUpdateTPS;

	bool startFlag;

private:
	PerformanceMonitor performMoniter;
	MonitoringLanClient lanClient;

	std::wstring m_tempIp;
	std::string m_ip;
};


#endif