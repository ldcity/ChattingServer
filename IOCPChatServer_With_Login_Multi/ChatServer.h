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
	void OnJob(uint64_t sessionID, CPacket* packet);
	void OnTimeout(uint64_t sessionID);
	void OnError(int errorCode, const wchar_t* msg);

	//--------------------------------------------------------------------------------------
	// player 관련 함수
	//--------------------------------------------------------------------------------------
	Player* FindPlayer(uint64_t sessionID)							// player 검색
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
	bool CreatePlayer(uint64_t sessionID);									// player 생성
	bool DeletePlayer(uint64_t sessionID);									// player 삭제
	

	// player 중복 체크
	bool CheckPlayer(uint64_t sessionID, INT64 accountNo)
	{
		// accountNo 중복체크
		AcquireSRWLockShared(&accountNoMapLock);
		auto accountIter = m_accountNo.find(accountNo);
		if (accountIter != m_accountNo.end())
		{
			Player* dupPlayer = FindPlayer(accountIter->second);

			if (dupPlayer == nullptr)
			{
				ReleaseSRWLockShared(&accountNoMapLock);
				return false;
			}

			DisconnectSession(dupPlayer->sessionID);
		}

		m_accountNo.insert({ accountNo, sessionID });
		ReleaseSRWLockShared(&accountNoMapLock);

		return true;
	}

	bool Authentication(Player* player);		// 동기 get 인증 요청										

	//--------------------------------------------------------------------------------------
	// Packet Proc
	//--------------------------------------------------------------------------------------
	bool PacketProc(uint64_t sessionID, CPacket* packet);					// Packet Handler
	void netPacketProc_Login(uint64_t sessionID, CPacket* packet);				// 로그인 요청
	void netPacketProc_ResLoginRedis(uint64_t sessionID, CPacket* packet);	// 로그인 응답
	void netPacketProc_SectorMove(uint64_t sessionID, CPacket* packet);			// 섹터 이동 요청
	void netPacketProc_Chatting(uint64_t sessionID, CPacket* packet);			// 채팅 보내기
	void netPacketProc_HeartBeat(uint64_t sessionID, CPacket* packet);			// 하트비트

private:
	Log* chatLog;

	int m_userMAXCnt;														// 최대 player 수
	int m_timeout;															// 타임아웃 시간

	HANDLE m_redisJobHandle;
	HANDLE m_redisJobEvent;

	HANDLE m_moniteringThread;							// Monitering Thread

	HANDLE m_moniterEvent;								// Monitering Event
	HANDLE m_runEvent;									// Thread Start Event

	TLSObjectPool<Player> playerPool = TLSObjectPool<Player>(200);

	TLSObjectPool<RedisJob> redisJobPool = TLSObjectPool<RedisJob>(0);

	LockFreeQueue<RedisJob*> redisJobQ = LockFreeQueue<RedisJob*>(1000);

	std::unordered_map<uint64_t, Player*> m_mapPlayer;							// 전체 Player 객체
	Sector m_Sector[dfSECTOR_Y_MAX][dfSECTOR_X_MAX];						// 섹터

	// 중복 account 확인을 위해 중복을 허용하는 multimap으로 사용
	// -> 이렇게 하지 않으면 중복 account 인걸 확인하여 이전 세션을 disconnect 한 후,
	// m_accountNo에 새롭게 할당된 player의 accountNo(같은 번호)를 insert 하려고 해도
	// onLeave에서 해당 accountNo를 제거하기 직전이면 insert되지 않음
	// 잠깐의 시간동안은 중복을 허용해야 함
	std::unordered_map<int64_t, uint64_t> m_accountNo;

	SRWLOCK playerMapLock;
	SRWLOCK accountNoMapLock;

	friend unsigned __stdcall RedisJobWorkerThread(PVOID param);			// Redis Job 일 처리 스레드
	friend unsigned __stdcall MoniteringThread(void* param);

	bool RedisJobWorkerThread_serv();
	bool MoniterThread_serv();

	// 모니터링 관련 변수들
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

	__int64 m_updateTotal;
	__int64 m_updateTPS;

	__int64 m_redisGetCnt;
	__int64 m_redisGetTPS;

	__int64 m_redisJobThreadUpdateTPS;
	__int64 m_redisJobEnqueueTPS;

	bool startFlag;

private:
	PerformanceMonitor performMoniter;
	MonitoringLanClient lanClient;

	std::wstring m_tempIp;
	std::string m_ip;

	CRedis* redis;

};




#endif