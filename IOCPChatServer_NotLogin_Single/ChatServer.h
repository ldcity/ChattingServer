#ifndef __NET_CHATSERVER_CLASS__
#define __NET_CHATSERVER_CLASS__

#include "NetServer.h"
#include "MonitoringLanClient.h"

#include <unordered_map>
#include <unordered_set>


class ChatServer : public NetServer
{
public:
	ChatServer();
	~ChatServer();

	bool ChatServerStart();
	bool ChatServerStop();

	// NetServer�� ���� �Լ� ������
	bool OnConnectionRequest(const wchar_t* IP, unsigned short PORT) override;
	void OnClientJoin(uint64_t sessionID) override;
	void OnClientLeave(uint64_t sessionID) override;
	void OnRecv(uint64_t sessionID, CPacket* packet) override;
	void OnTimeout(uint64_t sessionID) override;
	void OnError(int errorCode, const wchar_t* msg) override;

	//--------------------------------------------------------------------------------------
	// player ���� �Լ�
	//--------------------------------------------------------------------------------------
	// sessionID�� ����Ͽ� player �˻�
	inline Player* FindPlayer(uint64_t sessionID)
	{
		Player* player = nullptr;

		auto iter = m_mapPlayer.find(sessionID);

		if (iter == m_mapPlayer.end())

			return nullptr;

		return iter->second;
	}

	// player ����
	bool CreatePlayer(uint64_t sessionID);		

	// player ����
	bool DeletePlayer(uint64_t sessionID);									
	
	// player �ߺ� üũ
	inline bool CheckPlayer(Player* player, INT64 accountNo)
	{
		// accountNo �ߺ�üũ
		auto accountIter = m_accountNo.find(accountNo);
		if (accountIter != m_accountNo.end())
		{
			Player* dupPlayer = FindPlayer(accountIter->second);

			if (dupPlayer == nullptr)
				return false;

			m_accountNo.erase(accountIter);

			DisconnectSession(dupPlayer->sessionID);

			return true;
		}

		return true;
	}
	
	//// player �ߺ� üũ
	//bool CheckPlayer(uint64_t sessionID, INT64 accountNo)
	//{
	//	// accountNo �ߺ�üũ
	//	auto accountIter = m_accountNo.find(accountNo);
	//	if (accountIter != m_accountNo.end())
	//	{
	//		Player* dupPlayer = FindPlayer(accountIter->second);

	//		if (dupPlayer == nullptr)
	//		{
	//			return false;
	//		}

	//		DisconnectSession(dupPlayer->sessionID);
	//	}

	//	m_accountNo.insert({ accountNo, sessionID });

	//	return true;
	//}


	//--------------------------------------------------------------------------------------
	// Packet Proc
	//--------------------------------------------------------------------------------------
	bool PacketProc(uint64_t sessionID, CPacket* packet);					// Packet Handler
	void netPacketProc_Login(uint64_t sessionID, CPacket* packet);			// �α��� ��û
	void netPacketProc_SectorMove(uint64_t sessionID, CPacket* packet);		// ���� �̵� ��û
	void netPacketProc_Chatting(uint64_t sessionID, CPacket* packet);		// ä�� ������
	void netPacketProc_HeartBeat(uint64_t sessionID, CPacket* packet);		// ��Ʈ��Ʈ

private:
	//--------------------------------------------------------------------------------------
	// Job Info
	//--------------------------------------------------------------------------------------
	enum JobType
	{
		NEW_CONNECT,			// �� ����
		DISCONNECT,				// ���� ����
		MSG_PACKET				// ��Ŷ ó��
	};

	// Job ����ü
	struct ChatJob
	{
		uint64_t sessionID;
		WORD type;				// JobType
		CPacket* packet;
	};

private:
	Log* chatLog;

	int m_userMAXCnt;									// �ִ� player ��
	int m_timeout;										// Ÿ�Ӿƿ� �ð�

	HANDLE m_jobHandle;
	HANDLE m_jobEvent;

	HANDLE m_moniteringThread;							// Monitering Thread

	HANDLE m_moniterEvent;								// Monitering Event
	HANDLE m_runEvent;									// Thread Start Event

	TLSObjectPool<Player> playerPool = TLSObjectPool<Player>(200);		// Player ��ü�� ��� Pool
	TLSObjectPool<ChatJob> jobPool = TLSObjectPool<ChatJob>(300);		// OnRecv�� ���� ���� �۾� ó���� ���� Job Pool
	LockFreeQueue<ChatJob*> chatJobQ = LockFreeQueue<ChatJob*>(30000);	// Job Update Thread���� JobQ�� ���� Job ó��

	unordered_map<uint64_t, Player*> m_mapPlayer;							// ��ü Player ��ü
	unordered_set<Player*> m_Sector[dfSECTOR_Y_MAX][dfSECTOR_X_MAX];		// �� ���Ϳ� �����ϴ� Player ��ü
	
	unordered_multimap<int64_t, uint64_t> m_accountNo;
	//std::unordered_map<int64_t, uint64_t> m_accountNo;

private:
	PerformanceMonitor performMoniter;			// ���� ����͸� ������ ����
	MonitoringLanClient lanClient;				// ä�� ������ ����͸� ������ �����Ͽ� ����͸� ���� �����ϱ� ���� �ʿ�

	wstring m_tempIp;
	string m_ip;

	friend unsigned __stdcall JobWorkerThread(PVOID param);					// Job �� ó�� ������
	friend unsigned __stdcall MoniteringThread(void* param);

	bool JobWorkerThread_serv();
	bool MoniterThread_serv();

	// ����͸� ���� ������
private:
	__int64 m_totalPlayerCnt;												// player total
	__int64 m_loginPlayerCnt;
	__int64 m_jobUpdatecnt;													// job ����
	__int64 m_jobThreadUpdateCnt;											// job thread update Ƚ��
	
	__int64 m_loginPacketTPS;
	__int64 m_sectorMovePacketTPS;
	__int64 m_chattingReqTPS;
	__int64 m_chattingResTPS;
	
	__int64 m_timeoutTotalCnt;
	__int64 m_timeoutCntTPS;
	
	__int64 m_deletePlayerCnt;
	__int64 m_deletePlayerTPS;

	bool startFlag;

	WCHAR startTime[64];
};




#endif