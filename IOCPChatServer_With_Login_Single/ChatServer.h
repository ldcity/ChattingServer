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
		NEW_CONNECT,	// 새 접속
		DISCONNECT,		// 접속 해제
		MSG_PACKET,		// 수신된 패킷 처리
		LOGIN_RES,		// 로그인 응답 패킷 처리
		TIMEOUT			// 타임아웃
	};

	enum ErrorCode
	{
		REDISSETERROR,
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
	// player 관련 함수
	//--------------------------------------------------------------------------------------
	Player* FindPlayer(uint64_t sessionID)							// player 검색
	{
		Player* player = nullptr;

		auto iter = m_mapPlayer.find(sessionID);

		if (iter == m_mapPlayer.end())
		{
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
	void NetPacketProc_Login(uint64_t sessionID, CPacket* packet);			// 로그인 요청
	void NetPacketProc_ResLogin(uint64_t sessionID, CPacket* packet);	// 로그인 응답
	void NetPacketProc_SectorMove(uint64_t sessionID, CPacket* packet);		// 섹터 이동 요청
	void NetPacketProc_Chatting(uint64_t sessionID, CPacket* packet);		// 채팅 보내기
	void NetPacketProc_HeartBeat(uint64_t sessionID, CPacket* packet);		// 하트비트

private:
	RedisWorkerThread* redisWorkerThread;

private:
	Log* chatLog;

	int m_userMAXCnt;														// 최대 player 수
	int m_timeout;															// 타임아웃 시간

	HANDLE m_jobHandle;
	HANDLE m_jobEvent;

	HANDLE m_moniteringThread;							// Monitering Thread

	HANDLE m_moniterEvent;								// Monitering Event
	HANDLE m_runEvent;									// Thread Start Event

	TLSObjectPool<Player> playerPool = TLSObjectPool<Player>(190);

	TLSObjectPool<ChatJob> jobPool = TLSObjectPool<ChatJob>(290);
	LockFreeQueue<ChatJob*> chatJobQ = LockFreeQueue<ChatJob*>(15000);

	std::unordered_map<uint64_t, Player*> m_mapPlayer;							// 전체 Player 객체
	std::unordered_set<Player*> m_Sector[dfSECTOR_Y_MAX][dfSECTOR_X_MAX];		// 각 섹터에 존재하는 Player 객체
	std::unordered_map<int64_t, uint64_t> m_accountNo;

	friend unsigned __stdcall JobWorkerThread(PVOID param);					// Job 일 처리 스레드
	friend unsigned __stdcall MoniteringThread(void* param);

	bool JobWorkerThread_serv();
	bool MoniterThread_serv();

	// 모니터링 관련 변수들
private:
	__int64 _totalPlayerCnt;												// player total
	__int64 m_loginPlayerCnt;

	__int64 m_loginCount;
	__int64 m_loginTPS;

	__int64 m_jobUpdatecnt;													// job 개수
	__int64 m_jobUpdateTPS;											// job thread update 횟수

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