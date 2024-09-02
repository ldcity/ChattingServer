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
public:
	ChatServer();
	~ChatServer();

	bool ChatServerStart();
	bool ChatServerStop();

	bool OnConnectionRequest(const wchar_t* IP, unsigned short PORT);
	void OnClientJoin(uint64_t sessionID);
	void OnClientLeave(uint64_t sessionID);
	void OnRecv(uint64_t sessionID, CPacket* packet);
	void OnJob(uint64_t sessionID, CPacket* packet);
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
	bool CheckPlayer(uint64_t sessionID, INT64 accountNo)
	{
		// accountNo �ߺ�üũ
		AcquireSRWLockExclusive(&accountNoMapLock);
		auto accountIter = m_accountNo.find(accountNo);
		if (accountIter != m_accountNo.end())
		{
			Player* dupPlayer = FindPlayer(accountIter->second);

			if (dupPlayer == nullptr)
			{
				ReleaseSRWLockExclusive(&accountNoMapLock);
				return false;
			}

			DisconnectSession(dupPlayer->sessionID);
		}

		m_accountNo.insert({ accountNo, sessionID });
		ReleaseSRWLockExclusive(&accountNoMapLock);

		return true;
	}
	//--------------------------------------------------------------------------------------
	// Packet Proc
	//--------------------------------------------------------------------------------------
	bool PacketProc(uint64_t sessionID, CPacket* packet);						// Packet Handler
	void NetPacketProc_Login(uint64_t sessionID, CPacket* packet);				// �α��� ��û
	void NetPacketProc_SectorMove(uint64_t sessionID, CPacket* packet);			// ���� �̵� ��û
	void NetPacketProc_Chatting(uint64_t sessionID, CPacket* packet);			// ä�� ������
	void NetPacketProc_HeartBeat(uint64_t sessionID, CPacket* packet);			// ��Ʈ��Ʈ

private:
	Log* chatLog;

	int m_userMAXCnt;														// �ִ� player ��
	int m_timeout;															// Ÿ�Ӿƿ� �ð�

	HANDLE m_moniteringThread;							// Monitering Thread

	HANDLE m_moniterEvent;								// Monitering Event
	HANDLE m_runEvent;									// Thread Start Event

	TLSObjectPool<Player> playerPool = TLSObjectPool<Player>(200);

	std::unordered_map<uint64_t, Player*> m_mapPlayer;							// ��ü Player ��ü
	Sector m_Sector[dfSECTOR_Y_MAX][dfSECTOR_X_MAX];						// ����

	std::unordered_map<int64_t, uint64_t> m_accountNo;

	SRWLOCK playerMapLock;
	SRWLOCK accountNoMapLock;

	friend unsigned __stdcall MoniteringThread(void* param);

	bool MoniterThread_serv();

	// ����͸� ���� ������
private:
	__int64 m_totalPlayerCnt;												// player total
	__int64 m_loginPlayerCnt;
	
	__int64 m_loginPacketTPS;
	__int64 m_sectorMovePacketTPS;
	__int64 m_chattingReqTPS;
	__int64 m_chattingResTPS;
	
	__int64 m_timeoutTotalCnt;
	__int64 m_timeoutCntTPS;
	
	__int64 m_deletePlayerCnt;
	__int64 m_deletePlayerTPS;

	__int64 m_updateTPS;

	__int64 m_redisJobThreadUpdateTPS;
	__int64 m_loginResJobUpdateTPS;

	bool startFlag;

private:
	PerformanceMonitor performMoniter;
	MonitoringLanClient lanClient;

	std::wstring m_tempIp;
	std::string m_ip;

	CRedis* redis;

	static DWORD					_RedisTlsIdx;

	LockFreeStack<CRedis*> tlsRedisObjects = LockFreeStack<CRedis*>(5);		// tls db ��ü�� �����ϱ� ���� �ʿ� 

	wchar_t redisIP[20];
	int redisPort;
};




#endif