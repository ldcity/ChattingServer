#include "PCH.h"

#include "ChatServer.h"


// Job Worker Thread
unsigned __stdcall JobWorkerThread(PVOID param)
{
	ChatServer* chatServ = (ChatServer*)param;

	chatServ->JobWorkerThread_serv();

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

	// m_Sector �����ؾ���

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
	CloseHandle(m_moniteringThread);
	CloseHandle(m_moniterEvent);
	CloseHandle(m_runEvent);

}

// ä�ü��� ����
bool ChatServer::ChatServerStart()
{
	// ä�ü��� ���� �ð� ���
	SYSTEMTIME stNowTime;
	GetLocalTime(&stNowTime);
	wsprintfW(startTime, L"%d%02d%02d_%02d.%02d.%02d",
		stNowTime.wYear, stNowTime.wMonth, stNowTime.wDay, stNowTime.wHour, stNowTime.wMinute, stNowTime.wSecond);

	chatLog = new Log(L"ChatServer");

	// chatting server ���������� Parsing�Ͽ� �о��
	TextParser chatServerInfoTxt;
	const wchar_t* txtName = L"ChatServer.txt";
	chatServerInfoTxt.LoadFile(txtName);

	wchar_t ip[16];
	chatServerInfoTxt.GetValue(L"SERVER.BIND_IP", ip);

	m_tempIp = ip;
	int len = WideCharToMultiByte(CP_UTF8, 0, m_tempIp.c_str(), -1, NULL, 0, NULL, NULL);
	string result(len - 1, '\0');
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
	lanClient.MonitoringLanClientStart();

	// Network Logic Start
	bool ret = this->Start(ip, port, workerThread, runningThread, nagleOff, zeroCopyOff, sessionMAXCnt, packet_code, packet_key, m_timeout);

	if (!ret)
	{
		chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"NetServer Start Error");
		return false;
	}

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

	WaitForSingleObject(m_moniteringThread, INFINITE);
	WaitForSingleObject(m_jobHandle, INFINITE);

	return true;
}

bool ChatServer::ChatServerStop()
{
	// Sector & Player ��ü ����

	// ChatServer ���ҽ� ����
	CloseHandle(m_jobHandle);
	CloseHandle(m_jobEvent);

	// NetServer ����
	this->Stop();

	return true;
}

// Monitering Thread
bool ChatServer::MoniterThread_serv()
{
	DWORD threadID = GetCurrentThreadId();

	while (true)
	{
		// 1�ʸ��� ����͸� -> Ÿ�Ӿƿ� �ǵ� ó��
		DWORD ret = WaitForSingleObject(m_moniterEvent, 1000);

		if (ret == WAIT_TIMEOUT)
		{			
			__int64 chatReq = InterlockedExchange64(&m_chattingReqTPS, 0);
			__int64 chatRes = InterlockedExchange64(&m_chattingResTPS, 0);

			// ����͸� ���� ���ۿ� ������
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
			wprintf(L"[Update               ] Enq Cnt  : %10I64d   Thread Cnt : %10I64d\n", InterlockedExchange64(&m_jobUpdatecnt, 0), iJobThreadUpdateCnt);
			wprintf(L"[Job Pool             ] Capacity : %10llu   Use        : %10llu    Alloc : %10llu    Free : %10llu\n",
				jobPoolCapacity, jobPoolUseCnt, jobPoolAllocCnt, jobPoolFreeCnt);
			wprintf(L"[Player Pool          ] Capacity : %10llu   Use        : %10llu    Alloc : %10llu    Free : %10llu\n",
				playerPool.GetCapacity(), playerPool.GetObjectUseCount(), playerPool.GetObjectAllocCount(), playerPool.GetObjectFreeCount());
			wprintf(L"[Packet Pool          ] Capacity : %10llu   Use        : %10llu    Alloc : %10llu    Free : %10llu\n",
				packetPoolCapacity, packetPoolUseCnt, packetPoolAllocCnt, packetPoolFreeCnt);
			wprintf(L"[Packet List          ] Login    : %10I64d   SectorMove : %10I64d    Chat  : %10I64d (Aroung Avg : %.2f)\n",
				InterlockedExchange64(&m_loginPacketTPS, 0), InterlockedExchange64(&m_sectorMovePacketTPS, 0), chatReq, (double)chatRes / chatReq);
			wprintf(L"[Player               ] Create   : %10I64d   Login      : %10I64d\n", m_totalPlayerCnt, iLoginPlayerCnt);
			wprintf(L"[Delete               ] Total    : %10I64d   TPS        : %10I64d\n", m_deletePlayerCnt, InterlockedExchange64(&m_deletePlayerTPS, 0));
			wprintf(L"==============================================================\n\n");

			// ����͸� ������ ������ ����
			int iTime = (int)time(NULL);
			BYTE serverNo = SERVERTYPE::CHAT_SERVER_TYPE;

			// ChatServer ���� ���� ON / OFF
			CPacket* onPacket = CPacket::Alloc();
			lanClient.mpUpdateDataToMonitorServer(serverNo, MONITOR_DATA_TYPE_CHAT_SERVER_RUN, true, iTime, onPacket);
			lanClient.SendPacket(onPacket);
			CPacket::Free(onPacket);

			// ChatServer CPU ����
			CPacket* cpuPacket = CPacket::Alloc();
			lanClient.mpUpdateDataToMonitorServer(serverNo, MONITOR_DATA_TYPE_CHAT_SERVER_CPU, (int)performMoniter.GetProcessCpuTotal(), iTime, cpuPacket);
			lanClient.SendPacket(cpuPacket);
			CPacket::Free(cpuPacket);

			// ChatServer �޸� ��� MByte
			CPacket* memoryPacket = CPacket::Alloc();
			lanClient.mpUpdateDataToMonitorServer(serverNo, MONITOR_DATA_TYPE_CHAT_SERVER_MEM, (int)performMoniter.GetProcessUserMemoryByMB(), iTime, memoryPacket);
			lanClient.SendPacket(memoryPacket);
			CPacket::Free(memoryPacket);

			// ChatServer ���� �� (���ؼ� ��)
			CPacket* sessionPacket = CPacket::Alloc();
			lanClient.mpUpdateDataToMonitorServer(serverNo, MONITOR_DATA_TYPE_CHAT_SESSION, (int)iSessionCnt, iTime, sessionPacket);
			lanClient.SendPacket(sessionPacket);
			CPacket::Free(sessionPacket);

			// ChatServer �������� ����� �� (���� ������)
			CPacket* authPacket = CPacket::Alloc();
			lanClient.mpUpdateDataToMonitorServer(serverNo, MONITOR_DATA_TYPE_CHAT_PLAYER, (int)iLoginPlayerCnt, iTime, authPacket);
			lanClient.SendPacket(authPacket);
			CPacket::Free(authPacket);

			// ChatServer UPDATE ������ �ʴ� �ʸ� Ƚ��
			CPacket* updataPacket = CPacket::Alloc();
			lanClient.mpUpdateDataToMonitorServer(serverNo, MONITOR_DATA_TYPE_CHAT_UPDATE_TPS, (int)iJobThreadUpdateCnt, iTime, updataPacket);
			lanClient.SendPacket(updataPacket);
			CPacket::Free(updataPacket);

			// ChatServer ��ŶǮ ��뷮
			CPacket* poolPacket = CPacket::Alloc();
			lanClient.mpUpdateDataToMonitorServer(serverNo, MONITOR_DATA_TYPE_CHAT_PACKET_POOL, (int)packetPoolUseCnt, iTime, poolPacket);
			lanClient.SendPacket(poolPacket);
			CPacket::Free(poolPacket);

			// ChatServer UPDATE MSG Ǯ ��뷮
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
	while (true)
	{
		// JobQ�� Job�� ���ԵǸ� �̺�Ʈ �߻��Ͽ� ���
		WaitForSingleObject(m_jobEvent, INFINITE);

		ChatJob* chatJob = nullptr;

		// Job�� ���� ������ update �ݺ�
		while (chatJobQ.GetSize() > 0)
		{
			if (chatJobQ.Dequeue(chatJob))
			{
				// Job Type�� ���� �б� ó��
				switch (chatJob->type)
				{
				case JobType::NEW_CONNECT:
					CreatePlayer(chatJob->sessionID);				// Player ����
					break;

				case JobType::DISCONNECT:
					DeletePlayer(chatJob->sessionID);				// Player ����
					break;

				case JobType::MSG_PACKET:
					PacketProc(chatJob->sessionID, chatJob->packet); // ��Ŷ ó��
					break;

				default:
					DisconnectSession(chatJob->sessionID);
					break;
				}

				// ����, ���� Job�� packet�� nullptr�̱� ������ ��ȯ�� Packet�� ����
				if (chatJob->packet != nullptr)
					CPacket::Free(chatJob->packet);

				// JobPool�� Job ��ü ��ȯ
				jobPool.Free(chatJob);

				InterlockedIncrement64(&m_jobThreadUpdateCnt);
			}
		}
	}
}

// Packet Type�� �´� Handler ȣ��
bool ChatServer::PacketProc(uint64_t sessionID, CPacket* packet)
{
	WORD type;
	*packet >> type;

	switch (type)
	{
	case en_PACKET_CS_CHAT_REQ_LOGIN:
	{
		netPacketProc_Login(sessionID, packet);			// �α��� ��û
	}
		break;
	case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE:
	{
		netPacketProc_SectorMove(sessionID, packet);	// ���� �̵� ��û
	}
		break;
	case en_PACKET_CS_CHAT_REQ_MESSAGE:
	{
		netPacketProc_Chatting(sessionID, packet);		// ä�� ������
	}
		break;
	case en_PACKET_CS_CHAT_REQ_HEARTBEAT:
		netPacketProc_HeartBeat(sessionID, packet);		// ��Ʈ��Ʈ
		break;
	default:
		// �߸��� ��Ŷ
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

// ���� ó�� - �� �� �� Player ��ü�� �̸� ����������
// �α��� ���� ���Ḹ �� ���ǿ� ���� timeout�� �Ǵ��ؾ��ϱ� ������ (����� Ÿ�Ӿƿ� �̱���)
void ChatServer::OnClientJoin(uint64_t sessionID)
{
	// ù ������ ����� �ĺ��� ����͸��� �����ϱ� ���� �ʿ��� flag
	if (!startFlag)
	{
		ResumeThread(m_moniteringThread);
		startFlag = true;
	}

	ChatJob* job = jobPool.Alloc();			// jobPool���� job �Ҵ�
	job->type = JobType::NEW_CONNECT;		// �� ���� ����
	job->sessionID = sessionID;				// Player�� �ο��� SessionID
	job->packet = nullptr;					// Player ���� �ÿ��� ��Ŷ �ʿ� ����

	chatJobQ.Enqueue(job);					// JobQ�� Enqueue	
	InterlockedIncrement64(&m_jobUpdatecnt);// ����͸��� ����
	SetEvent(m_jobEvent);					// Job Worker Thread Event �߻�
}

// ���� ó��
void ChatServer::OnClientLeave(uint64_t sessionID)
{
	ChatJob* job = jobPool.Alloc();			// jobPool���� job �Ҵ�
	job->type = JobType::DISCONNECT;		// ���� ���� ����
	job->sessionID = sessionID;				// Player�� �ο��� SessionID
	job->packet = nullptr;					// Player ���� �ÿ��� ��Ŷ �ʿ� ����

	chatJobQ.Enqueue(job);					// JobQ�� Enqueue
	InterlockedIncrement64(&m_jobUpdatecnt);// ����͸��� ����
	SetEvent(m_jobEvent);					// Job Worker Thread Event �߻�
}

// ��Ŷ ó��
void ChatServer::OnRecv(uint64_t sessionID, CPacket* packet)
{
	ChatJob* job = jobPool.Alloc();			// jobPool���� job �Ҵ�
	job->type = JobType::MSG_PACKET;		// ��Ŷ ó�� ����
	job->sessionID = sessionID;				// Player�� �ο��� SessionID
	job->packet = packet;					// ó���� ��Ŷ ������

	chatJobQ.Enqueue(job);					// JobQ�� Enqueue
	InterlockedIncrement64(&m_jobUpdatecnt);// ����͸��� ����
	SetEvent(m_jobEvent);					// Job Worker Thread Event �߻�
}

// Network Logic ���κ��� timeout ó���� �߻��Ǹ� timeout Handler ȣ��
void ChatServer::OnTimeout(uint64_t sessionID)
{

}

void ChatServer::OnError(int errorCode, const wchar_t* msg)
{

}


//--------------------------------------------------------------------------------------
// player ���� �Լ�
//--------------------------------------------------------------------------------------

// player ����
bool ChatServer::CreatePlayer(uint64_t sessionID)
{
	// PlayerPool���� Player �Ҵ�
	Player* player = playerPool.Alloc();

	if (player == nullptr)
	{
		chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Player Pool Alloc Failed!");

		return false;
	}

	player->sessionID = sessionID;					// sessionID ����
	player->accountNo = -1;							// ����� �α��� ��û �� �� �����̹Ƿ� -1�� �ʱ�ȭ

	memset(player->sessionKey, 0, MSG_MAX_LEN);		// ����� �α��� ��û �� �� �����̹Ƿ� 0���� �ʱ�ȭ
	wmemset(player->ID, 0, ID_MAX_LEN);				// ����� �α��� ��û �� �� �����̹Ƿ� 0���� �ʱ�ȭ
	wmemset(player->nickname, 0, NICKNAME_MAX_LEN);	// ����� �α��� ��û �� �� �����̹Ƿ� 0���� �ʱ�ȭ

	player->sectorX = -1;							// ����� ���� �̵� ��û �� �� �����̹Ƿ� -1�η� �ʱ�ȭ
	player->sectorY = -1;							// ����� ���� �̵� ��û �� �� �����̹Ƿ� -1�η� �ʱ�ȭ

	player->recvLastTime = timeGetTime();

	m_mapPlayer.insert({ sessionID, player });		// ��ü Player�� �����ϴ� map�� insert

	InterlockedIncrement64(&m_totalPlayerCnt);

	return true;
}

// player ����
bool ChatServer::DeletePlayer(uint64_t sessionID)
{
	// player �˻�
	Player* player = FindPlayer(sessionID);
	if (player == nullptr)
	{
		chatLog->logger(dfLOG_LEVEL_DEBUG, __LINE__, L"DeletePlayer # Player Not Found! ID : %016llx", sessionID);
		CRASH()
		return false;
	}

	// ���Ϳ��� �ش� player ��ü ����
	// ���� �̵��� ���� ���� ���¸� ���� x,y ��ǥ�� ��� -1�̰�, �� �� Player�� ���� ���� �������� ����
	// -> x,y ��ǥ �� �� -1�� �ƴϸ� ���Ϳ��� �̵��� �ߴٴ� �ǹ��̹Ƿ� ���Ϳ� �÷��̾� ����
	if (player->sectorX != -1 && player->sectorY != -1)
		m_Sector[player->sectorY][player->sectorX].erase(player);		// �ش� ���Ϳ��� player ����

	player->recvLastTime = timeGetTime();

	m_mapPlayer.erase(player->sessionID);								// ��ü Player ���� map���� �ش� player ����

	auto accountIter = m_accountNo.equal_range(player->accountNo);		// �ߺ� ������ ���� ���, ���� iterator�� ������ ����

	for (; accountIter.first != accountIter.second;)
	{
		// �� �ߺ��ǿ� ���ؼ��� (acocuntNo�� ������ sessionID�� �ٸ�)
		if (player->sessionID != accountIter.first->second)
		{
			chatLog->logger(dfLOG_LEVEL_DEBUG, __LINE__, L"DeletePlayer # duplicated > prevID : %016llx\tcurID : %016llx\tprevAccountNo : %IId\tcurAccountNo : %IId",
				accountIter.first->second, player->sessionID, accountIter.first->first, player->accountNo);
			++accountIter.first;
		}
		else
		{
			// �ߺ� �ǿ��� ���� �����ؾ��� ������ ����
			accountIter.first = m_accountNo.erase(accountIter.first);

			InterlockedDecrement64(&m_loginPlayerCnt);
			InterlockedIncrement64(&m_deletePlayerCnt);
			InterlockedIncrement64(&m_deletePlayerTPS);

			break;
		}
	}

	playerPool.Free(player);		// PlayerPool�� player ��ȯ

	InterlockedDecrement64(&m_totalPlayerCnt);

	return true;
}

//--------------------------------------------------------------------------------------
// Packet Proc
//--------------------------------------------------------------------------------------

// �α��� ��û
void ChatServer::netPacketProc_Login(uint64_t sessionID, CPacket* packet)
{
	// Packet ũ�⿡ ���� ���� ó�� 
	if (packet->GetDataSize() < sizeof(INT64) + ID_MAX_LEN * sizeof(wchar_t) + NICKNAME_MAX_LEN * sizeof(wchar_t) + MSG_MAX_LEN * sizeof(char))
	{
		int size = packet->GetDataSize();
		chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Login Request Packet > Size Error : %d", size);
		DisconnectSession(sessionID);

		return;
	}

	INT64 _accountNo = 0;
	BYTE status = true;

	// accountNo�� ������ȭ�ؼ� ����
	*packet >> _accountNo;

	// player ã��
	Player* player = FindPlayer(sessionID);
	if (player == nullptr)
	{
		chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Login # %016llx Player Not Found!", sessionID);
		status = false;
	}
	else
	{
		// Player accountNo �ߺ� üũ (�ߺ� �α��� Ȯ��)
		CheckPlayer(player, _accountNo);

		// ���� ���� map�� accountNo insert
		m_accountNo.insert({ _accountNo, sessionID });			

		player->recvLastTime = timeGetTime();
		player->accountNo = _accountNo;

		// ��Ŷ ���� ������ �����͵��� ��� ������ȭ�ؼ� ����
		packet->GetData((char*)player->ID, ID_MAX_LEN * sizeof(wchar_t));
		packet->GetData((char*)player->nickname, NICKNAME_MAX_LEN * sizeof(wchar_t));
		packet->GetData((char*)player->sessionKey, MSG_MAX_LEN);

		InterlockedIncrement64(&m_loginPacketTPS);
		InterlockedIncrement64(&m_loginPlayerCnt);
	}

	CPacket* resLoginPacket = CPacket::Alloc();			// ���� ��Ŷ ����

	// �α��� ���� ��Ŷ Setting
	mpResLogin(resLoginPacket, status, _accountNo);

	// �α��� ���� ��Ŷ ����
	SendPacket(sessionID, resLoginPacket);

	CPacket::Free(resLoginPacket);						// ���� ��Ŷ ��ȯ
}

// ���� �̵� ��û
void ChatServer::netPacketProc_SectorMove(uint64_t sessionID, CPacket* packet)
{
	// Packet ũ�⿡ ���� ���� ó�� 
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

	// accountNo�� ���� ��ǥ�� ������ȭ�ؼ� ����
	*packet >> accountNo >> sectorX >> sectorY;

	// Player �˻�
	Player* player = FindPlayer(sessionID);
	if (player == nullptr)
	{
		chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Move Sector Request Packet > Player Not Found");
		DisconnectSession(sessionID);
		return;
	}

	// accountNo Ȯ��
	if (player->accountNo != accountNo)
	{
		chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Move Sector Request Packet > AccountNo Not Equal");
		DisconnectSession(player->sessionID);
		return;
	}

	// ���� ���� Ȯ��
	if (sectorX >= dfSECTOR_X_MAX || sectorX < 0 || sectorY >= dfSECTOR_Y_MAX || sectorY < 0)
	{
		chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Move Sector Request Packet > Sector Bound Error");
		DisconnectSession(player->sessionID);
		return;
	}

	player->recvLastTime = timeGetTime();

	// �ش� ���Ϳ� player�� �ִ��� Ȯ��
	// ���� ���� ���� ��ǥ�� -1�� �ƴϸ� ó�� �����̴� �� �ƴϹǷ� �̵� ���� ���� ���Ϳ� ��ġ�� �ִ� player ��ü�� �����ؾ���
	if (player->sectorX != -1 && player->sectorY != -1)
	{
		// ���� ������ǥ�� ���� ���� ���� ��ǥ�� �ٸ��ٴ� ���� ���͸� �̵� �ߴٴ� �ǹ� 
		// -> ���� ������ǥ���� player ��ü ���� �� ���� ���� ��ġ�� �߰�
		if (player->sectorX != sectorX || player->sectorY != sectorY)
		{
			auto iter = m_Sector[player->sectorY][player->sectorX].find(player);

			// ���� �̵� ���� ��ü ����
			if (iter != m_Sector[player->sectorY][player->sectorX].end())
				m_Sector[player->sectorY][player->sectorX].erase(iter);

			// ���� ���� ��ǥ�� ����
			player->sectorX = sectorX;
			player->sectorY = sectorY;

			// ���� ���� ��ġ�� �߰�
			m_Sector[player->sectorY][player->sectorX].emplace(player);
		}
	}
	// ó�� ��ǥ �̵� ��, �ش� ��ǥ�� ��ü �߰�
	else
	{
		// ���� ���� ��ǥ�� ����
		player->sectorX = sectorX;
		player->sectorY = sectorY;

		// ���� ��ġ�� �߰�
		m_Sector[player->sectorY][player->sectorX].emplace(player);
	}

	InterlockedIncrement64(&m_sectorMovePacketTPS);

	CPacket* resPacket = CPacket::Alloc();			// ���� ��Ŷ ����

	mpResSectorMove(resPacket, player->accountNo, player->sectorX, player->sectorY);

	// ���� �̵� ���� ��Ŷ ����
	SendPacket(sessionID, resPacket);

	CPacket::Free(resPacket);						// ���� ��Ŷ ��ȯ
}

// ä�� ������
void ChatServer::netPacketProc_Chatting(uint64_t sessionID, CPacket* packet)
{
	// Packet ũ�⿡ ���� ���� ó�� 
	if (packet->GetDataSize() < sizeof(INT64) + sizeof(WORD) + sizeof(wchar_t))
	{
		int size = packet->GetDataSize();
		chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Chatting Request Packet > Size Error : %d", size);
		DisconnectSession(sessionID);
		return;
	}

	// Player �˻�
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

	// accountNo�� ������ȭ�ؼ� ����
	*packet >> accountNo;

	// accountNo�� �ٸ� ���
	if (accountNo != player->accountNo)
	{
		chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Chatting Request Packet > account Error\tplayer: %IId\tpacket : %IId", player->accountNo, accountNo);
		DisconnectSession(sessionID);
		return;
	}

	// ä�� �޽��� ���̸� ������ȭ�ؼ� ����
	*packet >> msgLen;

	// ��� ���̷ε� ũ��� ���� ���̷ε� ũ�Ⱑ �ٸ� ���
	if (packet->GetDataSize() != msgLen)
	{
		chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Chatting Request Packet > Payload Size Error : %d", msgLen);
		DisconnectSession(sessionID);

		return;
	}

	player->recvLastTime = timeGetTime();

	// ���� ���� Ȯ��
	if (player->sectorX >= dfSECTOR_X_MAX || player->sectorX < 0 || player->sectorY >= dfSECTOR_Y_MAX || player->sectorY < 0)
	{
		chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Chatting Request Packet > Sector Bound Error");
		DisconnectSession(player->sessionID);
		return;
	}

	InterlockedIncrement64(&m_chattingReqTPS);

	CPacket* resPacket = CPacket::Alloc();			// ����  ��Ŷ ����

	// ä�� ���� ��Ŷ Setting
	mpResChatMessage(resPacket, player->accountNo, player->ID, player->nickname, msgLen, (WCHAR*)packet->GetReadBufferPtr());
	packet->MoveReadPos(msgLen);

	// player�� �����ϴ� ������ �ֺ� 9�� ���� ���ϱ�
	st_SECTOR_AROUND sectorAround;
	GetSectorAround(player->sectorX, player->sectorY, &sectorAround);

	// �ֺ� ���Ϳ� �����ϴ� Player�鿡�� ä�� ���� ��Ŷ ����
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

	CPacket::Free(resPacket);					// ���� ��Ŷ ��ȯ
}

// ��Ʈ��Ʈ - ����� �ƹ��� ����� ���� ����
void ChatServer::netPacketProc_HeartBeat(uint64_t sessionID, CPacket* packet)
{
	// ���� ó�� -> ��Ʈ��Ʈ ��Ŷ�� Ÿ�� �ܿ� �߰����� �����Ͱ� ������ �ȵ�
	if (packet->GetDataSize() > 0)
	{
		chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"HeartBeat Request Packet > Packet is not empty");

		DisconnectSession(sessionID);

		return;
	}

	// Player �˻�
	Player* player = FindPlayer(sessionID);

	// Player�� ���µ� ��Ŷ�� ���´�???? -> error
	if (player == nullptr)
	{
		chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"HeartBeat Request Packet > Player Not Found");

		CRASH();

		return;
	}

	player->recvLastTime = timeGetTime();
}