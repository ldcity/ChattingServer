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

	// NetServer의 가상 함수 재정의
	bool OnConnectionRequest(const wchar_t* IP, unsigned short PORT);	
	void OnClientJoin(uint64_t sessionID);	
	void OnClientLeave(uint64_t sessionID);			
	void OnRecv(uint64_t sessionID, CPacket* packet);
	void OnError(int errorCode, const wchar_t* msg);

	//--------------------------------------------------------------------------------------
	// player 관련 함수
	//--------------------------------------------------------------------------------------
	// sessionID를 사용하여 player 검색
	inline Player* FindPlayer(uint64_t sessionID)
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

	// player 생성
	bool CreatePlayer(uint64_t sessionID);		

	// player 삭제
	bool DeletePlayer(uint64_t sessionID);									
	
	// player 중복 체크
	inline bool CheckPlayer(Player* player, INT64 accountNo)
	{
		// accountNo 중복체크
		AcquireSRWLockShared(&accountNoMapLock);
		auto accountIter = m_accountNo.find(accountNo);
		if (accountIter != m_accountNo.end())
		{
			Player* dupPlayer = FindPlayer(accountIter->second);

			if (dupPlayer == nullptr)
				return false;

			m_accountNo.erase(accountIter);
			ReleaseSRWLockShared(&accountNoMapLock);

			DisconnectSession(dupPlayer->sessionID);

			return true;
		}

		ReleaseSRWLockShared(&accountNoMapLock);
		return true;
	}


	//--------------------------------------------------------------------------------------
	// Packet Proc
	//--------------------------------------------------------------------------------------
	bool PacketProc(uint64_t sessionID, CPacket* packet);					// Packet Handler
	void netPacketProc_Login(Player* player, CPacket* packet);			// 로그인 요청
	void netPacketProc_SectorMove(Player* player, CPacket* packet);		// 섹터 이동 요청
	void netPacketProc_Chatting(Player* player, CPacket* packet);		// 채팅 보내기
	void netPacketProc_HeartBeat(Player* player, CPacket* packet);		// 하트비트

private:
	//--------------------------------------------------------------------------------------
	// Job Info
	//--------------------------------------------------------------------------------------
	enum JobType
	{
		NEW_CONNECT,			// 새 접속
		DISCONNECT,				// 접속 해제
		MSG_PACKET				// 패킷 처리
	};

	// Job 구조체
	struct ChatJob
	{
		uint64_t sessionID;
		WORD type;				// JobType
		CPacket* packet;
	};

private:
	Log* chatLog;

	int m_userMAXCnt;									// 최대 player 수
	int m_timeout;										// 타임아웃 시간

	HANDLE m_jobHandle;
	HANDLE m_jobEvent;

	HANDLE m_moniteringThread;							// Monitering Thread

	HANDLE m_moniterEvent;								// Monitering Event
	HANDLE m_runEvent;									// Thread Start Event

	TLSObjectPool<Player> playerPool = TLSObjectPool<Player>(200);		// Player 객체를 담는 Pool
	//TLSObjectPool<ChatJob> jobPool = TLSObjectPool<ChatJob>(300);		// OnRecv를 통해 들어온 작업 처리를 위한 Job Pool
	//LockFreeQueue<ChatJob*> chatJobQ = LockFreeQueue<ChatJob*>(30000);	// Job Update Thread에서 JobQ에 쌓인 Job 처리

	unordered_map<uint64_t, Player*> m_mapPlayer;							// 전체 Player 객체
	//unordered_set<Player*> m_Sector[dfSECTOR_Y_MAX][dfSECTOR_X_MAX];		// 각 섹터에 존재하는 Player 객체
	Sector m_Sector[dfSECTOR_Y_MAX][dfSECTOR_X_MAX];						// 섹터

	// 중복 account 확인을 위해 중복을 허용하는 multimap으로 사용
	// -> 이렇게 하지 않으면 중복 account 인걸 확인하여 이전 세션을 disconnect 한 후,
	// m_accountNo에 새롭게 할당된 player의 accountNo(같은 번호)를 insert 하려고 해도
	// onLeave에서 해당 accountNo를 제거하기 직전이면 insert되지 않음
	// 잠깐의 시간동안은 중복을 허용해야 함
	unordered_multimap<int64_t, uint64_t> m_accountNo;

	SRWLOCK playerMapLock;
	SRWLOCK accountNoMapLock;

private:
	PerformanceMonitor performMoniter;			// 성능 모니터링 정보를 얻어옴
	MonitoringLanClient lanClient;				// 채팅 서버가 모니터링 서버에 접속하여 모니터링 정보 전송하기 위해 필요

	wstring m_tempIp;
	string m_ip;

	//friend unsigned __stdcall JobWorkerThread(PVOID param);					// Job 일 처리 스레드
	friend unsigned __stdcall MoniteringThread(void* param);

	//bool JobWorkerThread_serv();
	bool MoniterThread_serv();

	// 모니터링 관련 변수들
private:
	__int64 m_totalPlayerCnt;												// player total
	__int64 m_loginPlayerCnt;
	
	__int64 m_jobUpdatecnt;													// job 개수
	__int64 m_jobThreadUpdateCnt;											// job thread update 횟수

	__int64 m_UpdateTPS;													// job thread update 횟수
	
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