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

	bool ChatServer_Start();
	bool ChatServer_Stop();

	bool OnConnectionRequest(const wchar_t* IP, unsigned short PORT) override;
	void OnClientJoin(uint64_t sessionID) override;
	void OnClientLeave(uint64_t sessionID) override;
	void OnRecv(uint64_t sessionID, CPacket* packet) override;
	void OnTimeout(uint64_t sessionID) override;
	void OnError(int errorCode, const wchar_t* msg) override;

	//--------------------------------------------------------------------------------------
	// player 관련 함수
	//--------------------------------------------------------------------------------------
	Player* FindPlayer(uint64_t sessionID)							// player 검색
	{
		Player* player = nullptr;

		auto iter = _mapPlayer.find(sessionID);

		if (iter == _mapPlayer.end())
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
		auto accountIter = _accountNo.find(accountNo);
		if (accountIter != _accountNo.end())
		{
			Player* dupPlayer = FindPlayer(accountIter->second);

			if (dupPlayer == nullptr)
			{
				return false;
			}

			DisconnectSession(dupPlayer->sessionID);
		}

		_accountNo.insert({ accountNo, sessionID });

		return true;
	}

	//--------------------------------------------------------------------------------------
	// Packet Proc
	//--------------------------------------------------------------------------------------
	bool Packet_Proc(uint64_t sessionID, CPacket* packet);
	void NetPacketProc_Login(uint64_t sessionID, CPacket* packet);			// 로그인 요청
	void NetPacketProc_ResLogin(uint64_t sessionID, CPacket* packet);		// 로그인 응답
	void NetPacketProc_SectorMove(uint64_t sessionID, CPacket* packet);		// 섹터 이동 요청
	void NetPacketProc_Chatting(uint64_t sessionID, CPacket* packet);		// 채팅 보내기
	void NetPacketProc_HeartBeat(uint64_t sessionID, CPacket* packet);		// 하트비트

	friend unsigned __stdcall JobWorkerThread(PVOID param);					// Job 일 처리 스레드
	friend unsigned __stdcall MoniteringThread(void* param);

	bool JobWorkerThread_Serv();
	bool MoniterThread_Serv();

private:
	RedisWorkerThread* redisWorkerThread;

private:
	Log* _chatLog;

	int _userMAXCnt;										
	int _timeout;										// 타임아웃 시간

	HANDLE _jobHandle;
	HANDLE _jobEvent;

	HANDLE _moniteringThread;							// Monitering Thread

	HANDLE _moniterEvent;								// Monitering Event
	HANDLE _runEvent;									// Thread Start Event

	TLSObjectPool<Player> _playerPool = TLSObjectPool<Player>(190);

	TLSObjectPool<ChatJob> _jobPool = TLSObjectPool<ChatJob>(290);
	LockFreeQueue<ChatJob*> _chatJobQ = LockFreeQueue<ChatJob*>(15000);

	std::unordered_map<uint64_t, Player*> _mapPlayer;							// 전체 Player 객체
	std::unordered_set<Player*> _sector[dfSECTOR_Y_MAX][dfSECTOR_X_MAX];		// 각 섹터에 존재하는 Player 객체
	std::unordered_map<int64_t, uint64_t> _accountNo;

// 모니터링 관련 변수들
private:
	__int64 _totalPlayerCnt;												
	__int64 _loginPlayerCnt;
	__int64 _loginCount;
	__int64 _loginTPS;
	__int64 _jobUpdatecnt;												
	__int64 _jobUpdateTPS;			
	__int64 _loginPacketTPS;
	__int64 _sectorMovePacketTPS;
	__int64 _chattingReqTPS;
	__int64 _chattingResTPS;
	__int64 _deletePlayerCnt;
	__int64 _deletePlayerTPS;
	__int64 _redisJobThreadUpdateTPS;
	__int64 _jobThreadUpdateCnt;
	__int64 _loginResJobUpdateTPS;

private:
	PerformanceMonitor _performMoniter;
	MonitoringLanClient _lanClient;

	std::wstring _tempIp;
	std::string _ip;

private:
	bool _startFlag;
};


#endif