#include "PCH.h"
#include "ChatServer.h"

#include <conio.h>


DWORD ChatServer::_RedisTlsIdx = TlsAlloc();

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

	// chat server �������� �Ľ�
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

	//wchar_t redisIP[20];
	chatServerInfoTxt.GetValue(L"REDIS.IP", redisIP);

	//int redisPort;
	chatServerInfoTxt.GetValue(L"REDIS.PORT", &redisPort);

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

	// Monitering Thread
	m_moniteringThread = (HANDLE)_beginthreadex(NULL, 0, MoniteringThread, this, CREATE_SUSPENDED, NULL);
	if (m_moniteringThread == NULL)
	{
		int threadError = GetLastError();
		chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"_beginthreadex() Error : %d", threadError);

		return false;
	}

	chatLog->logger(dfLOG_LEVEL_DEBUG, __LINE__, L"Create Moniterting Thread");

	WaitForSingleObject(m_moniteringThread, INFINITE);
	
	return true;
}

bool ChatServer::ChatServerStop()
{
	// Sector & Player ��ü ����
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


	CRedis* redis;

	// ���ҽ� ���� �۾��� ���� �ʿ��� LockFreeStack���� DBConnector ��ü pop
	while (tlsRedisObjects.Pop(&redis))
		delete redis;

	TlsFree(_RedisTlsIdx);

	// m_Sector �����ؾ���

	CloseHandle(m_moniteringThread);
	CloseHandle(m_moniterEvent);
	CloseHandle(m_runEvent);

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
			__int64 iSessionCnt = sessionCnt;
			__int64 iLoginPlayerCnt = m_loginPlayerCnt;
			__int64 iUpdateCnt = InterlockedExchange64(&m_updateTPS, 0);

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
			wprintf(L"--------------------[Chatting Contents]-----------------------\n");
			wprintf(L"[Main Job             ] TPS      : %10I64d\n", iUpdateCnt);
			wprintf(L"[Login Res Update     ] TPS      : %10I64d\n", InterlockedExchange64(&m_loginResJobUpdateTPS, 0));
			wprintf(L"[Redis Update         ] TPS      : %10I64d\n", InterlockedExchange64(&m_redisJobThreadUpdateTPS, 0));
			wprintf(L"[Player Pool          ] Capacity : %10llu     Use          : %10llu    Alloc : %10llu    Free : %10llu\n",
				playerPool.GetCapacity(), playerPool.GetObjectUseCount(), playerPool.GetObjectAllocCount(), playerPool.GetObjectFreeCount());
			wprintf(L"[Packet Pool          ] Capacity : %10llu     Use          : %10llu    Alloc : %10llu    Free : %10llu\n",
				packetPoolCapacity, packetPoolUseCnt, packetPoolAllocCnt, packetPoolFreeCnt);
			wprintf(L"[Packet List          ] Login    : %10I64d    SectorMove   : %10I64d    Chat  : %10I64d (Aroung Avg : %.2f)\n",
				InterlockedExchange64(&m_loginPacketTPS, 0), InterlockedExchange64(&m_sectorMovePacketTPS, 0), chatReq, (double)chatRes / chatReq);
			wprintf(L"[Player               ] Create   : %10I64d    Login        : %10I64d\n", m_totalPlayerCnt, iLoginPlayerCnt);
			wprintf(L"[Delete               ] Total    : %10I64d    TPS          : %10I64d\n", m_deletePlayerCnt, InterlockedExchange64(&m_deletePlayerTPS, 0));
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
			lanClient.mpUpdateDataToMonitorServer(serverNo, MONITOR_DATA_TYPE_CHAT_UPDATE_TPS, (int)iUpdateCnt, iTime, updataPacket);
			lanClient.SendPacket(updataPacket);
			CPacket::Free(updataPacket);

			// ChatServer ��ŶǮ ��뷮
			CPacket* poolPacket = CPacket::Alloc();
			lanClient.mpUpdateDataToMonitorServer(serverNo, MONITOR_DATA_TYPE_CHAT_PACKET_POOL, (int)packetPoolUseCnt, iTime, poolPacket);
			lanClient.SendPacket(poolPacket);
			CPacket::Free(poolPacket);
		}
	}

	return true;
}

bool ChatServer::OnConnectionRequest(const wchar_t* IP, unsigned short PORT)
{

	return true;
}

// ���� ó�� - �� �� Player �����⸦ �̸� ����������
// �α��� ���� ���Ḹ �� ���ǿ� ���� timeout�� �Ǵ��ؾ��ϱ� ������
void ChatServer::OnClientJoin(uint64_t sessionID)
{
	if (!startFlag)
	{
		ResumeThread(m_moniteringThread);
		startFlag = true;
	}

	CreatePlayer(sessionID);

	InterlockedIncrement64(&m_updateTPS);
}

// ���� ó��
void ChatServer::OnClientLeave(uint64_t sessionID)
{
	DeletePlayer(sessionID);

	InterlockedIncrement64(&m_updateTPS);
}

// ��Ŷ ó��
void ChatServer::OnRecv(uint64_t sessionID, CPacket* packet)
{
	WORD type;
	*packet >> type;

	switch (type)
	{
	case en_PACKET_CS_CHAT_REQ_LOGIN:
		NetPacketProc_Login(sessionID, packet);			// �α��� ��û
	break;

	case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE:
		NetPacketProc_SectorMove(sessionID, packet);	// ���� �̵� ��û
	break;

	case en_PACKET_CS_CHAT_REQ_MESSAGE:
		NetPacketProc_Chatting(sessionID, packet);		// ä�� ������
	break;

	case en_PACKET_CS_CHAT_REQ_HEARTBEAT:
		NetPacketProc_HeartBeat(sessionID, packet);		// ��Ʈ��Ʈ
	break;

	default:
		// �߸��� ��Ŷ
		chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Packet Type Error > %d", type);
		DisconnectSession(sessionID);
		break;
	}

	if (packet != nullptr)
		CPacket::Free(packet);

	InterlockedIncrement64(&m_updateTPS);
}

void ChatServer::OnJob(uint64_t sessionID, CPacket* packet)
{

}

// Network Logic ���κ��� timeout ó���� �߻��Ǹ� timeout Handler ȣ��
void ChatServer::OnTimeout(uint64_t sessionID)
{
	DisconnectSession(sessionID);

	InterlockedIncrement64(&m_updateTPS);
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
	Player* player = playerPool.Alloc();

	if (player == nullptr)
	{
		chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Player Pool Alloc Failed!");

		CRASH();

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

	AcquireSRWLockExclusive(&playerMapLock);
	m_mapPlayer.insert({ sessionID, player });		// ��ü Player�� �����ϴ� map�� insert
	ReleaseSRWLockExclusive(&playerMapLock);

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
		CRASH();
		return false;
	}

	// ���Ϳ��� �ش� player ��ü ����
	// ���� �̵��� ���� ���� ���¸� ���� x,y ��ǥ�� ��� -1�̰�, �� �� Player�� ���� ���� �������� ����
	// -> x,y ��ǥ �� �� -1�� �ƴϸ� ���Ϳ��� �̵��� �ߴٴ� �ǹ��̹Ƿ� ���Ϳ� �÷��̾� ����
	if (player->sectorX != -1 && player->sectorY != -1)
	{
		AcquireSRWLockExclusive(&m_Sector[player->sectorY][player->sectorX].sectorLock);
		m_Sector[player->sectorY][player->sectorX].playerSet.erase(player);					// �ش� ���Ϳ��� player ����
		ReleaseSRWLockExclusive(&m_Sector[player->sectorY][player->sectorX].sectorLock);
	}

	AcquireSRWLockExclusive(&playerMapLock);
	m_mapPlayer.erase(player->sessionID);								// ��ü Player ���� map���� �ش� player ����
	ReleaseSRWLockExclusive(&playerMapLock);

	AcquireSRWLockExclusive(&accountNoMapLock);
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
	ReleaseSRWLockExclusive(&accountNoMapLock);

	playerPool.Free(player);		// PlayerPool�� player ��ȯ

	InterlockedDecrement64(&m_totalPlayerCnt);

	return true;
}

//--------------------------------------------------------------------------------------
// Packet Proc
//--------------------------------------------------------------------------------------

// �α��� ��û
void ChatServer::NetPacketProc_Login(uint64_t sessionID, CPacket* packet)
{
	PRO_BEGIN(L"Login");

	// Packet ũ�⿡ ���� ���� ó�� 
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

	// accountNo�� ������ȭ�ؼ� ����
	*packet >> _accountNo;

	// player ã��
	Player* player = FindPlayer(sessionID);
	if (player == nullptr)
	{
		chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Login # %016llx Player Not Found!", sessionID);
		CRASH();
	}

	// �ߺ� �α��� üũ
	if (!CheckPlayer(sessionID, _accountNo))
		return;

	player->recvLastTime = timeGetTime();
	player->accountNo = _accountNo;

	// ��Ŷ ���� ������ �����͵��� ��� ������ȭ�ؼ� ����
	packet->GetData((char*)player->ID, ID_MAX_LEN * sizeof(wchar_t));
	packet->GetData((char*)player->nickname, NICKNAME_MAX_LEN * sizeof(wchar_t));
	packet->GetData((char*)player->sessionKey, MSG_MAX_LEN);

	player->sessionKey[MSG_MAX_LEN] = L'\0';

	// redis�� ����� ������ū�� player�� ��ū ��
	std::string sessionKeyStr;
	sessionKeyStr.assign(player->sessionKey);

	// ����
	std::string accountNoStr = std::to_string(_accountNo);

	CRedis* redis_TLS = (CRedis*)TlsGetValue(this->_RedisTlsIdx);
	if (redis_TLS == nullptr)
	{
		redis_TLS = new CRedis;
		redis_TLS->Connect(redisIP, redisPort);

		TlsSetValue(this->_RedisTlsIdx, redis_TLS);
		tlsRedisObjects.Push(redis_TLS);
	}

	auto reply = redis_TLS->syncGet(accountNoStr);

	InterlockedIncrement64(&m_redisJobThreadUpdateTPS);

	// redis�� ���� Ű�� ������ ����!
	if (reply.is_null())
	{
		status = en_PACKET_CS_MONITOR_TOOL_RES_LOGIN::dfMONITOR_TOOL_LOGIN_ERR_NOSERVER;
	}
	// redis�� ���� Ű�� �ְ�, Ŭ���̾�Ʈ�� ���� �ִ� ���� Ű�� ���ٸ� ����!
	else
	{	// ���� ��ū�� ���ڿ� ����
		std::string redisSessionKey = reply.as_string();

		// ���� ��ū�� �ٸ��� �α��� ����!
		if (redisSessionKey.compare(sessionKeyStr) != 0)
		{
			status = en_PACKET_CS_MONITOR_TOOL_RES_LOGIN::dfMONITOR_TOOL_LOGIN_ERR_NOSERVER;
		}
	}

	// �α��� ���� ��Ŷ ����
	CPacket* resLoginPacket = CPacket::Alloc();

	// �α��� ���� ��Ŷ Setting
	mpResLogin(resLoginPacket, status, _accountNo);

	// �α��� ���� ��Ŷ ����
	SendPacket(sessionID, resLoginPacket);

	// ���� ��Ŷ ��ȯ
	CPacket::Free(resLoginPacket);

}

// ���� �̵� ��û
void ChatServer::NetPacketProc_SectorMove(uint64_t sessionID, CPacket* packet)
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

		DisconnectSession(sessionID);

		return;
	}

	// ���� ���� Ȯ��
	if (sectorX >= dfSECTOR_X_MAX || sectorX < 0 || sectorY >= dfSECTOR_Y_MAX || sectorY < 0)
	{
		chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Move Sector Request Packet > Sector Bound Error");

		DisconnectSession(sessionID);

		return;
	}

	player->recvLastTime = timeGetTime();

	// �ش� ���Ϳ� player�� �ִ��� Ȯ��
	// ���� ���� ���� ��ǥ�� -1�� �ƴϸ� ó�� �����̴� �� �ƴϹǷ� �̵� ���� ���� ���Ϳ� ��ġ�� �ִ� player ��ü�� �����ؾ���
	// -> �� ���Ϳ� ���� lock�� �ɾ�� ��
	if (player->sectorX != -1 && player->sectorY != -1)
	{
		// ���� ������ǥ�� ���� ���� ���� ��ǥ�� �ٸ��ٴ� ���� ���͸� �̵� �ߴٴ� �ǹ� 
		// -> ���� ������ǥ���� player ��ü ���� �� ���� ���� ��ġ�� �߰�
		if (player->sectorX != sectorX || player->sectorY != sectorY)
		{
			int curX = player->sectorX;
			int curY = player->sectorY;

			// ���� ���� ��ǥ�� ����
			player->sectorX = sectorX;
			player->sectorY = sectorY;

			// ���� ���Ϳ� �̵� ���Ϳ� ��� lock �ɾ�� ��
			while (true)
			{
				// lock ���� ������ ��� ����
				if (false == TryAcquireSRWLockExclusive(&m_Sector[curY][curX].sectorLock))
					continue;

				// lock ���� ������ ��� ���� (���� ���Ϳ� ���� lock�� ���� ����
				if (false == TryAcquireSRWLockExclusive(&m_Sector[sectorY][sectorX].sectorLock))
				{
					// ���� ���Ϳ� ���� ���� lock�� �����ؾ� ��
					ReleaseSRWLockExclusive(&m_Sector[curY][curX].sectorLock);

					// ���� ���� thread���� �� ���� ������ lock�� ��� �Ϳ� ���� ��������
					// ��� ���ѷ��� ������ thread ������ �������ϰ� �ǹǷ� �ٸ� thread�� ���� �ѱ�
					YieldProcessor();

					continue;
				}

				// �� ���Ϳ� ���� lock�� ��� ���� ����

				// ���� ���Ϳ� �ִ� player ��ü ã�Ƽ� ����
				auto iter = m_Sector[curY][curX].playerSet.find(player);

				if (iter != m_Sector[curY][curX].playerSet.end())
					m_Sector[curY][curX].playerSet.erase(iter);

				// ���� ���� ��ġ�� �߰�
				m_Sector[player->sectorY][player->sectorX].playerSet.emplace(player);

				ReleaseSRWLockExclusive(&m_Sector[curY][curX].sectorLock);
				ReleaseSRWLockExclusive(&m_Sector[player->sectorY][player->sectorX].sectorLock);

				break;
			}
		}
	}
	// ó�� ��ǥ �̵� ��, �ش� ��ǥ�� ��ü �߰�
	else
	{
		// ���� ���� ��ǥ�� ����
		player->sectorX = sectorX;
		player->sectorY = sectorY;

		AcquireSRWLockExclusive(&m_Sector[player->sectorY][player->sectorX].sectorLock);

		// ���� ��ġ�� �߰�
		m_Sector[player->sectorY][player->sectorX].playerSet.emplace(player);

		ReleaseSRWLockExclusive(&m_Sector[player->sectorY][player->sectorX].sectorLock);
	}

	InterlockedIncrement64(&m_sectorMovePacketTPS);

	CPacket* resPacket = CPacket::Alloc();			// ���� ��Ŷ ����

	mpResSectorMove(resPacket, player->accountNo, player->sectorX, player->sectorY);

	// ���� �̵� ���� ��Ŷ ����
	SendPacket(sessionID, resPacket);

	CPacket::Free(resPacket);						// ���� ��Ŷ ��ȯ
}

// ä�� ������
void ChatServer::NetPacketProc_Chatting(uint64_t sessionID, CPacket* packet)
{
	// PRO_BEGIN(L"Chat");

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
		chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Chatting Request Packet > account Error\tplayer: %IId\tpacket : %IId",
			player->accountNo, accountNo);

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

		DisconnectSession(sessionID);

		return;
	}

	InterlockedIncrement64(&m_chattingReqTPS);

	CPacket* resPacket = CPacket::Alloc();

	mpResChatMessage(resPacket, player->accountNo, player->ID, player->nickname, msgLen, (WCHAR*)packet->GetReadBufferPtr());
	packet->MoveReadPos(msgLen);

	// player�� �����ϴ� ������ �ֺ� 9�� ���� ���ϱ�
	st_SECTOR_AROUND sectorAround;
	GetSectorAround(player->sectorX, player->sectorY, &sectorAround);
	
	// �ֺ� sector lock
	for (int i = 0; i < sectorAround.iCount; i++)
		AcquireSRWLockShared(&m_Sector[sectorAround.Around[i].y][sectorAround.Around[i].x].sectorLock);

	// �ֺ� ���Ϳ� �����ϴ� Player�鿡�� ä�� ���� ��Ŷ ����
	for (int i = 0; i < sectorAround.iCount; i++)
	{
		auto iter = m_Sector[sectorAround.Around[i].y][sectorAround.Around[i].x].playerSet.begin();
		for (; iter != m_Sector[sectorAround.Around[i].y][sectorAround.Around[i].x].playerSet.end();)
		{
			Player* otherPlayer = *iter;
			++iter;

			InterlockedIncrement64(&m_chattingResTPS);

			SendPacket(otherPlayer->sessionID, resPacket);
		}
	}

	// �ֺ� sector unlock
	for (int i = 0; i < sectorAround.iCount; i++)
		ReleaseSRWLockShared(&m_Sector[sectorAround.Around[i].y][sectorAround.Around[i].x].sectorLock);

	CPacket::Free(resPacket);					// ���� ��Ŷ ��ȯ
}

// ��Ʈ��Ʈ - ����� �ƹ��� ����� ���� ����
void ChatServer::NetPacketProc_HeartBeat(uint64_t sessionID, CPacket* packet)
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

		DisconnectSession(player->sessionID);

		return;
	}

	player->recvLastTime = timeGetTime();
}
