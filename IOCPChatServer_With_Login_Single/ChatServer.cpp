#include "PCH.h"
#include "ChatServer.h"
#include <conio.h>

// Job Worker Thread
unsigned __stdcall JobWorkerThread(PVOID param)
{
	ChatServer* chatServ = (ChatServer*)param;

	chatServ->JobWorkerThread_Serv();

	return 0;
}

// Worker Thread Call
unsigned __stdcall MoniteringThread(void* param)
{
	ChatServer* chatServ = (ChatServer*)param;

	chatServ->MoniterThread_Serv();

	return 0;
}

ChatServer::ChatServer() : _startFlag(false)
{

}

ChatServer::~ChatServer()
{
	ChatServer_Stop();

}

bool ChatServer::ChatServer_Start()
{
	_chatLog = new Log(L"CharServer");

	// chat server 설정파일 파싱
	TextParser chatServerInfoTxt;

	const wchar_t* txtName = L"ChatServer.txt";
	chatServerInfoTxt.LoadFile(txtName);

	wchar_t ip[256];
	chatServerInfoTxt.GetValue(L"SERVER.BIND_IP", ip);

	_tempIp = ip;
	int len = WideCharToMultiByte(CP_UTF8, 0, _tempIp.c_str(), -1, NULL, 0, NULL, NULL);
	std::string result(len - 1, '\0');
	WideCharToMultiByte(CP_UTF8, 0, _tempIp.c_str(), -1, &result[0], len, NULL, NULL);
	_ip = result;

	_performMoniter.AddInterface(_ip);

	int port;
	chatServerInfoTxt.GetValue(L"SERVER.BIND_PORT", &port);

	int workerThread;
	chatServerInfoTxt.GetValue(L"SERVER.IOCP_WORKER_THREAD", &workerThread);

	int runningThread;
	chatServerInfoTxt.GetValue(L"SERVER.IOCP_ACTIVE_THREAD", &runningThread);

	int nagleOff;
	chatServerInfoTxt.GetValue(L"SERVER.NAGLE_OFF", &nagleOff);

	int zeroCopyOff;
	chatServerInfoTxt.GetValue(L"SERVER.ZEROCOPY_OFF", &zeroCopyOff);

	int sessionMAXCnt;
	chatServerInfoTxt.GetValue(L"SERVER.SESSION_MAX", &sessionMAXCnt);

	chatServerInfoTxt.GetValue(L"SERVER.USER_MAX", &_userMAXCnt);

	int packet_code;
	chatServerInfoTxt.GetValue(L"SERVER.PACKET_CODE", &packet_code);

	int packet_key;
	chatServerInfoTxt.GetValue(L"SERVER.PACKET_KEY", &packet_key);

	chatServerInfoTxt.GetValue(L"SERVICE.TIMEOUT_DISCONNECT", &_timeout);

	// Chatting Lan Client Start
	bool clientRet = _lanClient.MonitoringLanClientStart();

	if (!clientRet)
	{
		_chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"LanClient Start Error");
		return false;
	}

	bool ret = this->Start(ip, port, workerThread, runningThread, nagleOff, zeroCopyOff, sessionMAXCnt, packet_code, packet_key, _timeout);

	if (!ret)
	{
		_chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"NetServer Start Error");
		return false;
	}

	wchar_t redisIP[20];
	chatServerInfoTxt.GetValue(L"REDIS.IP", redisIP);

	int redisPort;
	chatServerInfoTxt.GetValue(L"REDIS.PORT", &redisPort);

	// Redis Worker Thread 생성 및 시작 - RedisConnector 객체 생성 및 Redis 연결
	redisWorkerThread = new RedisWorkerThread(this, redisIP, redisPort);

	if (redisWorkerThread == nullptr ||
		!redisWorkerThread->StartThread(RedisWorkerThread::ThreadFunction, redisWorkerThread))
	{
		_chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Redis Connect Failed...");
		return false;
	}

	// Create Manual Event
	_runEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (_runEvent == NULL)
	{
		int eventError = WSAGetLastError();
		_chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"CreateEvent() Error : %d", eventError);

		return false;
	}

	// Create Auto Event
	_moniterEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (_moniterEvent == NULL)
	{
		int eventError = WSAGetLastError();
		_chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"CreateEvent() Error : %d", eventError);

		return false;
	}

	// Create Auto Event
	_jobEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (_jobEvent == NULL)
	{
		int eventError = WSAGetLastError();
		_chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"CreateEvent() Error : %d", eventError);

		return false;
	}

	// Monitering Thread
	_moniteringThread = (HANDLE)_beginthreadex(NULL, 0, MoniteringThread, this, CREATE_SUSPENDED, NULL);
	if (_moniteringThread == NULL)
	{
		int threadError = GetLastError();
		_chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"_beginthreadex() Error : %d", threadError);

		return false;
	}

	_chatLog->logger(dfLOG_LEVEL_DEBUG, __LINE__, L"Create Moniterting Thread");

	// Job Worker Thread
	_jobHandle = (HANDLE)_beginthreadex(NULL, 0, JobWorkerThread, this, 0, NULL);
	if (_jobHandle == NULL)
	{
		int threadError = GetLastError();
		_chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"_beginthreadex() Error : %d", threadError);
		return false;
	}

	_chatLog->logger(dfLOG_LEVEL_DEBUG, __LINE__, L"Create Job Worker Thread");

	_chatLog->logger(dfLOG_LEVEL_DEBUG, __LINE__, L"Create Redis Job Worker Thread");

	WaitForSingleObject(_moniteringThread, INFINITE);
	WaitForSingleObject(_jobHandle, INFINITE);

	return true;
}

bool ChatServer::ChatServer_Stop()
{
	if (_mapPlayer.size() > 0)
	{
		for (auto iter = _mapPlayer.begin(); iter != _mapPlayer.end();)
		{
			_playerPool.Free(iter->second);
			iter = _mapPlayer.erase(iter);
		}
	}

	// _Sector 삭제해야함

	if (_chatJobQ.GetSize() > 0)
	{
		ChatJob* job = nullptr;
		while (_chatJobQ.Dequeue(job))
		{
			_jobPool.Free(job);
		}
	}

	CloseHandle(_jobHandle);
	CloseHandle(_jobEvent);
	CloseHandle(_moniteringThread);
	CloseHandle(_moniterEvent);
	CloseHandle(_runEvent);

	if (redisWorkerThread)
		delete redisWorkerThread;

	// NetServer 종료
	this->Stop();

	return true;
}

// Monitering Thread
bool ChatServer::MoniterThread_Serv()
{
	DWORD threadID = GetCurrentThreadId();

	while (true)
	{
		// 1초마다 모니터링 -> 타임아웃 건도 처리
		DWORD ret = WaitForSingleObject(_moniterEvent, 1000);

		if (ret == WAIT_TIMEOUT)
		{			
			__int64 chatReq = InterlockedExchange64(&_chattingReqTPS, 0);
			__int64 chatRes = InterlockedExchange64(&_chattingResTPS, 0);

			// 모니터링 서버 전송용 데이터
			__int64 iJobThreadUpdateCnt = InterlockedExchange64(&_jobUpdateTPS, 0);

			__int64 iSessionCnt = _sessionCnt;
			__int64 iLoginPlayerCnt = _loginPlayerCnt;

			__int64 jobPoolCapacity = _jobPool.GetCapacity();
			__int64 jobPoolUseCnt = _jobPool.GetObjectUseCount();
			__int64 jobPoolAllocCnt = _jobPool.GetObjectAllocCount();
			__int64 jobPoolFreeCnt = _jobPool.GetObjectFreeCount();

			__int64 packetPoolCapacity = CPacket::GetPoolCapacity();
			__int64 packetPoolUseCnt = CPacket::GetPoolUseCnt();
			__int64 packetPoolAllocCnt = CPacket::GetPoolAllocCnt();
			__int64 packetPoolFreeCnt = CPacket::GetPoolFreeCnt();

			wprintf(L"------------------------[Moniter]----------------------------\n");
			_performMoniter.PrintMonitorData();

			wprintf(L"------------------------[Network]----------------------------\n");
			wprintf(L"[Session              ] Total    : %10I64d\n", iSessionCnt);
			wprintf(L"[Accept               ] Total    : %10I64d        TPS        : %10I64d\n", _acceptCount, InterlockedExchange64(&_acceptTPS, 0));
			wprintf(L"[Release              ] Total    : %10I64d        TPS        : %10I64d\n", _releaseCount, InterlockedExchange64(&_releaseTPS, 0));
			wprintf(L"-------------------[Chatting Contents]------------------------\n");
			wprintf(L"[JobQ                 ] Main       : %10I64d      Redis     : %10I64d\n", _chatJobQ.GetSize(), redisWorkerThread->_jobQ.GetSize());
			wprintf(L"[Main Job             ] TPS        : %10I64d      Total     : %10I64d\n", iJobThreadUpdateCnt, _jobUpdatecnt);
			wprintf(L"[Redis Job            ] TPS        : %10I64d      Total     : %10I64d\n", InterlockedExchange64(&redisWorkerThread->_jobThreadUpdateTPS, 0), redisWorkerThread->_jobThreadUpdateCnt);
			wprintf(L"[Login Req Job        ] TPS        : %10I64d      Total     : %10I64d \n",
				InterlockedExchange64(&_loginTPS, 0), _loginCount);
			wprintf(L"[Login Res Job        ] TPS        : %10I64d\n", InterlockedExchange64(&_loginResJobUpdateTPS, 0));
			wprintf(L"[Job Pool Use         ] Main       : %10llu       Redis     : %10llu      Player      : %10llu\n",
				jobPoolUseCnt,  redisWorkerThread->_jobPool.GetObjectUseCount(), _playerPool.GetObjectUseCount());
			wprintf(L"[Packet Pool          ] Capacity   : %10llu       Use       : %10llu      Alloc : %10llu    Free : %10llu\n",
				packetPoolCapacity, packetPoolUseCnt, packetPoolAllocCnt, packetPoolFreeCnt);
			wprintf(L"[Packet List          ] SectorMove : %10I64d      Chat      : %10I64d (Aroung Avg : %.2f)\n",
				InterlockedExchange64(&_sectorMovePacketTPS, 0), chatReq, (double)chatRes / chatReq);
			wprintf(L"[Player               ] Create     : %10I64d      Login     : %10I64d\n", _totalPlayerCnt, iLoginPlayerCnt);
			wprintf(L"[Delete               ] Total      : %10I64d      TPS       : %10I64d\n", _deletePlayerCnt, InterlockedExchange64(&_deletePlayerTPS, 0));
			wprintf(L"==============================================================\n\n");

			// 모니터링 서버로 데이터 전송
			int iTime = (int)time(NULL);
			BYTE serverNo = SERVERTYPE::CHAT_SERVER_TYPE;

			// ChatServer 실행 여부 ON / OFF
			CPacket* onPacket = CPacket::Alloc();
			_lanClient.mpUpdateDataToMonitorServer(serverNo, MONITOR_DATA_TYPE_CHAT_SERVER_RUN, true, iTime, onPacket);
			_lanClient.SendPacket(onPacket);
			CPacket::Free(onPacket);

			// ChatServer CPU 사용률
			CPacket* cpuPacket = CPacket::Alloc();
			_lanClient.mpUpdateDataToMonitorServer(serverNo, MONITOR_DATA_TYPE_CHAT_SERVER_CPU, (int)_performMoniter.GetProcessCpuTotal(), iTime, cpuPacket);
			_lanClient.SendPacket(cpuPacket);
			CPacket::Free(cpuPacket);

			// ChatServer 메모리 사용 MByte
			CPacket* memoryPacket = CPacket::Alloc();
			_lanClient.mpUpdateDataToMonitorServer(serverNo, MONITOR_DATA_TYPE_CHAT_SERVER_MEM, (int)_performMoniter.GetProcessUserMemoryByMB(), iTime, memoryPacket);
			_lanClient.SendPacket(memoryPacket);
			CPacket::Free(memoryPacket);

			// ChatServer 세션 수 (컨넥션 수)
			CPacket* sessionPacket = CPacket::Alloc();
			_lanClient.mpUpdateDataToMonitorServer(serverNo, MONITOR_DATA_TYPE_CHAT_SESSION, (int)iSessionCnt, iTime, sessionPacket);
			_lanClient.SendPacket(sessionPacket);
			CPacket::Free(sessionPacket);

			// ChatServer 인증성공 사용자 수 (실제 접속자)
			CPacket* authPacket = CPacket::Alloc();
			_lanClient.mpUpdateDataToMonitorServer(serverNo, MONITOR_DATA_TYPE_CHAT_PLAYER, (int)iLoginPlayerCnt, iTime, authPacket);
			_lanClient.SendPacket(authPacket);
			CPacket::Free(authPacket);

			// ChatServer UPDATE 스레드 초당 초리 횟수
			CPacket* updataPacket = CPacket::Alloc();
			_lanClient.mpUpdateDataToMonitorServer(serverNo, MONITOR_DATA_TYPE_CHAT_UPDATE_TPS, (int)iJobThreadUpdateCnt, iTime, updataPacket);
			_lanClient.SendPacket(updataPacket);
			CPacket::Free(updataPacket);

			// ChatServer 패킷풀 사용량
			CPacket* poolPacket = CPacket::Alloc();
			_lanClient.mpUpdateDataToMonitorServer(serverNo, MONITOR_DATA_TYPE_CHAT_PACKET_POOL, (int)packetPoolUseCnt, iTime, poolPacket);
			_lanClient.SendPacket(poolPacket);
			CPacket::Free(poolPacket);

			// ChatServer UPDATE MSG 풀 사용량
			CPacket* jobPacket = CPacket::Alloc();
			_lanClient.mpUpdateDataToMonitorServer(serverNo, MONITOR_DATA_TYPE_CHAT_UPDATEMSG_POOL, (int)jobPoolUseCnt, iTime, jobPacket);
			_lanClient.SendPacket(jobPacket);
			CPacket::Free(jobPacket);

		}
	}

	return true;
}


void ChatServer::SendJob(uint64_t sessionID, WORD type, CPacket* packet)
{
	// 저장 성공 시, 로그인 응답 처리에 대한 작업을 Job Worker Thread로 넘김
	ChatJob* job = _jobPool.Alloc();
	job->sessionID = sessionID;
	job->type = type;
	job->packet = packet;

	_chatJobQ.Enqueue(job);
	SetEvent(_jobEvent);
}


bool ChatServer::JobWorkerThread_Serv()
{
	DWORD threadID = GetCurrentThreadId();

	while (true)
	{
		// JobQ에 Job이 삽입되면 이벤트 발생하여 깨어남
		WaitForSingleObject(_jobEvent, INFINITE);

		ChatJob* chatJob = nullptr;

		// Job이 없을 때까지 update 반복
		while (_chatJobQ.GetSize() > 0)
		{
			if (_chatJobQ.Dequeue(chatJob))
			{
				// Job Type에 따른 분기 처리
				switch (chatJob->type)
				{
				case JobType::NEW_CONNECT:
					CreatePlayer(chatJob->sessionID);					// Player 생성
					break;

				case JobType::DISCONNECT:
					DeletePlayer(chatJob->sessionID);					// Player 삭제
					break;

				case JobType::MSG_PACKET:
					Packet_Proc(chatJob->sessionID, chatJob->packet);	// 패킷 처리
					break;

				// 컨텐츠에서 발생한 로그인 응답 패킷 처리
				case JobType::LOGIN_RES:
					NetPacketProc_ResLogin(chatJob->sessionID, chatJob->packet);
					break;

				case JobType::TIMEOUT:
					DisconnectSession(chatJob->sessionID);				// 타임 아웃
					break;

				default:
					DisconnectSession(chatJob->sessionID);
					break;
				}

				// 접속, 해제 Job은 packet이 nullptr이기 때문에 반환할 Packet이 없음
				if (chatJob->packet != nullptr)
					CPacket::Free(chatJob->packet);

				// JobPool에 Job 객체 반환
				_jobPool.Free(chatJob);

				InterlockedIncrement64(&_jobUpdatecnt);
				InterlockedIncrement64(&_jobUpdateTPS);
			}
		}
	}
}

bool ChatServer::Packet_Proc(uint64_t sessionID, CPacket* packet)
{
	WORD type;
	*packet >> type;

	switch (type)
	{
	case en_PACKET_CS_CHAT_REQ_LOGIN:
		NetPacketProc_Login(sessionID, packet);					// 로그인 요청
		break;

	case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE:
		NetPacketProc_SectorMove(sessionID, packet);			// 섹터 이동 요청
		break;

	case en_PACKET_CS_CHAT_REQ_MESSAGE:
		NetPacketProc_Chatting(sessionID, packet);				// 채팅 보내기
		break;

	case en_PACKET_CS_CHAT_REQ_HEARTBEAT:
		NetPacketProc_HeartBeat(sessionID, packet);				// 하트비트
		break;

	case en_PACKET_ON_TIMEOUT:
		DisconnectSession(sessionID);					// 세션 타임아웃
		break;

	default:
		// 잘못된 패킷
		_chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Packet Type Error > %d", type);
		DisconnectSession(sessionID);
		break;
	}

	return true;
}

bool ChatServer::OnConnectionRequest(const wchar_t* IP, unsigned short PORT)
{

	return true;
}

// 접속 처리 - 이 때 Player 껍데기를 미리 만들어놔야함
// 로그인 전에 연결만 된 세션에 대해 timeout도 판단해야하기 때문에
void ChatServer::OnClientJoin(uint64_t sessionID)
{
	if (!_startFlag)
	{
		ResumeThread(_moniteringThread);
		_startFlag = true;
	}

	ChatJob* job = _jobPool.Alloc();			// jobPool에서 job 할당
	job->type = JobType::NEW_CONNECT;		// 새 접속 상태
	job->sessionID = sessionID;				// Player에 부여할 SessionID
	job->packet = nullptr;					// Player 생성 시에는 패킷 필요 없음

	_chatJobQ.Enqueue(job);					// JobQ에 Enqueue

	InterlockedIncrement64(&_jobUpdatecnt);// 모니터링용 변수
	SetEvent(_jobEvent);					// Job Worker Thread Event 발생
}

// 해제 처리
void ChatServer::OnClientLeave(uint64_t sessionID)
{
	ChatJob* job = _jobPool.Alloc();
	job->type = JobType::DISCONNECT;
	job->sessionID = sessionID;
	job->packet = nullptr;

	_chatJobQ.Enqueue(job);
	InterlockedIncrement64(&_jobUpdatecnt);
	SetEvent(_jobEvent);
}

// 패킷 처리
void ChatServer::OnRecv(uint64_t sessionID, CPacket* packet)
{
	ChatJob* job = _jobPool.Alloc();
	job->type = JobType::MSG_PACKET;
	job->sessionID = sessionID;
	job->packet = packet;

	_chatJobQ.Enqueue(job);
	InterlockedIncrement64(&_jobUpdatecnt);
	SetEvent(_jobEvent);
}

// Network Logic 으로부터 timeout 처리가 발생되면 timeout Handler 호출
void ChatServer::OnTimeout(uint64_t sessionID)
{
	
}

void ChatServer::OnError(int errorCode, const wchar_t* msg)
{

}

//--------------------------------------------------------------------------------------
// player 관련 함수
//--------------------------------------------------------------------------------------

// player 생성
bool ChatServer::CreatePlayer(uint64_t sessionID)
{
	Player* player = _playerPool.Alloc();

	if (player == nullptr)
	{
		_chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Player Pool Alloc Failed!");

		CRASH();

		return false;
	}

	player->sessionID = sessionID;					// sessionID 셋팅
	player->accountNo = -1;							// 현재는 로그인 요청 안 온 상태이므로 -1로 초기화

	memset(player->sessionKey, 0, MSG_MAX_LEN);		// 현재는 로그인 요청 안 온 상태이므로 0으로 초기화
	wmemset(player->ID, 0, ID_MAX_LEN);				// 현재는 로그인 요청 안 온 상태이므로 0으로 초기화
	wmemset(player->nickname, 0, NICKNAME_MAX_LEN);	// 현재는 로그인 요청 안 온 상태이므로 0으로 초기화

	player->sectorX = -1;							// 현재는 섹터 이동 요청 안 온 상태이므로 -1로로 초기화
	player->sectorY = -1;							// 현재는 섹터 이동 요청 안 온 상태이므로 -1로로 초기화

	player->recvLastTime = timeGetTime();

	_mapPlayer.insert({ sessionID, player });		// 전체 Player를 관리하는 map에 insert

	InterlockedIncrement64(&_totalPlayerCnt);
	InterlockedIncrement64(&_loginPlayerCnt);

	return true;
}

// player 삭제
bool ChatServer::DeletePlayer(uint64_t sessionID)
{
	// player 검색
	Player* player = FindPlayer(sessionID);

	if (player == nullptr)
	{
		_chatLog->logger(dfLOG_LEVEL_DEBUG, __LINE__, L"DeletePlayer # Player Not Found! ID : %016llx", sessionID);
		CRASH();
		return false;
	}

	// 섹터에서 해당 player 객체 삭제
	// 섹터 이동도 하지 않은 상태면 섹터 x,y 좌표가 모두 -1이고, 이 때 Player는 섹터 내에 존재하지 않음
	// -> x,y 좌표 둘 다 -1이 아니면 섹터에서 이동을 했다는 의미이므로 섹터에 플레이어 존재
	if (player->sectorX != -1 && player->sectorY != -1)
	{
		_sector[player->sectorY][player->sectorX].erase(player);
	}

	player->recvLastTime = timeGetTime();

	_mapPlayer.erase(player->sessionID);								// 전체 Player 관리 map에서 해당 player 삭제
	_accountNo.erase(player->accountNo);

	InterlockedDecrement64(&_loginPlayerCnt);
	InterlockedIncrement64(&_deletePlayerCnt);

	_playerPool.Free(player);		// PlayerPool에 player 반환

	InterlockedDecrement64(&_totalPlayerCnt);

	return true;
}

//--------------------------------------------------------------------------------------
// Packet Proc
//--------------------------------------------------------------------------------------

// 로그인 요청
void ChatServer::NetPacketProc_Login(uint64_t sessionID, CPacket* packet)
{
	InterlockedIncrement64(&_loginCount);
	InterlockedIncrement64(&_loginTPS);

	// Packet 크기에 대한 예외 처리 
	if (packet->GetDataSize() < sizeof(INT64) + ID_MAX_LEN * sizeof(wchar_t) + NICKNAME_MAX_LEN * sizeof(wchar_t) + MSG_MAX_LEN * sizeof(char))
	{
		int size = packet->GetDataSize();

		_chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Login Request Packet > Size Error : %d", size);

		DisconnectSession(sessionID);

		return;
	}

	INT64 _accountNo = 0;
	BYTE status = en_PACKET_CS_LOGIN_RES_LOGIN::dfLOGIN_STATUS_OK;

	// accountNo를 역직렬화해서 얻어옴
	*packet >> _accountNo;

	// player 찾기
	Player* player = FindPlayer(sessionID);
	if (player == nullptr)
	{
		_chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Login # %016llx Player Not Found!", sessionID);
		CRASH();
	}

	// 중복 로그인 체크
	if (!CheckPlayer(sessionID, _accountNo))
		return;

	player->recvLastTime = timeGetTime();
	player->accountNo = _accountNo;

	// 패킷 내에 나머지 데이터들을 모두 역직렬화해서 얻어옴
	packet->GetData((char*)player->ID, ID_MAX_LEN * sizeof(wchar_t));
	packet->GetData((char*)player->nickname, NICKNAME_MAX_LEN * sizeof(wchar_t));
	packet->GetData((char*)player->sessionKey, MSG_MAX_LEN);

	player->sessionKey[MSG_MAX_LEN] = L'\0';

	// redis에 저장된 인증토큰과 player의 토큰 비교
	std::string sessionKeyStr(player->sessionKey);

	redisWorkerThread->EnqueueJob(RedisWorkerThread::REDISTYPE::GET, sessionID, _accountNo, sessionKeyStr, nullptr);
}

// 섹터 이동 요청
void ChatServer::NetPacketProc_SectorMove(uint64_t sessionID, CPacket* packet)
{
	// Packet 크기에 대한 예외 처리 
	if (packet->GetDataSize() < sizeof(INT64) + sizeof(WORD) * 2)
	{
		int size = packet->GetDataSize();

		_chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Move Sector Request Packet > Size Error : %d", size);
	
		DisconnectSession(sessionID);

		return;
	}

	INT64 accountNo;
	short sectorX;
	short sectorY;

	// accountNo와 섹터 좌표를 역직렬화해서 얻어옴
	*packet >> accountNo >> sectorX >> sectorY;

	// Player 검색
	Player* player = FindPlayer(sessionID);
	if (player == nullptr)
	{
		_chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Move Sector Request Packet > Player Not Found");

		DisconnectSession(sessionID);

		return;
	}

	// accountNo 확인
	if (player->accountNo != accountNo)
	{
		_chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Move Sector Request Packet > AccountNo Not Equal");

		DisconnectSession(player->sessionID);

		return;
	}

	// 섹터 범위 확인
	if (sectorX >= dfSECTOR_X_MAX || sectorX < 0 || sectorY >= dfSECTOR_Y_MAX || sectorY < 0)
	{
		_chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Move Sector Request Packet > Sector Bound Error");

		DisconnectSession(player->sessionID);

		return;
	}

	player->recvLastTime = timeGetTime();

	// 해당 섹터에 player가 있는지 확인
	// 만약 현재 섹터 좌표가 -1이 아니면 처음 움직이는 게 아니므로 이동 전에 현재 섹터에 위치해 있는 player 객체를 삭제해야함
	if (player->sectorX != -1 && player->sectorY != -1)
	{
		// 이전 섹터좌표와 현재 얻은 섹터 좌표가 다르다는 것은 섹터를 이동 했다는 의미 
		// -> 이전 섹터좌표에서 player 객체 삭제 후 현재 섹터 위치에 추가
		if (player->sectorX != sectorX || player->sectorY != sectorY)
		{
			auto iter = _sector[player->sectorY][player->sectorX].find(player);

			// 섹터 이동 전에 객체 삭제
			if (iter != _sector[player->sectorY][player->sectorX].end())
			{
				_sector[player->sectorY][player->sectorX].erase(iter);
			}

			// 현재 섹터 좌표로 보정
			player->sectorX = sectorX;
			player->sectorY = sectorY;

			// 섹터 위치에 추가
			_sector[player->sectorY][player->sectorX].emplace(player);
		}
	}
	// 처음 좌표 이동 시, 해당 좌표에 객체 추가
	else
	{
		// 현재 섹터 좌표로 보정
		player->sectorX = sectorX;
		player->sectorY = sectorY;

		// 섹터 위치에 추가
		_sector[player->sectorY][player->sectorX].emplace(player);
	}

	InterlockedIncrement64(&_sectorMovePacketTPS);

	CPacket* resPacket = CPacket::Alloc();

	MPResSectorMove(resPacket, player->accountNo, player->sectorX, player->sectorY);
	
	// 섹터 이동 응답 패킷 전송
	SendPacket(sessionID, resPacket);

	CPacket::Free(resPacket);
}

// 채팅 보내기
void ChatServer::NetPacketProc_Chatting(uint64_t sessionID, CPacket* packet)
{
	// Packet 크기에 대한 예외 처리 
	if (packet->GetDataSize() < sizeof(INT64) + sizeof(WORD) + sizeof(wchar_t))
	{
		int size = packet->GetDataSize();
		_chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Chatting Request Packet > Size Error : %d", size);
		DisconnectSession(sessionID);

		return;
	}

	// Player 검색
	Player* player = FindPlayer(sessionID);
	if (player == nullptr)
	{
		_chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Chatting Request Packet > Player Not Found");
		DisconnectSession(sessionID);
		return;
	}

	INT64 accountNo;
	WORD msgLen;
	WCHAR* message;

	// accountNo를 역직렬화해서 얻어옴
	*packet >> accountNo;

	// accountNo가 다른 경우
	if (accountNo != player->accountNo)
	{
		_chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Chatting Request Packet > account Error\tplayer: %IId\tpacket : %IId",
			player->accountNo, accountNo);

		DisconnectSession(sessionID);

		return;
	}

	// 채팅 메시지 길이를 역직렬화해서 얻어옴
	*packet >> msgLen;

	// 헤더 페이로드 크기와 실제 페이로드 크기가 다른 경우
	if (packet->GetDataSize() != msgLen)
	{
		_chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Chatting Request Packet > Payload Size Error : %d", msgLen);

		DisconnectSession(sessionID);

		return;
	}

	player->recvLastTime = timeGetTime();

	// 섹터 범위 확인
	if (player->sectorX >= dfSECTOR_X_MAX || player->sectorX < 0 || player->sectorY >= dfSECTOR_Y_MAX || player->sectorY < 0)
	{
		_chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Chatting Request Packet > Sector Bound Error");

		DisconnectSession(player->sessionID);

		return;
	}

	InterlockedIncrement64(&_chattingReqTPS);

	CPacket* resPacket = CPacket::Alloc();

	MPResChatMessage(resPacket, player->accountNo, player->ID, player->nickname, msgLen, (WCHAR*)packet->GetReadBufferPtr());
	packet->MoveReadPos(msgLen);

	// player가 존재하는 섹터의 주변 9개 섹터 구하기
	st_SECTOR_AROUND sectorAround;
	GetSectorAround(player->sectorX, player->sectorY, sectorAround);

	// 주변 섹터에 존재하는 Player들에게 채팅 응답 패킷 전송
	for (int i = 0; i < sectorAround.iCount; i++)
	{
		auto iter = _sector[sectorAround.Around[i].y][sectorAround.Around[i].x].begin();
		for (; iter != _sector[sectorAround.Around[i].y][sectorAround.Around[i].x].end();)
		{
			Player* otherPlayer = *iter;
			++iter;

			InterlockedIncrement64(&_chattingResTPS);

			SendPacket(otherPlayer->sessionID, resPacket);
		}
	}

	CPacket::Free(resPacket);
}

// 하트비트 - 현재는 아무런 기능이 없는 상태
void ChatServer::NetPacketProc_HeartBeat(uint64_t sessionID, CPacket* packet)
{
	// 예외 처리 -> 하트비트 패킷은 타입 외에 추가적인 데이터가 있으면 안됨
	if (packet->GetDataSize() > 0)
	{
		_chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"HeartBeat Request Packet > Packet is not empty");

		DisconnectSession(sessionID);

		return;
	}

	// Player 검색
	Player* player = FindPlayer(sessionID);

	// Player가 없는데 패킷이 들어온다???? -> error
	if (player == nullptr)
	{
		_chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"HeartBeat Request Packet > Player Not Found");

		return;
	}

	player->recvLastTime = timeGetTime();
}

// 비동기 redis 요청 결과를 얻은 뒤, 이후 로그인 job 처리
void ChatServer::NetPacketProc_ResLogin(uint64_t sessionID, CPacket* packet)
{
	InterlockedIncrement64(&_loginResJobUpdateTPS);

	// 로그인 응답 패킷 전송
	SendPacket(sessionID, packet);
}