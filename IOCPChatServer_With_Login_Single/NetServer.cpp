#include "PCH.h"

#include "NetServer.h"

// ========================================================================
// Thread Call
// ========================================================================
// Accept Thread Call
unsigned __stdcall AcceptThread(void* param)
{
	NetServer* lanServ = (NetServer*)param;

	lanServ->AcceptThread_Serv();

	return 0;
}

// Worker Thread Call
unsigned __stdcall WorkerThread(void* param)
{
	NetServer* lanServ = (NetServer*)param;

	lanServ->WorkerThread_Serv();

	return 0;
}

// Timeout Thread Call
unsigned __stdcall TimeoutThread(void* param)
{
	NetServer* netServ = (NetServer*)param;

	netServ->TimeoutThread_Serv();

	return 0;
}

NetServer::NetServer() : _listenSocket(INVALID_SOCKET), _serverPort(0), _iocpHandle(INVALID_HANDLE_VALUE), _acceptThread(INVALID_HANDLE_VALUE), _workerThreads{ INVALID_HANDLE_VALUE },
_maxAcceptCount(0), _sessionArray{ nullptr }, _acceptCount(0), _acceptTPS(0), _sessionCnt(0),
_releaseCount(0), _releaseTPS(0), _recvMsgTPS(0), _sendMsgTPS(0), _recvCallTPS(0), _sendCallTPS(0),
_recvBytesTPS(0), _sendBytesTPS(0), _workerThreadCount(0), _runningThreadCount(0), _startMonitering(false) {
	// ========================================================================
	// Initialize
	// ========================================================================
	_logger = new Log(L"NetServer");

	wprintf(L"NetServer Initializing...\n");

	WSADATA  wsaData;

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		int initError = WSAGetLastError();

		return;
	}
}

NetServer::~NetServer()
{
	Stop();
}

bool NetServer::Start(const wchar_t* IP, unsigned short PORT, int createWorkerThreadCnt, int runningWorkerThreadCnt, bool nagelOff, bool zeroCopyOff, int maxAcceptCnt, unsigned char packet_code, unsigned char packet_key, DWORD timeout)
{
	// ������ ���� �ð�
	// ���ǵ��� ������ ���� �ð��� �������� Ÿ�Ӿƿ� �ƴ��� �Ǻ��ϱ� ���� �ʿ�
	_serverTime = timeGetTime();
	_timeout = timeout;

	CPacket::SetCode(packet_code);	// Packet Code
	CPacket::SetKey(packet_key);	// CheckSum ������ ���� Key

	_maxAcceptCount = maxAcceptCnt;

	// �ִ� ���� ������ ����ŭ �̸� ���� �迭 ����
	_sessionArray = new stSESSION[_maxAcceptCount];

	//  ��� ������ ���� �迭�� index�� �����ϱ� ���� LockFreeStack ���
	// ��� ������ index�� ������������ ������ ���� max index���� push
	for (int i = _maxAcceptCount - 1; i >= 0; i--)
	{
		// ��� ������ �ε��� push
		_availableIndexStack.Push(i);
	}

	// Create Listen Socket
	_listenSocket = socket(AF_INET, SOCK_STREAM, 0);

	if (INVALID_SOCKET == _listenSocket)
	{
		int sockError = WSAGetLastError();

		return false;
	}

	SOCKADDR_IN serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(PORT);
	InetPtonW(AF_INET, IP, &serverAddr.sin_addr);

	// bind
	if (bind(_listenSocket, (SOCKADDR*)&serverAddr, sizeof(SOCKADDR_IN)) == SOCKET_ERROR)
	{
		int bindError = WSAGetLastError();

		return false;
	}

	if (!zeroCopyOff)
	{
		// TCP Send Buffer Remove - zero copy
		int sendVal = 0;
		if (setsockopt(_listenSocket, SOL_SOCKET, SO_SNDBUF, (const char*)&sendVal, sizeof(sendVal)) == SOCKET_ERROR)
		{
			int setsockoptError = WSAGetLastError();
			_logger->logger(dfLOG_LEVEL_ERROR, __LINE__, L"setsockopt() Error : %d", setsockoptError);

			return false;
		}
	}

	if (nagelOff)
	{
		// Nagle off
		if (setsockopt(_listenSocket, IPPROTO_TCP, TCP_NODELAY, (const char*)&nagelOff, sizeof(nagelOff)) == SOCKET_ERROR)
		{
			int setsockoptError = WSAGetLastError();

			return false;
		}
	}

	// TIME_WAIT off
	struct linger ling;
	ling.l_onoff = 1;
	ling.l_linger = 0;
	if (setsockopt(_listenSocket, SOL_SOCKET, SO_LINGER, (const char*)&ling, sizeof(ling)) == SOCKET_ERROR)
	{
		int setsockoptError = WSAGetLastError();

		return false;
	}

	// listen
	if (listen(_listenSocket, SOMAXCONN) == SOCKET_ERROR)
	{
		int listenError = WSAGetLastError();

		return false;
	}

	SYSTEM_INFO si;
	GetSystemInfo(&si);

	// CPU Core Counting
	// Worker Thread ������ 0 ���϶��, �ھ� ���� * 2 �� �缳��
	if (createWorkerThreadCnt <= 0)
		_workerThreadCount = si.dwNumberOfProcessors * 2;
	else
		_workerThreadCount = createWorkerThreadCnt;

	// Running Thread�� CPU Core ������ �ʰ��Ѵٸ� CPU Core ������ �缳��
	if (runningWorkerThreadCnt > si.dwNumberOfProcessors)
		_runningThreadCount = si.dwNumberOfProcessors;
	else
		_runningThreadCount = runningWorkerThreadCnt;

	// Create I/O Completion Port
	_iocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, _runningThreadCount);
	if (_iocpHandle == NULL)
	{
		int iocpError = WSAGetLastError();
		
		return false;
	}

	// ========================================================================
	// Create Thread
	// ========================================================================

	// Accept Thread
	_acceptThread = (HANDLE)_beginthreadex(NULL, 0, AcceptThread, this, 0, NULL);
	if (AcceptThread == NULL)
	{
		int threadError = GetLastError();

		return false;
	}

	// Worker Thread
	_workerThreads.resize(_workerThreadCount);
	for (int i = 0; i < _workerThreads.size(); i++)
	{
		_workerThreads[i] = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, this, 0, NULL);
		if (_workerThreads[i] == NULL)
		{
			int threadError = GetLastError();

			return false;
		}
	}

	// Timeout Thread
	_timeoutThread = (HANDLE)_beginthreadex(NULL, 0, TimeoutThread, this, 0, NULL);
	if (AcceptThread == NULL)
	{
		int threadError = GetLastError();
		_logger->logger(dfLOG_LEVEL_ERROR, __LINE__, L"_beginthreadex() Error : %d", threadError);

		return false;
	}

	return true;
}

// Accept Thread
bool NetServer::AcceptThread_Serv()
{
	DWORD threadID = GetCurrentThreadId();

	wprintf(L"AcceptThread[%d] Start...\n", threadID);

	SOCKADDR_IN clientAddr;
	int addrLen = sizeof(SOCKADDR_IN);
	SOCKET clientSocket;
	DWORD recvBytes;

	// ������ Ŭ�� ���� ���
	wchar_t szClientIP[16] = { 0 };

	unsigned long long s_sessionUniqueID = 0;				// Unique Value

	while (true)
	{
		// accept
		clientSocket = accept(_listenSocket, (SOCKADDR*)&clientAddr, &addrLen);

		if (clientSocket == INVALID_SOCKET)
		{
			int acceptError = GetLastError();

			break;
		}


		InetNtopW(AF_INET, &clientAddr.sin_addr, szClientIP, 16);

		// Black IP���� Ȯ�� - ����� �̱���
		if (!OnConnectionRequest(szClientIP, ntohs(clientAddr.sin_port)))
		{
			// ���� �ź�
			// ...
			closesocket(clientSocket);
			break;
		}

		// ��� ������ index ����
		int index;

		// ����ִ� �迭 ã��
		// index stack�� ��������� �迭�� �� ����� (full!)
		if (!_availableIndexStack.Pop(&index))
		{
			_logger->logger(dfLOG_LEVEL_ERROR, __LINE__, L"accept # index full");

			// ��� accept �ߴ� ���� ����
			closesocket(clientSocket);

			continue;
		}
		
		// accept ��
		InterlockedIncrement64(&_acceptTPS);
		InterlockedIncrement64(&_acceptCount);

		// ���� �迭���� �ش� index�� ���� ������
		stSESSION* session = &_sessionArray[index];

		// ������ sessionID ����
		uint64_t sessionID = CreateSessionID(++s_sessionUniqueID, index);
	
		session->Init(sessionID, clientSocket, clientAddr);
		
		// IOCP�� ���� ����
		// ���� �ּҰ��� Ű ��
		if (CreateIoCompletionPort((HANDLE)clientSocket, _iocpHandle, (ULONG_PTR)session, 0) == NULL)
		{
			int ciocpError = WSAGetLastError();

			// accept�� �ٷ� ��ȯ�ϴ� ���̹Ƿ� iocount ���� �� io �Ϸ� ���� Ȯ��
			if (0 == InterlockedDecrement64((LONG64*)&session->_ioRefCount))
			{
				ReleaseSession(session);
				continue;
			}
		}

		// ���� ó��
		// ������ backup �س��� sessionID�� �Ű������� ���� -> ������ ��������� ���� ������ sessionID�� �����ϰ� �Ǹ�
		// �� ���̿� ������ ���Ҵ�Ǿ� �ٸ� ������ ���� ��, �ش� sessionID�� ���� player�� ������ �� ����
		// ex) ���� Ŭ���̾�Ʈ ���� ��, ������ �� �����ƴµ� player�� �����ִ� ���� �߻�
		OnClientJoin(sessionID);

		InterlockedIncrement64(&_sessionCnt);

		// �񵿱� recv I/O ��û
		RecvPost(session);

		// accept���� �ø� ����ī��Ʈ ����
		// ����ī��Ʈ�� 0�̶�� ���� ����
		if (0 == InterlockedDecrement64(&session->_ioRefCount))
		{
			ReleaseSession(session);
		}
	}

	return true;
}

// Worker Thread
bool NetServer::WorkerThread_Serv()
{
	DWORD threadID = GetCurrentThreadId();

	stSESSION* pSession = nullptr;
	BOOL bSuccess = true;
	long cbTransferred = 0;
	LPOVERLAPPED pOverlapped = nullptr;
	
	bool completionOK;

	while (true)
	{
		// �ʱ�ȭ
		cbTransferred = 0;
		pSession = nullptr;
		pOverlapped = nullptr;
		completionOK = false;

		// GQCS call
		// client�� send���� �����ʰ� �ٷ� disconnect �� ��� -> WorkerThread���� recv 0�� ���� GQCS�� ���
		bSuccess = GetQueuedCompletionStatus(_iocpHandle, (LPDWORD)&cbTransferred, (PULONG_PTR)&pSession,
			&pOverlapped, INFINITE);

		// IOCP Error or TIMEOUT or PQCS�� ���� NULL ����
		if (pOverlapped == NULL)
		{
			int iocpError = WSAGetLastError();

			// ��� IOCP Worker Thread�� �����Ű�� ���� PQCS�� ȣ����
			PostQueuedCompletionStatus(_iocpHandle, 0, 0, 0);
			_logger->logger(dfLOG_LEVEL_ERROR, __LINE__, L"worker # iocp error %d", iocpError);

			break;	
		}

		// SendPost �۾��� PQCS�� ������ ��� 
		if (cbTransferred == 0 && (unsigned char)pOverlapped == PQCSTYPE::SENDPOST)
		{
			// WSASend 1ȸ ������ �ϱ� ���� sendFlag�� �ٽ� �۽� ���� ���·� �����Ŵ
			InterlockedExchange8((char*)&pSession->_sendFlag, false);

			// �ٸ� IOCP worker thread�� ���ؼ� dequeue�Ǿ� sendQ�� �������� SendPost �Լ��� ������ �ʿ䰡 ����
			if (pSession->_sendQ.GetSize() > 0)
				SendPost(pSession);
		}
		// Release �۾��� PQCS�� ������ ��� 
		else if (cbTransferred == 0 && (unsigned char)pOverlapped == PQCSTYPE::RELEASE)
		{
			// ioRefCount�� 0�� ��쿡 PQCS�� ȣ���� ���̱� ������ �� ������ ioRefCount�� 0��
			// -> continue�� �Ѱܾ� �ϴܿ� ioRefCount�� ���ҽ�Ű�� ���� skip ����
			ReleaseSession(pSession);
			continue;
		}
		// Recv Packet Handler
		else if (pOverlapped == &pSession->_stRecvOverlapped && cbTransferred > 0)
		{
			completionOK = RecvProc(pSession, cbTransferred);
		}
		// Send Packet Handler
		else if (pOverlapped == &pSession->_stSendOverlapped && cbTransferred > 0)
		{
			completionOK = SendProc(pSession, cbTransferred);
		}

		// I/O �Ϸ� ������ ���̻� ���ٸ� ���� ���� �۾�
		if (0 == InterlockedDecrement64(&pSession->_ioRefCount))
		{
			ReleaseSession(pSession);
		}

	}

	return true;
}

// Ÿ�Ӿƿ� ������ ���� ������
bool NetServer::TimeoutThread_Serv()
{
	// 2�ʸ��� Ÿ�Ӿƿ� ���� (2�� ������ Ÿ�Ӿƿ� ������ ���)
	while (1)
	{
		// ������ ���� �ð�
		// ���ǵ��� ������ ���� �ð��� �������� Ÿ�Ӿƿ� �ƴ��� �Ǻ��ϱ� ���� �ʿ�
		_serverTime = timeGetTime();

		for (int i = 0; i < _maxAcceptCount; i++)
		{
			// �� ���̿� �ٸ� ������ ���Ҵ�Ǿ� �ٸ� ������ �� ���� ������ �̸� sessionID ����
			uint64_t sessionID = _sessionArray[i]._sessionID;

			// �̹� release �� ���� skip
			if ((InterlockedOr64((LONG64*)&_sessionArray[i]._ioRefCount, 0) & RELEASEMASKING) != 0) continue;

			// ���� ���� �Ҵ��� �� �� ���� skip
			if (_sessionArray[i]._socketClient == INVALID_SOCKET) continue;

			// �̹� disconnect �� ���� skip
			if (_sessionArray[i]._isDisconnected == true) continue;

			// ���Ҵ�Ǿ� �ٸ� ������ �� ���� skip
			if (sessionID != _sessionArray[i]._sessionID) continue;

			// ���ǿ� �ο��� Ÿ�Ӿƿ� ���� �ð�(Timer)�� ���� ���� �ð����� �̷��� ���� (Ÿ�Ӿƿ����� ��������) skip
			if (InterlockedOr((LONG*)&_sessionArray[i]._timer, 0) >= _serverTime) continue;

			// ----------------------------------------------------------------------
			// �̰��� ������ ���ǵ��� Ÿ�Ӿƿ� ������� ��
			OnTimeout(sessionID);		// Contents �������� Ÿ�Ӿƿ� ���� logic ó��

			// Ÿ�Ӿƿ� ���� ���� ���̶�� flag �ٽ� �ǵ�����
			if (_sessionArray[i]._sendDisconnFlag == true)
				InterlockedExchange8((char*)&_sessionArray[i]._sendDisconnFlag, false);
		}

		Sleep(2000);
	}

	return true;
}

bool NetServer::RecvProc(stSESSION* pSession, long cbTransferred)
{
	// ������ payload ũ�⸸ŭ recv �������� write ������ �̵�
	pSession->_recvRingBuffer.MoveWritePtr(cbTransferred);

	// ���� recv �����ۿ��� ��� ���� ũ��
	int useSize = pSession->_recvRingBuffer.GetUseSize();

	// ������ ���� ������ �Ǿ��ִ� ������ ���, �� ���̿� ��Ŷ�� �͵� Ÿ�Ӿƿ� �ֱ� ������Ʈ�� �ϸ� �ȵ�
	// -> ���� ���� ���� ���ŵ� ���ǵ��� �ֱ⸦ ��� ������Ʈ�ϸ�
	// ������ ����� �Ǵ� ������ ��� Ÿ�Ӿƿ� �ֱⰡ ���ŵǾ� ������ ����
	if (!pSession->_sendDisconnFlag)
		SetTimeout(pSession);

	// Recv Message Process
	while (useSize > 0)
	{
		NetHeader header;

		// Header ũ�⸸ŭ �ִ��� Ȯ��
		if (useSize <= sizeof(NetHeader))
			break;

		// Header Peek
		pSession->_recvRingBuffer.Peek((char*)&header, sizeof(NetHeader));

		// Packet ũ�⸸ŭ �ִ��� Ȯ��
		if (useSize < sizeof(NetHeader) + header.len)
			break;

		// packet code Ȯ��
		if (header.code != CPacket::GetCode())
		{
			DisconnectSession(pSession->_sessionID);

			return false;
		}

		// Len Ȯ�� (�����ų� ���� �� �ִ� ��Ŷ ũ�⺸�� Ŭ �� -> ���� ���)
		if (header.len < 0 || header.len > MAX_PACKET_LEN)
		{
			DisconnectSession(pSession->_sessionID);

			return false;
		}

		// PacketPool���� packet �Ҵ�
		CPacket* packet = CPacket::Alloc();

		// packet ũ�⸸ŭ ������ Dequeue (�������)
		pSession->_recvRingBuffer.Dequeue(packet->GetBufferPtr(), header.len + CPacket::en_PACKET::DEFAULT_HEADER_SIZE);

		// packet ũ�⸸ŭ packet write pos �̵�
		packet->MoveWritePos(header.len);

		// ���ڵ��Ͽ� ���� ������ ��
		if (!packet->Decoding())
		{
			CPacket::Free(packet);

			DisconnectSession(pSession->_sessionID);

			return false;
		}

		// Recv Message TPS
		InterlockedIncrement64((LONG64*)&_recvMsgTPS);

		// Recv Bytes TPS
		InterlockedAdd64((LONG64*)&_recvBytesTPS, header.len);

		// ������ �� recv ó��
		OnRecv(pSession->_sessionID, packet);

		useSize = pSession->_recvRingBuffer.GetUseSize();
	}

	// Recv ����
	RecvPost(pSession);


	return true;
}

bool NetServer::SendProc(stSESSION* pSession, long cbTransferred)
{
	// sendPost���� ������ 0�� ��츦 �ɷ��´µ��� �� ������ �߻��ϴ� ���� error
	if (pSession->_sendPacketCount == 0)
		return false;

	int totalSendBytes = 0;
	int iSendCount;

	// send �Ϸ� ������ ��Ŷ�� PacketPool�� ��ȯ
	for (iSendCount = 0; iSendCount < pSession->_sendPacketCount; iSendCount++)
	{
		totalSendBytes += pSession->_sendPackets[iSendCount]->GetDataSize();
		CPacket::Free(pSession->_sendPackets[iSendCount]);
	}

	// Send Bytes TPS
	InterlockedAdd64((long long*)&_sendBytesTPS, totalSendBytes);

	// Send Message TPS
	InterlockedAdd64((long long*)&_sendMsgTPS, pSession->_sendPacketCount);

	pSession->_sendPacketCount = 0;

	// ���� �� flag�� �ٽ� ������ ���·� �ǵ�����
	InterlockedExchange8((char*)&pSession->_sendFlag, false);

	// 1ȸ send ��, sendQ�� �׿��ִ� ������ ������ ��� send
	if (pSession->_sendQ.GetSize() > 0)
	{
		// sendFlag�� false�ΰ� �ѹ� Ȯ���� ������ ���Ͷ� �� (������� �� ���̿� true�� ��찡 �ɷ����� ���Ͷ� call ����)
		if (pSession->_sendFlag == false)
		{
			// ������ ���¿��� ���� ���·� ����
			if (false == InterlockedExchange8((char*)&pSession->_sendFlag, true))
			{
				// SendPost �۾��� �ϱ� ������ �ش� Session�� ��� �־�� �ϹǷ� ���� ī��Ʈ ����
				InterlockedIncrement64(&pSession->_ioRefCount);
				PostQueuedCompletionStatus(_iocpHandle, 0, (ULONG_PTR)pSession, (LPOVERLAPPED)PQCSTYPE::SENDPOST);
			}
		}
	}

	return true;
}

bool NetServer::RecvPost(stSESSION* pSession)
{
	// recv �ɱ� ���� ������ ���� �ܺο��� disconnect �Լ��� ȣ��� �� ����
	// -> recv �� �ɷ��� ���� io �۾��� ����ص� �ǹ̰� �����ϱ� ������ recvpost ������ ���ƹ���
	if (pSession->_isDisconnected)
		return false;

	// ������ ��Ͽ� �ʿ��� ����
	WSABUF wsa[2] = { 0 };
	int wsaCnt = 1;
	DWORD flags = 0;

	int freeSize = pSession->_recvRingBuffer.GetFreeSize();					// ������  ���� ������
	int directEequeueSize = pSession->_recvRingBuffer.DirectEnqueueSize();	// ������ ���ο��� �ѹ��� ��ť ������ ����

	if (freeSize == 0)
		return false;

	// �����ۿ��� �߰��� �߸� ���� �ѹ��� ��ť ������ ������ WSABUF�� ����
	wsa[0].buf = pSession->_recvRingBuffer.GetWriteBufferPtr();
	wsa[0].len = directEequeueSize;

	// ������ ���ο��� �� ������ �� �������� ���� ��� �ι�° �� ������ WSABUF�� �ι�° �迭�� ����
	if (freeSize > directEequeueSize)
	{
		wsa[1].buf = pSession->_recvRingBuffer.GetBufferPtr();
		wsa[1].len = freeSize - directEequeueSize;
		++wsaCnt;
	}

	// recv overlapped I/O ����ü reset
	ZeroMemory(&pSession->_stRecvOverlapped, sizeof(OVERLAPPED));

	// recv
	// ioCount : WSARecv �Ϸ� ������ WSARecv�Լ��� return���� ���� ������ �� �����Ƿ� WSARecv ȣ�� ���� �������Ѿ� ��
	InterlockedIncrement64(&pSession->_ioRefCount);
	int recvRet = WSARecv(pSession->_socketClient, wsa, wsaCnt, NULL, &flags, &pSession->_stRecvOverlapped, NULL);
	InterlockedIncrement64(&_recvCallTPS);

	// ����ó��
	if (recvRet == SOCKET_ERROR)
	{
		int recvError = WSAGetLastError();

		if (recvError != WSA_IO_PENDING)
		{
			if (recvError != ERROR_10054 && recvError != ERROR_10058 && recvError != ERROR_10060)
			{	
				// ����				
				OnError(recvError, L"RecvPost # WSARecv Error\n");
			}

			// Pending�� �ƴ� ���, �Ϸ� ���� ����
			if (0 == InterlockedDecrement64(&pSession->_ioRefCount))
			{
				ReleaseSession(pSession);
			}
			return false;
		}
		// Pending�� ���
		else
		{			
			// Pending �ɷȴµ�, �� ������ disconnect�Ǹ� �� �� �����ִ� �񵿱� io �����������
			if (pSession->_isDisconnected)
			{
				CancelIoEx((HANDLE)pSession->_socketClient, &pSession->_stRecvOverlapped);
			}

		}
	}

	return true;
}

bool NetServer::SendPost(stSESSION* pSession)
{
	// 1ȸ �۽� ������ ���� flag Ȯ�� (send �Լ� ȣ���ϴ� �۾� ��ü�� ������ ������ call Ƚ���� ���̱� ���� ���)
	// false �� �̼۽� �����̹Ƿ� send �۾� ����
	// true �� �۽� ���̹Ƿ� �Ϸ� ������ ���� ������ send �۾��� �ϸ� �ȵ�
	if ((pSession->_sendFlag == true) || true == InterlockedExchange8((char*)&pSession->_sendFlag, true))
		return false;

	// �ٸ� �����忡�� Dequeue �������� ��� SendQ�� ������ �� ����
	if (pSession->_sendQ.GetSize() <= 0)
	{
		// * ������ �Ͼ �� �ִ� ��Ȳ
		// �ٸ� �����忡�� dequeue�� ���� �ؼ� size�� 0�� �Ǿ� �� ���ǹ��� �����Ѱǵ�
		// �� ��ġ���� �Ǵٸ� �����忡�� ��Ŷ�� enqueue�ǰ� sendpost�� �Ͼ�� �Ǹ�
		// ���� sendFlag�� false�� ������� ���� �����̱� ������ sendpost �Լ� ��� ���ǿ� �ɷ� ���������� ��
		// �� ��, �� ������� �ٽ� ���ƿ��� �� ���, 
		// sendQ�� ��Ŷ�� �ִ� �����̹Ƿ� sendFlas�� false�� �ٲ��ֱ⸸ �ϰ� �����ϴ°� �ƴ϶�
		// �ѹ� �� sendQ�� size Ȯ�� �� sendpost PQCS ���� �� �����ؾ� ��
		InterlockedExchange8((char*)&pSession->_sendFlag, false);

		// �� ���̿� SendQ�� Enqueue �ƴٸ� �ٽ� SendPost Call 
		if (pSession->_sendQ.GetSize() > 0)
		{
			// sendpost �Լ� ������ send call�� 1ȸ ������
			// sendpost �Լ��� ȣ���ϱ� ���� PQCS�� 1ȸ ������ �־� ���� ������
			// -> �׷��� ���� ���, sendpacket ���´�� ��� PQCS ȣ���ϰ� �Ǿ� ������ ���� ���� �� ����  
			if (pSession->_sendFlag == false)
			{
				if (false == InterlockedExchange8((char*)&pSession->_sendFlag, true))
				{
					InterlockedIncrement64(&pSession->_ioRefCount);
					PostQueuedCompletionStatus(_iocpHandle, 0, (ULONG_PTR)pSession, (LPOVERLAPPED)PQCSTYPE::SENDPOST);
				}
			}
		}
		return false;
	}

	// ������ ����� ���� ����
	WSABUF wsa[MAX_WSA_BUF] = { 0 };
	int deqIdx = 0;

	// sendQ�� ���� ��Ŷ���� Dequeue�Ͽ� �۽ſ� ��Ŷ �迭�� ����
	while (pSession->_sendQ.Dequeue(pSession->_sendPackets[deqIdx]))
	{
		// ���� ��Ŷ�� WSABUF�� ����
		wsa[deqIdx].buf = pSession->_sendPackets[deqIdx]->GetNetBufferPtr();
		wsa[deqIdx].len = pSession->_sendPackets[deqIdx]->GetNetDataSize();

		deqIdx++;

		// WSABUF ���� �ִ� ������ŭ ����
		if (deqIdx >= MAX_WSA_BUF)
			break;
	}

	pSession->_sendPacketCount = deqIdx;		// �Ϸ� ���� ��, PacketPool�� ��ȯ�� ��Ŷ ����

	// send overlapped I/O ����ü reset
	ZeroMemory(&pSession->_stSendOverlapped, sizeof(OVERLAPPED));

	// send
	// ioCount : WSASend �Ϸ� ������ ���Ϻ��� ���� ������ �� �����Ƿ� WSASend ȣ�� ���� �������Ѿ� ��
	InterlockedIncrement64(&pSession->_ioRefCount);
	int sendRet = WSASend(pSession->_socketClient, wsa, deqIdx, NULL, 0, &pSession->_stSendOverlapped, NULL);
	InterlockedIncrement64(&_sendCallTPS);

	// ����ó��
	if (sendRet == SOCKET_ERROR)
	{
		int sendError = WSAGetLastError();

		// default error�� ����
		if (sendError != WSA_IO_PENDING)
		{
			if (sendError != ERROR_10054 && sendError != ERROR_10058 && sendError != ERROR_10060)
			{
				OnError(sendError, L"SendPost # WSASend Error\n");
			}

			// Pending�� �ƴ� ���, �Ϸ� ���� ���� -> IOCount�� ����
			if (0 == InterlockedDecrement64(&pSession->_ioRefCount))
			{
				ReleaseSession(pSession);
			}

			return false;
		}
	}

	return true;
}

bool NetServer::SendPacket(uint64_t sessionID, CPacket* packet)
{
	// sessionID���� index ã�� ��, �ش� Session ����
	int index = GetSessionIndex(sessionID);
	if (index < 0 || index >= _maxAcceptCount)
	{
		return false;
	}

	stSESSION* pSession = &_sessionArray[index];

	if (pSession == nullptr)
	{
		return false;
	}

	// ���� ��� ����ī��Ʈ ������ ���� ������ Release ������ ���� Ȯ�� (���Ͷ� �������� ������ ���� ����)
	// Release ��Ʈ���� 1(ioRefCount���� ���� 31bit ��ġ�� Release flagó�� ���)�̸� Release �۾� ���� ���̶�� �ǹ�
	// ��ó�� �ٽ� release���� ������ �����̹Ƿ� ioRefCount ���� ���ص� ��
	if (InterlockedIncrement64(&pSession->_ioRefCount) & RELEASEMASKING)
	{
		return false;
	}

	// ------------------------------------------------------------------------------------
	// �̶����ʹ� Release �Լ� ���Ե� ���ϰ� ������ SendPacket ������ ������ �� ����

	// �ش� ������ �´��� �ٽ� Ȯ�� (�ٸ� ������ ���� ���� & �Ҵ�Ǿ�, �ٸ� ������ ���� ���� ����)
	// �ش� ������ �ƴ� ���, ������ �����ߴ� ioCount�� �ǵ������� �� (�ǵ����� ������ ���Ҵ� ������ io�� 0�� �� ���� ����)
	if (sessionID != pSession->_sessionID)
	{
		// �ܺ� ������ ������ ���� ������ ���� Release �Լ� ó���� PQCS �Լ��� ȣ���Ͽ� �񵿱������� ����
		if (0 == InterlockedDecrement64(&pSession->_ioRefCount))
			ReleasePQCS(pSession);

		return false;
	}

	// �ܺ� ���������� disconnect �ϴ� ����
	if (pSession->_isDisconnected)
	{
		if (0 == InterlockedDecrement64(&pSession->_ioRefCount))
			ReleasePQCS(pSession);

		return false;
	}

	// ��� ���� & ���ڵ�
	packet->Encoding();

	// Enqueue�� ��Ŷ�� Dequeue�ϱ� ������ �����Ǹ� �ȵǱ� ������ ��Ŷ ����ī��Ʈ ���� -> Dequeue�� �� ����
	packet->addRefCnt();

	// packet �����͸� SendQ�� enqueue
	pSession->_sendQ.Enqueue(packet);

	// sendpost �Լ� ������ send call�� 1ȸ ������
	// sendpost �Լ��� ȣ���ϱ� ���� PQCS�� 1ȸ ������ �־� ���� ������
	// -> �׷��� ���� ���, sendpacket ���´�� ��� PQCS ȣ���ϰ� �Ǿ� ������ �����Ѵ�� �ȳ��� �� ���� 
	if (pSession->_sendFlag == false)
	{
		if (false == InterlockedExchange8((char*)&pSession->_sendFlag, true))
		{
			InterlockedIncrement64(&pSession->_ioRefCount);
			PostQueuedCompletionStatus(_iocpHandle, 0, (ULONG_PTR)pSession, (LPOVERLAPPED)PQCSTYPE::SENDPOST);
		}
	}

	// sendPacket �Լ����� ������Ų ���� ���� ī��Ʈ ����
	if (0 == InterlockedDecrement64(&pSession->_ioRefCount))
	{
		ReleasePQCS(pSession);
		return false;
	}
}

void NetServer::ReleaseSession(stSESSION* pSession)
{
	// �ٸ� ������ �ش� ������ ���(I/O �۾� or sendpacket or disconnect)�ϴ��� Ȯ��
	// ����ī��Ʈ ���� 31bit ��ġ�� release flag�� 1�� ��
	if (InterlockedCompareExchange64(&pSession->_ioRefCount, RELEASEMASKING, 0) != 0)
		return;

	//-----------------------------------------------------------------------------------
	// Release ���� ���Ժ�
	//-----------------------------------------------------------------------------------
	// ioCount = 0, releaseFlag = 1 �� ����

	uint64_t sessionID = pSession->_sessionID;
	SOCKET sock = pSession->_socketClient;

	// ���� ����
	pSession->Release();

	// recv�� ���̻� ������ �ȵǹǷ� ���� close
	closesocket(sock);

	// index�� stack�� push (�̻�� index�� ����)
	_availableIndexStack.Push(GetSessionIndex(sessionID));

	// ����� ���� ���ҽ� ���� (ȣ�� �Ŀ� �ش� ������ ���Ǹ� �ȵ�)
	OnClientLeave(sessionID);

	// �������� client �� ����
	InterlockedDecrement64(&_sessionCnt);

	// ���� ������ clinet �� ����
	InterlockedIncrement64(&_releaseCount);
	InterlockedIncrement64(&_releaseTPS);
}

bool NetServer::DisconnectSession(uint64_t sessionID)
{
	// sessionID���� index ã�� ��, �ش� Session ����
	int index = GetSessionIndex(sessionID);
	if (index < 0 || index >= _maxAcceptCount) return false;

	stSESSION* pSession = &_sessionArray[index];

	if (pSession == nullptr) return false;

	// ���� ��� ����ī��Ʈ ������ ���� ������ Release ������ ���� Ȯ�� (���Ͷ� �������� ������ ���� ����)
	// Release ��Ʈ���� 1(ioRefCount���� ���� 31bit ��ġ�� Release flagó�� ���)�̸� Release �۾� ���� ���̶�� �ǹ�
	// ��ó�� �ٽ� release���� ������ �����̹Ƿ� ioRefCount ���� ���ص� ��
	if (InterlockedIncrement64(&pSession->_ioRefCount) & RELEASEMASKING)
		return false;

	// �ش� ������ �´��� �ٽ� Ȯ�� (�ٸ� ������ ���� ���� & �Ҵ�Ǿ�, �ٸ� ������ ���� ���� ����)
	// -> �̹� �ܺο��� disconnect �� ���� accept�Ǿ� ���� session�� �Ҵ�Ǿ��� ���
	// ������ �����ߴ� ioCount�� �ǵ����� �� (�ǵ����� ������ ���Ҵ� ������ io�� 0�� �� ���� ����)
	if (sessionID != pSession->_sessionID)
	{
		if (0 == InterlockedDecrement64(&pSession->_ioRefCount))
			ReleasePQCS(pSession);

		return false;
	}

	// �ܺο��� disconnect �ϴ� ����
	if (pSession->_isDisconnected)
	{
		if (0 == InterlockedDecrement64(&pSession->_ioRefCount))
			ReleasePQCS(pSession);

		return false;
	}

	// ------------------------ Disconnect Ȯ�� ------------------------
	// �׳� closesocket�� �ϰ� �Ǹ� closesocket �Լ��� CancelIoEx �Լ� ���̿��� ������ ������ 
	// ���Ҵ�Ǿ� �ٸ� ������ �� �� ����
	// �׶� ���Ҵ�� ������ IO �۾����� CancelIoEx�� ���� ���ŵǴ� ���� �߻�
	// disconnected flag�� true�� �����Ͽ� sendPacket �� recvPost �Լ� ������ ����
	InterlockedExchange8((char*)&pSession->_isDisconnected, true);

	// ���� ���� ���̾��� IO �۾� ��� ���
	CancelIoEx((HANDLE)pSession->_socketClient, NULL);

	// Disconnect �Լ����� ������Ų ���� ���� ī��Ʈ ����
	if (0 == InterlockedDecrement64(&pSession->_ioRefCount))
	{
		ReleasePQCS(pSession);
		return false;
	}
	return true;
}



void NetServer::Stop()
{
	// ...
	closesocket(_listenSocket);

	// worker thread�� ���� PQCS ����
	for (int i = 0; i < _workerThreadCount; i++)
	{
		PostQueuedCompletionStatus(_iocpHandle, 0, 0, 0);
	}

	WaitForMultipleObjects(_workerThreadCount, &_workerThreads[0], TRUE, INFINITE);
	WaitForSingleObject(_acceptThread, INFINITE);
	
	CloseHandle(_iocpHandle);
	CloseHandle(_acceptThread);
	
	for (int i = 0; i < _workerThreadCount; i++)
		CloseHandle(_workerThreads[i]);

	if (_sessionArray != nullptr)
		delete[] _sessionArray;

	WSACleanup();
}
