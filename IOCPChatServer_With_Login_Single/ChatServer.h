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
		NEW_CONNECT,			// 새 접속
		DISCONNECT,				// 접속 해제
		MSG_PACKET,				// 패킷
		REDIS_RES,				// 레디스 결과 이후 로그인 처리
		TIMEOUT					// 타임아웃
	};

	// Job 구조체
	struct ChatJob
	{
		// Session 고유 ID
		uint64_t sessionID;

		// Job Type (새 접속, 패킷 메시지, 접속 해제 등)
		WORD type;

		// 패킷 포인터
		CPacket* packet;
	};

	// Redis Job 구조체
	struct RedisJob
	{
		uint64_t sessionID;				// Session 고유 ID
		INT64 accountNo;
		std::string sessionKey;
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
	// player 관련 함수
	//--------------------------------------------------------------------------------------
	Player* FindPlayer(uint64_t sessionID)							// player 검색
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

	bool CreatePlayer(uint64_t sessionID);									// player 생성
	bool DeletePlayer(uint64_t sessionID);									// player 삭제

	// player 중복 체크
	bool CheckPlayer(uint64_t sessionID, INT64 accountNo)
	{
		// accountNo 중복체크
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
	void netPacketProc_Login(uint64_t sessionID, CPacket* packet);			// 로그인 요청
	void netPacketProc_ResLoginRedis(uint64_t sessionID, CPacket* packet);		// 로그인 응답
	void netPacketProc_SectorMove(uint64_t sessionID, CPacket* packet);		// 섹터 이동 요청
	void netPacketProc_Chatting(uint64_t sessionID, CPacket* packet);		// 채팅 보내기
	void netPacketProc_HeartBeat(uint64_t sessionID, CPacket* packet);		// 하트비트

private:
	Log* chatLog;

	int m_userMAXCnt;														// 최대 player 수
	int m_timeout;															// 타임아웃 시간

	HANDLE m_jobHandle;
	HANDLE m_jobEvent;

	HANDLE m_redisJobHandle;
	HANDLE m_redisJobEvent;

	HANDLE m_moniteringThread;							// Monitering Thread

	HANDLE m_moniterEvent;								// Monitering Event
	HANDLE m_runEvent;								// Thread Start Event

	TLSObjectPool<Player> playerPool = TLSObjectPool<Player>(150);
	TLSObjectPool<ChatJob> jobPool = TLSObjectPool<ChatJob>(200);
	TLSObjectPool<RedisJob> redisJobPool = TLSObjectPool<RedisJob>(50);

	LockFreeQueue<ChatJob*> chatJobQ = LockFreeQueue<ChatJob*>(10000);
	LockFreeQueue<RedisJob*> redisJobQ = LockFreeQueue<RedisJob*>(100);

	std::unordered_map<uint64_t, Player*> m_mapPlayer;				// 전체 Player 관리
	std::unordered_set<Player*> m_Sector[dfSECTOR_Y_MAX][dfSECTOR_X_MAX];		// 각 섹터에 존재하는 Player 관리
	std::unordered_map<int64_t, uint64_t> m_accountNo;				// Player 계정 번호 관리

	friend unsigned __stdcall JobWorkerThread(PVOID param);				// Job 일 처리 스레드
	friend unsigned __stdcall RedisJobWorkerThread(PVOID param);			// Redis Job 일 처리 스레드
	friend unsigned __stdcall MoniteringThread(void* param);

	bool JobWorkerThread_serv();
	bool RedisJobWorkerThread_serv();
	bool MoniterThread_serv();

// 모니터링 관련 변수들
private:
	__int64 m_totalPlayerCnt;												
	__int64 m_loginPlayerCnt;

	__int64 m_jobUpdatecnt;													
	__int64 m_jobThreadUpdateCnt;										

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

	__int64 m_redisJobEnqueueTPS;
	__int64 m_redisJobThreadUpdateTPS;


	bool startFlag;

private:
	PerformanceMonitor performMoniter;
	MonitoringLanClient lanClient;

	std::wstring m_tempIp;
	std::string m_ip;

	CRedis* redis;
};


#endif
