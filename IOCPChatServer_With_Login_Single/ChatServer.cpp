#include "PCH.h"
#include "ChatServer.h"

// Job Worker Thread
unsigned __stdcall JobWorkerThread(PVOID param)
{
	ChatServer* chatServ = (ChatServer*)param;

	chatServ->JobWorkerThread_serv();

	return 0;
}

// Redis Job Worker Thread
unsigned __stdcall RedisJobWorkerThread(PVOID param)
{
	ChatServer* chatServ = (ChatServer*)param;

	chatServ->RedisJobWorkerThread_serv();

	return 0;
}

// Worker Thread Call
unsigned __stdcall MoniteringThread(void* param)
{
	ChatServer* chatServ = (ChatServer*)param;

	chatServ->MoniterThread_serv();

	return 0;
}

ChatServer::ChatServer() : startFlag(false)
{

}

ChatServer::~ChatServer()
{
	ChatServerStop();

}

bool ChatServer::ChatServerStart()
{
	chatLog = new Log(L"CharServer");

	// chat server 설정파일 파싱
	TextParser chatServerInfoTxt;

	const wchar_t* txtName = L"ChatServer.txt";
	chatServerInfoTxt.LoadFile(txtName);

	wchar_t ip[256];
	chatServerInfoTxt.GetValue(L"SERVER.BIND_IP", ip);

	m_tempIp = ip;
	int len = WideCharToMultiByte(CP_UTF8, 0, m_tempIp.c_str(), -1, NULL, 0, NULL, NULL);
	std::string result(len - 1, '\0');
	WideCharToMultiByte(CP_UTF8, 0, m_tempIp.c_str(), -1, &result[0], len, NULL, NULL);
	m_ip = result;

	performMoniter.AddInterface(m_ip);

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

	chatServerInfoTxt.GetValue(L"SERVER.USER_MAX", &m_userMAXCnt);

	int packet_code;
	chatServerInfoTxt.GetValue(L"SERVER.PACKET_CODE", &packet_code);

	int packet_key;
	chatServerInfoTxt.GetValue(L"SERVER.PACKET_KEY", &packet_key);

	chatServerInfoTxt.GetValue(L"SERVICE.TIMEOUT_DISCONNECT", &m_timeout);

	// Chatting Lan Client Start
	bool clientRet = lanClient.MonitoringLanClientStart();

	if (!clientRet)
	{
		chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"LanClient Start Error");
		return false;
	}

	bool ret = this->Start(ip, port, workerThread, runningThread, nagleOff, zeroCopyOff, sessionMAXCnt, packet_code, packet_key, m_timeout);

	if (!ret)
	{
		chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"NetServer Start Error");
		return false;
	}

	wchar_t redisIP[20];
	chatServerInfoTxt.GetValue(L"REDIS.IP", redisIP);

	int redisPort;
	chatServerInfoTxt.GetValue(L"REDIS.PORT", &redisPort);

	redis = new CRedis();
	redis->Connect(redisIP, redisPort);

	// Create Manual Event
	m_runEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (m_runEvent == NULL)
	{
		int eventError = WSAGetLastError();
		chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"CreateEvent() Error : %d", eventError);

		return false;
	}

	// Create Auto Event
	m_moniterEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (m_moniterEvent == NULL)
	{
		int eventError = WSAGetLastError();
		chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"CreateEvent() Error : %d", eventError);

		return false;
	}

	// Create Auto Event
	m_jobEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (m_jobEvent == NULL)
	{
		int eventError = WSAGetLastError();
		chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"CreateEvent() Error : %d", eventError);

		return false;
	}

	// Create Auto Event
	m_redisJobEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (m_redisJobEvent == NULL)
	{
		int eventError = WSAGetLastError();
		chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"CreateEvent() Error : %d", eventError);

		return false;
	}

	// Monitering Thread
	m_moniteringThread = (HANDLE)_beginthreadex(NULL, 0, MoniteringThread, this, CREATE_SUSPENDED, NULL);
	if (m_moniteringThread == NULL)
	{
		int threadError = GetLastError();
		chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"_beginthreadex() Error : %d", threadError);

		return false;
	}

	chatLog->logger(dfLOG_LEVEL_DEBUG, __LINE__, L"Create Moniterting Thread");

	// Job Worker Thread
	m_jobHandle = (HANDLE)_beginthreadex(NULL, 0, JobWorkerThread, this, 0, NULL);
	if (m_jobHandle == NULL)
	{
		int threadError = GetLastError();
		chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"_beginthreadex() Error : %d", threadError);
		return false;
	}

	chatLog->logger(dfLOG_LEVEL_DEBUG, __LINE__, L"Create Job Worker Thread");

	// Redis Job Worker Thread
	m_redisJobHandle = (HANDLE)_beginthreadex(NULL, 0, RedisJobWorkerThread, this, 0, NULL);
	if (m_redisJobHandle == NULL)
	{
		int threadError = GetLastError();
		chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"_beginthreadex() Error : %d", threadError);

		return false;
	}

	chatLog->logger(dfLOG_LEVEL_DEBUG, __LINE__, L"Create Redis Job Worker Thread");

	WaitForSingleObject(m_moniteringThread, INFINITE);
	WaitForSingleObject(m_jobHandle, INFINITE);
	WaitForSingleObject(m_redisJobHandle, INFINITE);

	return true;
}

bool ChatServer::ChatServerStop()
{
	chatLog->~Log();
	logger->~Log();

	if (m_mapPlayer.size() > 0)
	{
		for (auto iter = m_mapPlayer.begin(); iter != m_mapPlayer.end();)
		{
			playerPool.Free(iter->second);
			iter = m_mapPlayer.erase(iter);
		}
	}

	// m_Sector 삭제해야함

	if (chatJobQ.GetSize() > 0)
	{
		ChatJob* job = nullptr;
		while (chatJobQ.Dequeue(job))
		{
			jobPool.Free(job);
		}
	}

	CloseHandle(m_jobHandle);
	CloseHandle(m_jobEvent);
	CloseHandle(m_redisJobEvent);
	CloseHandle(m_redisJobHandle);
	CloseHandle(m_moniteringThread);
	CloseHandle(m_moniterEvent);
	CloseHandle(m_runEvent);

	delete redis;

	// NetServer 종료
	this->Stop();

	return true;
}

// Monitering Thread
bool ChatServer::MoniterThread_serv()
{
	DWORD threadID = GetCurrentThreadId();

	while (true)
	{
		// 1초마다 모니터링 -> 타임아웃 건도 처리
		DWORD ret = WaitForSingleObject(m_moniterEvent, 1000);

		if (ret == WAIT_TIMEOUT)
		{			
			__int64 chatReq = InterlockedExchange64(&m_chattingReqTPS, 0);
			__int64 chatRes = InterlockedExchange64(&m_chattingResTPS, 0);

			// 모니터링 서버 전송용 데이터
			__int64 iJobThreadUpdateCnt = InterlockedExchange64(&m_jobThreadUpdateCnt, 0);

			__int64 iSessionCnt = sessionCnt;
			__int64 iLoginPlayerCnt = m_loginPlayerCnt;

			__int64 jobPoolCapacity = jobPool.GetCapacity();
			__int64 jobPoolUseCnt = jobPool.GetObjectUseCount();
			__int64 jobPoolAllocCnt = jobPool.GetObjectAllocCount();
			__int64 jobPoolFreeCnt = jobPool.GetObjectFreeCount();

			__int64 packetPoolCapacity = CPacket::GetPoolCapacity();
			__int64 packetPoolUseCnt = CPacket::GetPoolUseCnt();
			__int64 packetPoolAllocCnt = CPacket::GetPoolAllocCnt();
			__int64 packetPoolFreeCnt = CPacket::GetPoolFreeCnt();

			wprintf(L"==============================================================\n");
			wprintf(L"%s\n", startTime);

			wprintf(L"------------------------[Moniter]----------------------------\n");
			performMoniter.PrintMonitorData();

			wprintf(L"------------------------[Network]----------------------------\n");
			wprintf(L"[Session              ] Total    : %10I64d\n", iSessionCnt);
			wprintf(L"[Accept               ] Total    : %10I64d   TPS : %10I64d\n", acceptCount, InterlockedExchange64(&acceptTPS, 0));
			wprintf(L"[Release              ] Total    : %10I64d   TPS : %10I64d\n", releaseCount, InterlockedExchange64(&releaseTPS, 0));
			wprintf(L"[Recv Call            ] Total    : %10I64d   TPS : %10I64d\n", recvCallCount, InterlockedExchange64(&recvCallTPS, 0));
			wprintf(L"[Send Call            ] Total    : %10I64d   TPS : %10I64d\n", sendCallCount, InterlockedExchange64(&sendCallTPS, 0));
			wprintf(L"[Recv Bytes           ] Total    : %10I64d   TPS : %10I64d\n", recvBytes, InterlockedExchange64(&recvBytesTPS, 0));
			wprintf(L"[Send Bytes           ] Total    : %10I64d   TPS : %10I64d\n", sendBytes, InterlockedExchange64(&sendBytesTPS, 0));
			wprintf(L"[Recv  Packet         ] Total    : %10I64d   TPS : %10I64d\n", recvMsgCount, InterlockedExchange64(&recvMsgTPS, 0));
			wprintf(L"[Send  Packet         ] Total    : %10I64d   TPS : %10I64d\n", sendMsgCount, InterlockedExchange64(&sendMsgTPS, 0));
			wprintf(L"[Pending TPS          ] Recv     : %10I64d   Send: %10I64d\n", InterlockedExchange64(&recvPendingTPS, 0), InterlockedExchange64(&sendPendingTPS, 0));
			wprintf(L"------------------------[Contents]----------------------------\n");
			wprintf(L"[JobQ                 ] Size     : %10I64d\n", chatJobQ.GetSize());
			wprintf(L"[Update Job           ] Enq Cnt  : %10I64d   Thread Cnt : %10I64d\n", InterlockedExchange64(&m_jobUpdatecnt, 0), iJobThreadUpdateCnt);
			wprintf(L"[Redis Update         ] Enqueue  : %10I64d   TPS         : %10I64d\n", InterlockedExchange64(&m_redisJobEnqueueTPS, 0), InterlockedExchange64(&m_redisJobThreadUpdateTPS, 0));
			wprintf(L"[Redis Get            ] Total    : %10I64d   TPS        : %10I64d\n", m_redisGetCnt, InterlockedExchange64(&m_redisGetTPS, 0));
			wprintf(L"[Job Pool             ] Capacity : %10llu    Use        : %10llu    Alloc : %10llu    Free : %10llu\n",
				jobPoolCapacity, jobPoolUseCnt, jobPoolAllocCnt, jobPoolFreeCnt);
			wprintf(L"[Player Pool          ] Capacity : %10llu    Use        : %10llu    Alloc : %10llu    Free : %10llu\n",
				playerPool.GetCapacity(), playerPool.GetObjectUseCount(), playerPool.GetObjectAllocCount(), playerPool.GetObjectFreeCount());
			wprintf(L"[Packet Pool          ] Capacity : %10llu    Use        : %10llu    Alloc : %10llu    Free : %10llu\n",
				packetPoolCapacity, packetPoolUseCnt, packetPoolAllocCnt, packetPoolFreeCnt);
			wprintf(L"[Packet List          ] Login    : %10I64d   SectorMove : %10I64d    Chat  : %10I64d (Aroung Avg : %.2f)\n",
				InterlockedExchange64(&m_loginPacketTPS, 0), InterlockedExchange64(&m_sectorMovePacketTPS, 0), chatReq, (double)chatRes / chatReq);
			wprintf(L"[Player               ] Create   : %10I64d   Login      : %10I64d\n", m_totalPlayerCnt, iLoginPlayerCnt);
			wprintf(L"[Delete               ] Total    : %10I64d   TPS        : %10I64d\n", m_deletePlayerCnt, InterlockedExchange64(&m_deletePlayerTPS, 0));
			wprintf(L"==============================================================\n\n");

			// 모니터링 서버로 데이터 전송
			int iTime = (int)time(NULL);
			BYTE serverNo = SERVERTYPE::CHAT_SERVER_TYPE;

			// ChatServer 실행 여부 ON / OFF
			CPacket* onPacket = CPacket::Alloc();
			lanClient.mpUpdateDataToMonitorServer(serverNo, MONITOR_DATA_TYPE_CHAT_SERVER_RUN, true, iTime, onPacket);
			lanClient.SendPacket(onPacket);
			CPacket::Free(onPacket);

			// ChatServer CPU 사용률
			CPacket* cpuPacket = CPacket::Alloc();
			lanClient.mpUpdateDataToMonitorServer(serverNo, MONITOR_DATA_TYPE_CHAT_SERVER_CPU, (int)performMoniter.GetProcessCpuTotal(), iTime, cpuPacket);
			lanClient.SendPacket(cpuPacket);
			CPacket::Free(cpuPacket);

			// ChatServer 메모리 사용 MByte
			CPacket* memoryPacket = CPacket::Alloc();
			lanClient.mpUpdateDataToMonitorServer(serverNo, MONITOR_DATA_TYPE_CHAT_SERVER_MEM, (int)performMoniter.GetProcessUserMemoryByMB(), iTime, memoryPacket);
			lanClient.SendPacket(memoryPacket);
			CPacket::Free(memoryPacket);

			// ChatServer 세션 수 (컨넥션 수)
			CPacket* sessionPacket = CPacket::Alloc();
			lanClient.mpUpdateDataToMonitorServer(serverNo, MONITOR_DATA_TYPE_CHAT_SESSION, (int)iSessionCnt, iTime, sessionPacket);
			lanClient.SendPacket(sessionPacket);
			CPacket::Free(sessionPacket);

			// ChatServer 인증성공 사용자 수 (실제 접속자)
			CPacket* authPacket = CPacket::Alloc();
			lanClient.mpUpdateDataToMonitorServer(serverNo, MONITOR_DATA_TYPE_CHAT_PLAYER, (int)iLoginPlayerCnt, iTime, authPacket);
			lanClient.SendPacket(authPacket);
			CPacket::Free(authPacket);

			// ChatServer UPDATE 스레드 초당 초리 횟수
			CPacket* updataPacket = CPacket::Alloc();
			lanClient.mpUpdateDataToMonitorServer(serverNo, MONITOR_DATA_TYPE_CHAT_UPDATE_TPS, (int)iJobThreadUpdateCnt, iTime, updataPacket);
			lanClient.SendPacket(updataPacket);
			CPacket::Free(updataPacket);

			// ChatServer 패킷풀 사용량
			CPacket* poolPacket = CPacket::Alloc();
			lanClient.mpUpdateDataToMonitorServer(serverNo, MONITOR_DATA_TYPE_CHAT_PACKET_POOL, (int)packetPoolUseCnt, iTime, poolPacket);
			lanClient.SendPacket(poolPacket);
			CPacket::Free(poolPacket);

			// ChatServer UPDATE MSG 풀 사용량
			CPacket* jobPacket = CPacket::Alloc();
			lanClient.mpUpdateDataToMonitorServer(serverNo, MONITOR_DATA_TYPE_CHAT_UPDATEMSG_POOL, (int)jobPoolUseCnt, iTime, jobPacket);
			lanClient.SendPacket(jobPacket);
			CPacket::Free(jobPacket);
		}
	}

	return true;
}

bool ChatServer::JobWorkerThread_serv()
{
	DWORD threadID = GetCurrentThreadId();

	while (true)
	{
		// JobQ에 Job이 삽입되면 이벤트 발생하여 깨어남
		WaitForSingleObject(m_jobEvent, INFINITE);

		ChatJob* chatJob = nullptr;

		// Job이 없을 때까지 update 반복
		while (chatJobQ.GetSize() > 0)
		{
			if (chatJobQ.Dequeue(chatJob))
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
					PacketProc(chatJob->sessionID, chatJob->packet);	// 패킷 처리
					break;

				case JobType::REDIS_RES:
					netPacketProc_ResLoginRedis(chatJob->sessionID, chatJob->packet);	// 로그인 응답
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
				jobPool.Free(chatJob);

				InterlockedIncrement64(&m_jobThreadUpdateCnt);
			}
		}
	}
}

bool ChatServer::RedisJobWorkerThread_serv()
{
	DWORD threadID = GetCurrentThreadId();

	while (true)
	{
		// JobQ에 Job이 삽입되면 이벤트 발생하여 깨어남
		WaitForSingleObject(m_redisJobEvent, INFINITE);

		RedisJob* redisJob = nullptr;

		// Job이 없을 때까지 update 반복
		while (redisJobQ.GetSize() > 0)
		{
			if (redisJobQ.Dequeue(redisJob))
			{
				std::string accountNoStr = std::to_string(redisJob->accountNo);
				// 비동기 redis get 요청
				redis->asyncGet(accountNoStr, [=](const cpp_redis::reply& reply) {
					BYTE status = en_PACKET_CS_MONITOR_TOOL_RES_LOGIN::dfMONITOR_TOOL_LOGIN_OK;

					// redis에 인증 키가 없으면 실패!
					if (reply.is_null())
					{
						status = en_PACKET_CS_MONITOR_TOOL_RES_LOGIN::dfMONITOR_TOOL_LOGIN_ERR_NOSERVER;
					}
					// redis에 인증 키가 있고, 클라이언트가 갖고 있는 인증 키와 같다면 성공!
					else
					{	// 인증 토큰의 문자열 얻어옴
						std::string redisSessionKey = reply.as_string();

						// 인증 토큰이 다르면 로그인 실패!
						if (redisSessionKey.compare(redisJob->sessionKey) != 0)
						{
							status = en_PACKET_CS_MONITOR_TOOL_RES_LOGIN::dfMONITOR_TOOL_LOGIN_ERR_NOSERVER;
						}
					}

					// 나머지 작업 처리를 job queue로 던짐
					ChatJob* job = jobPool.Alloc();
					job->sessionID = redisJob->sessionID;
					job->type = JobType::REDIS_RES;

					// 로그인 응답 패킷 전송
					CPacket* resLoginPacket = CPacket::Alloc();

					// 로그인 응답 패킷 Setting
					mpResLogin(resLoginPacket, status, redisJob->accountNo);

					job->packet = resLoginPacket;

					chatJobQ.Enqueue(job);
					InterlockedIncrement64(&m_jobUpdatecnt);
					SetEvent(m_jobEvent);

					// JobPool에 Job 객체 반환
					redisJobPool.Free(redisJob);

					InterlockedIncrement64(&m_redisJobThreadUpdateTPS);
				});
			}			
		}
	}
}

bool ChatServer::PacketProc(uint64_t sessionID, CPacket* packet)
{
	WORD type;
	*packet >> type;

	switch (type)
	{
	case en_PACKET_CS_CHAT_REQ_LOGIN:
		netPacketProc_Login(sessionID, packet);					// 로그인 요청
		break;

	case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE:
		netPacketProc_SectorMove(sessionID, packet);			// 섹터 이동 요청
		break;

	case en_PACKET_CS_CHAT_REQ_MESSAGE:
		netPacketProc_Chatting(sessionID, packet);				// 채팅 보내기
		break;

	case en_PACKET_CS_CHAT_REQ_HEARTBEAT:
		netPacketProc_HeartBeat(sessionID, packet);				// 하트비트
		break;

	case en_PACKET_ON_TIMEOUT:
		DisconnectSession(sessionID);					// 세션 타임아웃
		break;

	default:
		// 잘못된 패킷
		chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Packet Type Error > %d", type);
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
	if (!startFlag)
	{
		ResumeThread(m_moniteringThread);
		startFlag = true;
	}

	ChatJob* job = jobPool.Alloc();				// jobPool에서 job 할당
	job->type = JobType::NEW_CONNECT;			// 새 접속 상태
	job->sessionID = sessionID;				// Player에 부여할 SessionID
	job->packet = nullptr;					// Player 생성 시에는 패킷 필요 없음

	chatJobQ.Enqueue(job);					// JobQ에 Enqueue

	InterlockedIncrement64(&m_jobUpdatecnt);수
	SetEvent(m_jobEvent);					// Job Worker Thread Event 발생
}

// 해제 처리
void ChatServer::OnClientLeave(uint64_t sessionID)
{
	ChatJob* job = jobPool.Alloc();
	job->type = JobType::DISCONNECT;
	job->sessionID = sessionID;
	job->packet = nullptr;

	chatJobQ.Enqueue(job);
	InterlockedIncrement64(&m_jobUpdatecnt);
	SetEvent(m_jobEvent);
}

// 패킷 처리
void ChatServer::OnRecv(uint64_t sessionID, CPacket* packet)
{
	ChatJob* job = jobPool.Alloc();
	job->type = JobType::MSG_PACKET;
	job->sessionID = sessionID;
	job->packet = packet;

	chatJobQ.Enqueue(job);
	InterlockedIncrement64(&m_jobUpdatecnt);
	SetEvent(m_jobEvent);
}

// Network Logic 으로부터 timeout 처리가 발생되면 timeout Handler 호출
void ChatServer::OnTimeout(uint64_t sessionID)
{
	ChatJob* job = jobPool.Alloc();
	job->type = JobType::TIMEOUT;
	job->sessionID = sessionID;
	job->packet = nullptr;

	chatJobQ.Enqueue(job);
	InterlockedIncrement64(&m_jobUpdatecnt);
	SetEvent(m_jobEvent);
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
	Player* player = playerPool.Alloc();

	if (player == nullptr)
	{
		chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Player Pool Alloc Failed!");

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

	m_mapPlayer.insert({ sessionID, player });		// 전체 Player를 관리하는 map에 insert

	InterlockedIncrement64(&m_totalPlayerCnt);

	return true;
}

// player 삭제
bool ChatServer::DeletePlayer(uint64_t sessionID)
{
	// player 검색
	Player* player = FindPlayer(sessionID);

	if (player == nullptr)
	{
		chatLog->logger(dfLOG_LEVEL_DEBUG, __LINE__, L"DeletePlayer # Player Not Found! ID : %016llx", sessionID);
		CRASH();
		return false;
	}

	// 섹터에서 해당 player 객체 삭제
	// 섹터 이동도 하지 않은 상태면 섹터 x,y 좌표가 모두 -1이고, 이 때 Player는 섹터 내에 존재하지 않음
	// -> x,y 좌표 둘 다 -1이 아니면 섹터에서 이동을 했다는 의미이므로 섹터에 플레이어 존재
	if (player->sectorX != -1 && player->sectorY != -1)
	{
		m_Sector[player->sectorY][player->sectorX].erase(player);
	}

	player->recvLastTime = timeGetTime();

	m_mapPlayer.erase(player->sessionID);				// 전체 Player 관리 map에서 해당 player 삭제
	m_accountNo.erase(player->accountNo);

	InterlockedDecrement64(&m_loginPlayerCnt);
	InterlockedIncrement64(&m_deletePlayerCnt);

	playerPool.Free(player);					// PlayerPool에 player 반환

	InterlockedDecrement64(&m_totalPlayerCnt);

	return true;
}

//--------------------------------------------------------------------------------------
// Packet Proc
//--------------------------------------------------------------------------------------

// 로그인 요청
void ChatServer::netPacketProc_Login(uint64_t sessionID, CPacket* packet)
{
	// Packet 크기에 대한 예외 처리 
	if (packet->GetDataSize() < sizeof(INT64) + ID_MAX_LEN * sizeof(wchar_t) + NICKNAME_MAX_LEN * sizeof(wchar_t) + MSG_MAX_LEN * sizeof(char))
	{
		int size = packet->GetDataSize();

		chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Login Request Packet > Size Error : %d", size);

		DisconnectSession(sessionID);

		return;
	}

	InterlockedIncrement64(&m_loginPacketTPS);
	InterlockedIncrement64(&m_loginPlayerCnt);

	INT64 _accountNo = 0;
	BYTE status = en_PACKET_CS_LOGIN_RES_LOGIN::dfLOGIN_STATUS_OK;

	// accountNo를 역직렬화해서 얻어옴
	*packet >> _accountNo;

	// player 찾기
	Player* player = FindPlayer(sessionID);
	if (player == nullptr)
	{
		chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Login # %016llx Player Not Found!", sessionID);
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
	std::string sessionKeyStr;
	sessionKeyStr.assign(player->sessionKey);

	//// 동기
	//auto reply = redis->syncGet(accountNoStr);

	//std::string redisSessionKey;

	//// 인증 토큰이 Redis에 존재함
	//if (!reply.is_null())
	//{
	//	// 인증 토큰의 문자열 얻어옴
	//	redisSessionKey = reply.as_string();

	//	InterlockedIncrement64(&m_redisGetCnt);
	//	InterlockedIncrement64(&m_redisGetTPS);
	//}

	//// redis에서 갖고온 토큰이 없거나 토큰이 다를 경우 유효하지 않은 접근
	//if (reply.is_null() || 0 != strcmp(redisSessionKey.c_str(), player->sessionKey))
	//	status = false;
	//// 로그인 유효성 판단 성공
	//else
	//	status = true;

	//InterlockedIncrement64(&m_loginPacketTPS);
	//InterlockedIncrement64(&m_loginPlayerCnt);

	//// 로그인 응답 패킷 전송
	//CPacket* resLoginPacket = CPacket::Alloc();

	//// 로그인 응답 패킷 Setting
	//mpResLogin(resLoginPacket, status, _accountNo);

	//// 로그인 응답 패킷 전송
	//SendPacket(sessionID, resLoginPacket);

	//// 응답 패킷 반환
	//CPacket::Free(resLoginPacket);

	// 비동기
	RedisJob* job = redisJobPool.Alloc();
	job->sessionID = sessionID;
	job->accountNo = _accountNo;
	job->sessionKey = sessionKeyStr;

	redisJobQ.Enqueue(job);
	SetEvent(m_redisJobEvent);
	InterlockedIncrement64(&m_redisJobEnqueueTPS);
}

// 섹터 이동 요청
void ChatServer::netPacketProc_SectorMove(uint64_t sessionID, CPacket* packet)
{
	// Packet 크기에 대한 예외 처리 
	if (packet->GetDataSize() < sizeof(INT64) + sizeof(WORD) * 2)
	{
		int size = packet->GetDataSize();

		chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Move Sector Request Packet > Size Error : %d", size);
	
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
		chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Move Sector Request Packet > Player Not Found");

		DisconnectSession(sessionID);

		return;
	}

	// accountNo 확인
	if (player->accountNo != accountNo)
	{
		chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Move Sector Request Packet > AccountNo Not Equal");

		DisconnectSession(player->sessionID);

		return;
	}

	// 섹터 범위 확인
	if (sectorX >= dfSECTOR_X_MAX || sectorX < 0 || sectorY >= dfSECTOR_Y_MAX || sectorY < 0)
	{
		chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Move Sector Request Packet > Sector Bound Error");

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
			auto iter = m_Sector[player->sectorY][player->sectorX].find(player);

			// 섹터 이동 전에 객체 삭제
			if (iter != m_Sector[player->sectorY][player->sectorX].end())
			{
				m_Sector[player->sectorY][player->sectorX].erase(iter);
			}

			// 현재 섹터 좌표로 보정
			player->sectorX = sectorX;
			player->sectorY = sectorY;

			// 섹터 위치에 추가
			m_Sector[player->sectorY][player->sectorX].emplace(player);
		}
	}
	// 처음 좌표 이동 시, 해당 좌표에 객체 추가
	else
	{
		// 현재 섹터 좌표로 보정
		player->sectorX = sectorX;
		player->sectorY = sectorY;

		// 섹터 위치에 추가
		m_Sector[player->sectorY][player->sectorX].emplace(player);
	}

	InterlockedIncrement64(&m_sectorMovePacketTPS);

	CPacket* resPacket = CPacket::Alloc();

	mpResSectorMove(resPacket, player->accountNo, player->sectorX, player->sectorY);

	// 섹터 이동 응답 패킷 전송
	SendPacket(sessionID, resPacket);

	CPacket::Free(resPacket);
}

// 채팅 보내기
void ChatServer::netPacketProc_Chatting(uint64_t sessionID, CPacket* packet)
{
	// Packet 크기에 대한 예외 처리 
	if (packet->GetDataSize() < sizeof(INT64) + sizeof(WORD) + sizeof(wchar_t))
	{
		int size = packet->GetDataSize();
		chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Chatting Request Packet > Size Error : %d", size);
		DisconnectSession(sessionID);

		return;
	}

	// Player 검색
	Player* player = FindPlayer(sessionID);
	if (player == nullptr)
	{
		chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Chatting Request Packet > Player Not Found");
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
		chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Chatting Request Packet > account Error\tplayer: %IId\tpacket : %IId",
			player->accountNo, accountNo);

		DisconnectSession(sessionID);

		return;
	}

	// 채팅 메시지 길이를 역직렬화해서 얻어옴
	*packet >> msgLen;

	// 헤더 페이로드 크기와 실제 페이로드 크기가 다른 경우
	if (packet->GetDataSize() != msgLen)
	{
		chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Chatting Request Packet > Payload Size Error : %d", msgLen);

		DisconnectSession(sessionID);

		return;
	}

	player->recvLastTime = timeGetTime();

	// 섹터 범위 확인
	if (player->sectorX >= dfSECTOR_X_MAX || player->sectorX < 0 || player->sectorY >= dfSECTOR_Y_MAX || player->sectorY < 0)
	{
		chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Chatting Request Packet > Sector Bound Error");

		DisconnectSession(player->sessionID);

		return;
	}

	InterlockedIncrement64(&m_chattingReqTPS);

	CPacket* resPacket = CPacket::Alloc();

	mpResChatMessage(resPacket, player->accountNo, player->ID, player->nickname, msgLen, (WCHAR*)packet->GetReadBufferPtr());
	packet->MoveReadPos(msgLen);

	// player가 존재하는 섹터의 주변 9개 섹터 구하기
	st_SECTOR_AROUND sectorAround;
	GetSectorAround(player->sectorX, player->sectorY, &sectorAround);

	// 주변 섹터에 존재하는 Player들에게 채팅 응답 패킷 전송
	for (int i = 0; i < sectorAround.iCount; i++)
	{
		auto iter = m_Sector[sectorAround.Around[i].y][sectorAround.Around[i].x].begin();
		for (; iter != m_Sector[sectorAround.Around[i].y][sectorAround.Around[i].x].end();)
		{
			Player* otherPlayer = *iter;
			++iter;

			InterlockedIncrement64(&m_chattingResTPS);

			SendPacket(otherPlayer->sessionID, resPacket);
		}
	}

	//resPacket->subRefCnt();
	CPacket::Free(resPacket);
}

// 하트비트 - 현재는 아무런 기능이 없는 상태
void ChatServer::netPacketProc_HeartBeat(uint64_t sessionID, CPacket* packet)
{
	// 예외 처리 -> 하트비트 패킷은 타입 외에 추가적인 데이터가 있으면 안됨
	if (packet->GetDataSize() > 0)
	{
		chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"HeartBeat Request Packet > Packet is not empty");

		DisconnectSession(sessionID);

		return;
	}

	// Player 검색
	Player* player = FindPlayer(sessionID);

	// Player가 없는데 패킷이 들어온다???? -> error
	if (player == nullptr)
	{
		chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"HeartBeat Request Packet > Player Not Found");

		DisconnectSession(player->sessionID);

		return;
	}

	player->recvLastTime = timeGetTime();
}

// 비동기 redis 요청 결과를 얻은 뒤, 이후 로그인 job 처리
void ChatServer::netPacketProc_ResLoginRedis(uint64_t sessionID, CPacket* packet)
{
	// 로그인 응답 패킷 전송
	SendPacket(sessionID, packet);
}
