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

	// chat server �������� �Ľ�
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

	// Redis Worker Thread ���� �� ���� - RedisConnector ��ü ���� �� Redis ����
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

	// _Sector �����ؾ���

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

	// NetServer ����
	this->Stop();

	return true;
}

// Monitering Thread
bool ChatServer::MoniterThread_Serv()
{
	DWORD threadID = GetCurrentThreadId();

	while (true)
	{
		// 1�ʸ��� ����͸� -> Ÿ�Ӿƿ� �ǵ� ó��
		DWORD ret = WaitForSingleObject(_moniterEvent, 1000);

		if (ret == WAIT_TIMEOUT)
		{			
			__int64 chatReq = InterlockedExchange64(&_chattingReqTPS, 0);
			__int64 chatRes = InterlockedExchange64(&_chattingResTPS, 0);

			// ����͸� ���� ���ۿ� ������
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

			// ����͸� ������ ������ ����
			int iTime = (int)time(NULL);
			BYTE serverNo = SERVERTYPE::CHAT_SERVER_TYPE;

			// ChatServer ���� ���� ON / OFF
			CPacket* onPacket = CPacket::Alloc();
			_lanClient.mpUpdateDataToMonitorServer(serverNo, MONITOR_DATA_TYPE_CHAT_SERVER_RUN, true, iTime, onPacket);
			_lanClient.SendPacket(onPacket);
			CPacket::Free(onPacket);

			// ChatServer CPU ����
			CPacket* cpuPacket = CPacket::Alloc();
			_lanClient.mpUpdateDataToMonitorServer(serverNo, MONITOR_DATA_TYPE_CHAT_SERVER_CPU, (int)_performMoniter.GetProcessCpuTotal(), iTime, cpuPacket);
			_lanClient.SendPacket(cpuPacket);
			CPacket::Free(cpuPacket);

			// ChatServer �޸� ��� MByte
			CPacket* memoryPacket = CPacket::Alloc();
			_lanClient.mpUpdateDataToMonitorServer(serverNo, MONITOR_DATA_TYPE_CHAT_SERVER_MEM, (int)_performMoniter.GetProcessUserMemoryByMB(), iTime, memoryPacket);
			_lanClient.SendPacket(memoryPacket);
			CPacket::Free(memoryPacket);

			// ChatServer ���� �� (���ؼ� ��)
			CPacket* sessionPacket = CPacket::Alloc();
			_lanClient.mpUpdateDataToMonitorServer(serverNo, MONITOR_DATA_TYPE_CHAT_SESSION, (int)iSessionCnt, iTime, sessionPacket);
			_lanClient.SendPacket(sessionPacket);
			CPacket::Free(sessionPacket);

			// ChatServer �������� ����� �� (���� ������)
			CPacket* authPacket = CPacket::Alloc();
			_lanClient.mpUpdateDataToMonitorServer(serverNo, MONITOR_DATA_TYPE_CHAT_PLAYER, (int)iLoginPlayerCnt, iTime, authPacket);
			_lanClient.SendPacket(authPacket);
			CPacket::Free(authPacket);

			// ChatServer UPDATE ������ �ʴ� �ʸ� Ƚ��
			CPacket* updataPacket = CPacket::Alloc();
			_lanClient.mpUpdateDataToMonitorServer(serverNo, MONITOR_DATA_TYPE_CHAT_UPDATE_TPS, (int)iJobThreadUpdateCnt, iTime, updataPacket);
			_lanClient.SendPacket(updataPacket);
			CPacket::Free(updataPacket);

			// ChatServer ��ŶǮ ��뷮
			CPacket* poolPacket = CPacket::Alloc();
			_lanClient.mpUpdateDataToMonitorServer(serverNo, MONITOR_DATA_TYPE_CHAT_PACKET_POOL, (int)packetPoolUseCnt, iTime, poolPacket);
			_lanClient.SendPacket(poolPacket);
			CPacket::Free(poolPacket);

			// ChatServer UPDATE MSG Ǯ ��뷮
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
	// ���� ���� ��, �α��� ���� ó���� ���� �۾��� Job Worker Thread�� �ѱ�
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
		// JobQ�� Job�� ���ԵǸ� �̺�Ʈ �߻��Ͽ� ���
		WaitForSingleObject(_jobEvent, INFINITE);

		ChatJob* chatJob = nullptr;

		// Job�� ���� ������ update �ݺ�
		while (_chatJobQ.GetSize() > 0)
		{
			if (_chatJobQ.Dequeue(chatJob))
			{
				// Job Type�� ���� �б� ó��
				switch (chatJob->type)
				{
				case JobType::NEW_CONNECT:
					CreatePlayer(chatJob->sessionID);					// Player ����
					break;

				case JobType::DISCONNECT:
					DeletePlayer(chatJob->sessionID);					// Player ����
					break;

				case JobType::MSG_PACKET:
					Packet_Proc(chatJob->sessionID, chatJob->packet);	// ��Ŷ ó��
					break;

				// ���������� �߻��� �α��� ���� ��Ŷ ó��
				case JobType::LOGIN_RES:
					NetPacketProc_ResLogin(chatJob->sessionID, chatJob->packet);
					break;

				case JobType::TIMEOUT:
					DisconnectSession(chatJob->sessionID);				// Ÿ�� �ƿ�
					break;

				default:
					DisconnectSession(chatJob->sessionID);
					break;
				}

				// ����, ���� Job�� packet�� nullptr�̱� ������ ��ȯ�� Packet�� ����
				if (chatJob->packet != nullptr)
					CPacket::Free(chatJob->packet);

				// JobPool�� Job ��ü ��ȯ
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
		NetPacketProc_Login(sessionID, packet);					// �α��� ��û
		break;

	case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE:
		NetPacketProc_SectorMove(sessionID, packet);			// ���� �̵� ��û
		break;

	case en_PACKET_CS_CHAT_REQ_MESSAGE:
		NetPacketProc_Chatting(sessionID, packet);				// ä�� ������
		break;

	case en_PACKET_CS_CHAT_REQ_HEARTBEAT:
		NetPacketProc_HeartBeat(sessionID, packet);				// ��Ʈ��Ʈ
		break;

	case en_PACKET_ON_TIMEOUT:
		DisconnectSession(sessionID);					// ���� Ÿ�Ӿƿ�
		break;

	default:
		// �߸��� ��Ŷ
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

// ���� ó�� - �� �� Player �����⸦ �̸� ����������
// �α��� ���� ���Ḹ �� ���ǿ� ���� timeout�� �Ǵ��ؾ��ϱ� ������
void ChatServer::OnClientJoin(uint64_t sessionID)
{
	if (!_startFlag)
	{
		ResumeThread(_moniteringThread);
		_startFlag = true;
	}

	ChatJob* job = _jobPool.Alloc();			// jobPool���� job �Ҵ�
	job->type = JobType::NEW_CONNECT;		// �� ���� ����
	job->sessionID = sessionID;				// Player�� �ο��� SessionID
	job->packet = nullptr;					// Player ���� �ÿ��� ��Ŷ �ʿ� ����

	_chatJobQ.Enqueue(job);					// JobQ�� Enqueue

	InterlockedIncrement64(&_jobUpdatecnt);// ����͸��� ����
	SetEvent(_jobEvent);					// Job Worker Thread Event �߻�
}

// ���� ó��
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

// ��Ŷ ó��
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
	Player* player = _playerPool.Alloc();

	if (player == nullptr)
	{
		_chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Player Pool Alloc Failed!");

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

	_mapPlayer.insert({ sessionID, player });		// ��ü Player�� �����ϴ� map�� insert

	InterlockedIncrement64(&_totalPlayerCnt);
	InterlockedIncrement64(&_loginPlayerCnt);

	return true;
}

// player ����
bool ChatServer::DeletePlayer(uint64_t sessionID)
{
	// player �˻�
	Player* player = FindPlayer(sessionID);

	if (player == nullptr)
	{
		_chatLog->logger(dfLOG_LEVEL_DEBUG, __LINE__, L"DeletePlayer # Player Not Found! ID : %016llx", sessionID);
		CRASH();
		return false;
	}

	// ���Ϳ��� �ش� player ��ü ����
	// ���� �̵��� ���� ���� ���¸� ���� x,y ��ǥ�� ��� -1�̰�, �� �� Player�� ���� ���� �������� ����
	// -> x,y ��ǥ �� �� -1�� �ƴϸ� ���Ϳ��� �̵��� �ߴٴ� �ǹ��̹Ƿ� ���Ϳ� �÷��̾� ����
	if (player->sectorX != -1 && player->sectorY != -1)
	{
		_sector[player->sectorY][player->sectorX].erase(player);
	}

	player->recvLastTime = timeGetTime();

	_mapPlayer.erase(player->sessionID);								// ��ü Player ���� map���� �ش� player ����
	_accountNo.erase(player->accountNo);

	InterlockedDecrement64(&_loginPlayerCnt);
	InterlockedIncrement64(&_deletePlayerCnt);

	_playerPool.Free(player);		// PlayerPool�� player ��ȯ

	InterlockedDecrement64(&_totalPlayerCnt);

	return true;
}

//--------------------------------------------------------------------------------------
// Packet Proc
//--------------------------------------------------------------------------------------

// �α��� ��û
void ChatServer::NetPacketProc_Login(uint64_t sessionID, CPacket* packet)
{
	InterlockedIncrement64(&_loginCount);
	InterlockedIncrement64(&_loginTPS);

	// Packet ũ�⿡ ���� ���� ó�� 
	if (packet->GetDataSize() < sizeof(INT64) + ID_MAX_LEN * sizeof(wchar_t) + NICKNAME_MAX_LEN * sizeof(wchar_t) + MSG_MAX_LEN * sizeof(char))
	{
		int size = packet->GetDataSize();

		_chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Login Request Packet > Size Error : %d", size);

		DisconnectSession(sessionID);

		return;
	}

	INT64 _accountNo = 0;
	BYTE status = en_PACKET_CS_LOGIN_RES_LOGIN::dfLOGIN_STATUS_OK;

	// accountNo�� ������ȭ�ؼ� ����
	*packet >> _accountNo;

	// player ã��
	Player* player = FindPlayer(sessionID);
	if (player == nullptr)
	{
		_chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Login # %016llx Player Not Found!", sessionID);
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
	std::string sessionKeyStr(player->sessionKey);

	redisWorkerThread->EnqueueJob(RedisWorkerThread::REDISTYPE::GET, sessionID, _accountNo, sessionKeyStr, nullptr);
}

// ���� �̵� ��û
void ChatServer::NetPacketProc_SectorMove(uint64_t sessionID, CPacket* packet)
{
	// Packet ũ�⿡ ���� ���� ó�� 
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

	// accountNo�� ���� ��ǥ�� ������ȭ�ؼ� ����
	*packet >> accountNo >> sectorX >> sectorY;

	// Player �˻�
	Player* player = FindPlayer(sessionID);
	if (player == nullptr)
	{
		_chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Move Sector Request Packet > Player Not Found");

		DisconnectSession(sessionID);

		return;
	}

	// accountNo Ȯ��
	if (player->accountNo != accountNo)
	{
		_chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Move Sector Request Packet > AccountNo Not Equal");

		DisconnectSession(player->sessionID);

		return;
	}

	// ���� ���� Ȯ��
	if (sectorX >= dfSECTOR_X_MAX || sectorX < 0 || sectorY >= dfSECTOR_Y_MAX || sectorY < 0)
	{
		_chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Move Sector Request Packet > Sector Bound Error");

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
			auto iter = _sector[player->sectorY][player->sectorX].find(player);

			// ���� �̵� ���� ��ü ����
			if (iter != _sector[player->sectorY][player->sectorX].end())
			{
				_sector[player->sectorY][player->sectorX].erase(iter);
			}

			// ���� ���� ��ǥ�� ����
			player->sectorX = sectorX;
			player->sectorY = sectorY;

			// ���� ��ġ�� �߰�
			_sector[player->sectorY][player->sectorX].emplace(player);
		}
	}
	// ó�� ��ǥ �̵� ��, �ش� ��ǥ�� ��ü �߰�
	else
	{
		// ���� ���� ��ǥ�� ����
		player->sectorX = sectorX;
		player->sectorY = sectorY;

		// ���� ��ġ�� �߰�
		_sector[player->sectorY][player->sectorX].emplace(player);
	}

	InterlockedIncrement64(&_sectorMovePacketTPS);

	CPacket* resPacket = CPacket::Alloc();

	MPResSectorMove(resPacket, player->accountNo, player->sectorX, player->sectorY);
	
	// ���� �̵� ���� ��Ŷ ����
	SendPacket(sessionID, resPacket);

	CPacket::Free(resPacket);
}

// ä�� ������
void ChatServer::NetPacketProc_Chatting(uint64_t sessionID, CPacket* packet)
{
	// Packet ũ�⿡ ���� ���� ó�� 
	if (packet->GetDataSize() < sizeof(INT64) + sizeof(WORD) + sizeof(wchar_t))
	{
		int size = packet->GetDataSize();
		_chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Chatting Request Packet > Size Error : %d", size);
		DisconnectSession(sessionID);

		return;
	}

	// Player �˻�
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

	// accountNo�� ������ȭ�ؼ� ����
	*packet >> accountNo;

	// accountNo�� �ٸ� ���
	if (accountNo != player->accountNo)
	{
		_chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Chatting Request Packet > account Error\tplayer: %IId\tpacket : %IId",
			player->accountNo, accountNo);

		DisconnectSession(sessionID);

		return;
	}

	// ä�� �޽��� ���̸� ������ȭ�ؼ� ����
	*packet >> msgLen;

	// ��� ���̷ε� ũ��� ���� ���̷ε� ũ�Ⱑ �ٸ� ���
	if (packet->GetDataSize() != msgLen)
	{
		_chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Chatting Request Packet > Payload Size Error : %d", msgLen);

		DisconnectSession(sessionID);

		return;
	}

	player->recvLastTime = timeGetTime();

	// ���� ���� Ȯ��
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

	// player�� �����ϴ� ������ �ֺ� 9�� ���� ���ϱ�
	st_SECTOR_AROUND sectorAround;
	GetSectorAround(player->sectorX, player->sectorY, sectorAround);

	// �ֺ� ���Ϳ� �����ϴ� Player�鿡�� ä�� ���� ��Ŷ ����
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

// ��Ʈ��Ʈ - ����� �ƹ��� ����� ���� ����
void ChatServer::NetPacketProc_HeartBeat(uint64_t sessionID, CPacket* packet)
{
	// ���� ó�� -> ��Ʈ��Ʈ ��Ŷ�� Ÿ�� �ܿ� �߰����� �����Ͱ� ������ �ȵ�
	if (packet->GetDataSize() > 0)
	{
		_chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"HeartBeat Request Packet > Packet is not empty");

		DisconnectSession(sessionID);

		return;
	}

	// Player �˻�
	Player* player = FindPlayer(sessionID);

	// Player�� ���µ� ��Ŷ�� ���´�???? -> error
	if (player == nullptr)
	{
		_chatLog->logger(dfLOG_LEVEL_ERROR, __LINE__, L"HeartBeat Request Packet > Player Not Found");

		return;
	}

	player->recvLastTime = timeGetTime();
}

// �񵿱� redis ��û ����� ���� ��, ���� �α��� job ó��
void ChatServer::NetPacketProc_ResLogin(uint64_t sessionID, CPacket* packet)
{
	InterlockedIncrement64(&_loginResJobUpdateTPS);

	// �α��� ���� ��Ŷ ����
	SendPacket(sessionID, packet);
}