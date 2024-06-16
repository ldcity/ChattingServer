#include "PCH.h"
#include "LanClient.h"


// ========================================================================
// Thread Call
// ========================================================================

// Worker Thread Call
unsigned __stdcall LanWorkerThread(void* param)
{
	LanClient* lanServ = (LanClient*)param;

	lanServ->LanWorkerThread_serv();

	return 0;
}


LanClient::LanClient() :  IOCPHandle(0), mIP{ 0 }, mPORT(0), mWorkerThreads{ 0 }, recvMsgTPS(0), sendMsgTPS(0),
recvMsgCount(0), sendMsgCount(0), recvCallTPS(0), sendCallTPS(0), recvCallCount(0), sendCallCount(0), recvPendingTPS(0), sendPendingTPS(0),
recvBytesTPS(0), sendBytesTPS(0), recvBytes(0), sendBytes(0), s_workerThreadCount(0), s_runningThreadCount(0), startMonitering(false)
{
	// ========================================================================
	// Initialize
	// ========================================================================
	wprintf(L"LanClient Initializing...\n");

	mSession.sessionID = -1;
	mSession.m_socketClient = INVALID_SOCKET;
	ZeroMemory(mSession.IP_str, sizeof(mSession.IP_str));
	mSession.IP_num = 0;
	mSession.PORT = 0;
	//LastRecvTime = 0;

	ZeroMemory(&mSession.m_stRecvOverlapped, sizeof(OVERLAPPED));
	ZeroMemory(&mSession.m_stSendOverlapped, sizeof(OVERLAPPED));
	mSession.recvRingBuffer.ClearBuffer();

	mSession.sendPacketCount = 0;
	mSession.ioRefCount = 0;			// accept 이후 바로 recv 걸어버리기 때문에 항상 default가 1
	mSession.sendFlag = false;
	mSession.isDisconnected = false;
	mSession.isUsed = false;

	mOk = false;

	logger = new Log(L"LanClient");

	WSADATA  wsaData;

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		int initError = WSAGetLastError();

		return;
	}
}

LanClient::~LanClient()
{
	Stop();
}

bool LanClient::Start(const wchar_t* IP, unsigned short PORT, int createWorkerThreadCnt, int runningWorkerThreadCnt, bool nagelOff) 
{
	wmemcpy_s(mIP, wcslen(IP) + 1, IP, wcslen(IP) + 1);
	mPORT = PORT;
	mNagleOff = nagelOff;

	SYSTEM_INFO si;
	GetSystemInfo(&si);

	// CPU Core Counting
	// Worker Thread 개수가 0 이하라면, 코어 개수 * 2 로 재설정
	if (createWorkerThreadCnt <= 0)
		s_workerThreadCount = si.dwNumberOfProcessors * 2;
	else
		s_workerThreadCount = createWorkerThreadCnt;
	
	// Running Thread가 CPU Core 개수를 초과한다면 CPU Core 개수로 재설정
	if (runningWorkerThreadCnt > si.dwNumberOfProcessors)
		s_runningThreadCount = si.dwNumberOfProcessors;
	else
		s_runningThreadCount = runningWorkerThreadCnt;

	// Create I/O Completion Port
	IOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, s_runningThreadCount);
	if (IOCPHandle == NULL)
	{
		int iocpError = WSAGetLastError();
		
		return false;
	}

	// ========================================================================
	// Create Thread
	// ======================================================================== 
 
	// Worker Thread
	mWorkerThreads.resize(s_workerThreadCount);
	for (int i = 0; i < mWorkerThreads.size(); i++)
	{
		mWorkerThreads[i] = (HANDLE)_beginthreadex(NULL, 0, LanWorkerThread, this, 0, NULL);
		if (mWorkerThreads[i] == NULL)
		{
			int threadError = GetLastError();

			return false;
		}
	}

	return true;
}

bool LanClient::Connect()
{
	// 이미 접속중인 세션이 또 접속하면 안됨
	if (mSession.isUsed == true)
	{
		logger->logger(dfLOG_LEVEL_ERROR, __LINE__, L"connect # already connected");
		DisconnectSession();
		return false;
	}

	if (mSession.m_socketClient != INVALID_SOCKET)
		return false;


	// Create Socket
	mSession.m_socketClient = socket(AF_INET, SOCK_STREAM, 0);
	if (INVALID_SOCKET == mSession.m_socketClient)
	{
		int sockError = WSAGetLastError();

		return false;
	}
	
	// TCP Send Buffer Remove - zero copy
	int sendVal = 0;
	if (setsockopt(mSession.m_socketClient, SOL_SOCKET, SO_SNDBUF, (const char*)&sendVal, sizeof(sendVal)) == SOCKET_ERROR)
	{
		int setsockoptError = WSAGetLastError();

		return false;
	}

	if (mNagleOff)
	{
		// Nagle off
		if (setsockopt(mSession.m_socketClient, IPPROTO_TCP, TCP_NODELAY, (const char*)&mNagleOff, sizeof(mNagleOff)) == SOCKET_ERROR)
		{
			int setsockoptError = WSAGetLastError();

			return false;
		}
	}

	// TIME_WAIT off
	struct linger ling;
	ling.l_onoff = 1;
	ling.l_linger = 0;
	if (setsockopt(mSession.m_socketClient, SOL_SOCKET, SO_LINGER, (const char*)&ling, sizeof(ling)) == SOCKET_ERROR)
	{
		int setsockoptError = WSAGetLastError();

		return false;
	}

	SOCKADDR_IN serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(mPORT);
	InetPtonW(AF_INET, mIP, &serverAddr.sin_addr);
	
	// 세션 참조 카운트를 올려줌
	InterlockedExchange64(&mSession.ioRefCount, 1);

	if (connect(mSession.m_socketClient, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
	{
		int connectError = WSAGetLastError();

		logger->logger(dfLOG_LEVEL_ERROR, __LINE__, L"connect # connect error : %d", connectError);

		closesocket(mSession.m_socketClient);
		mSession.m_socketClient = INVALID_SOCKET;

		return false;
	}

	// 링버퍼 내부에 남아있는 거 처리
	mSession.recvRingBuffer.ClearBuffer();
	ZeroMemory(&mSession.m_stSendOverlapped, sizeof(OVERLAPPED));
	ZeroMemory(&mSession.m_stRecvOverlapped, sizeof(OVERLAPPED));

	// 종료flag 셋팅
	InterlockedExchange8((char*)&mSession.isDisconnected, false);

	InterlockedExchange8((char*)&mSession.isUsed, true);

	InterlockedExchange8((char*)&mSession.sendFlag, false);

	if (CreateIoCompletionPort((HANDLE)mSession.m_socketClient, IOCPHandle, (ULONG_PTR)&mSession, 0) == NULL)
	{
		int ciocpError = WSAGetLastError();

		logger->logger(dfLOG_LEVEL_ERROR, __LINE__, L"connect # iocp error : %d", ciocpError);

		closesocket(mSession.m_socketClient);
		mSession.m_socketClient = INVALID_SOCKET;

		CloseHandle(IOCPHandle);

		return false;
	}

	OnClientJoin();

	RecvPost();

	// 올린 참조카운트 감소
	if (0 == InterlockedDecrement64(&mSession.ioRefCount))
	{
		ReleaseSession();
	}

	
	return true;
}

// Worker Thread
bool LanClient::LanWorkerThread_serv()
{
	DWORD threadID = GetCurrentThreadId();

	stSESSION* pSession = nullptr;
	BOOL bSuccess = true;
	long cbTransferred = 0;
	LPOVERLAPPED pOverlapped = nullptr;

	bool completionOK;

	while (true)
	{
		// 초기화
		cbTransferred = 0;
		pSession = nullptr;
		pOverlapped = nullptr;
		completionOK = false;

		// GQCS call
			// client가 send조차 하지않고 바로 disconnect 할 경우 -> WorkerThread에서 recv 0을 위한 GQCS가 깨어남
		bSuccess = GetQueuedCompletionStatus(IOCPHandle, (LPDWORD)&cbTransferred, (PULONG_PTR)&pSession,
			&pOverlapped, INFINITE);

		// IOCP Error or TIMEOUT or PQCS로 직접 NULL 던짐
		// 세션 멤버변수에서 error counting을 하여 5회 이상 발생했을 시, 해제 작업 들어가는 아이디어도 있긴 함
		// 완료통지가 안왔을 경우, 이번 루프를 skip
		if (pOverlapped == NULL)
		{
			int iocpError = WSAGetLastError();
			logger->logger(dfLOG_LEVEL_ERROR, __LINE__, L"worker # iocp error : %d", iocpError);

			PostQueuedCompletionStatus(IOCPHandle, 0, 0, 0);

			break;
		}

		// Send / Recv Proc
		else if (pOverlapped == &pSession->m_stRecvOverlapped && cbTransferred > 0)
		{
			completionOK = RecvProc(cbTransferred);
		}
		else if (pOverlapped == &pSession->m_stSendOverlapped && cbTransferred > 0)
		{
			completionOK = SendProc(cbTransferred);
		
		}

		// I/O 완료 통지가 더이상 없다면 세션 해제 작업
		if (0 == InterlockedDecrement64(&pSession->ioRefCount))
		{
			ReleaseSession();
		}

	}

	return true;
}

bool LanClient::RecvProc(long cbTransferred)
{
	mSession.recvRingBuffer.MoveWritePtr(cbTransferred);

	int useSize = mSession.recvRingBuffer.GetUseSize();

	// Recv Message Process
	while (useSize > 0)
	{
		LANHeader header;

		// Header 크기만큼 있는지 확인
		if (useSize <= sizeof(LANHeader))
			break;

		// Header Peek
		mSession.recvRingBuffer.Peek((char*)&header, sizeof(LANHeader));

		// Packet 크기만큼 있는지 확인
		if (useSize < sizeof(LANHeader) + header.len)
			break;


		// Len 확인 (페이로드 길이 확인)
		if (header.len <= 0 || header.len > MAX_PACKET_LEN)
		{
			logger->logger(dfLOG_LEVEL_ERROR, __LINE__, L"RecvProc # len error > size : %d", header.len);
			DisconnectSession();

			return false;
		}

		// packet alloc
		CPacket* packet = CPacket::Alloc();

		// payload 크기만큼 데이터 Dequeue
		mSession.recvRingBuffer.Dequeue(packet->GetLanBufferPtr(), header.len + CPacket::en_PACKET::LAN_HEADER_SIZE);

		// payload 크기만큼 packet write pos 이동
		packet->MoveWritePos(header.len);

		// Total Recv Message Count
		InterlockedIncrement64((LONG64*)&recvMsgCount);

		// Recv Message TPS
		InterlockedIncrement64((LONG64*)&recvMsgTPS);

		// Total Recv Bytes
		InterlockedAdd64((LONG64*)&recvBytes, header.len);

		// Recv Bytes TPS
		InterlockedAdd64((LONG64*)&recvBytesTPS, header.len);

		// 컨텐츠 쪽 recv 처리
		OnRecv(packet);

		useSize = mSession.recvRingBuffer.GetUseSize();
	}

	// Recv 재등록
	RecvPost();


	return true;
}

bool LanClient::SendProc(long cbTransferred)
{
	// sendPost에서 사이즈 0일 경우를 걸러냈는데도 이 조건이 발생하는 경우는 error
	if (mSession.sendPacketCount == 0)
		CRASH();

	int totalSendBytes = 0;
	int iSendCount;

	// send 완료 통지된 패킷 제거
	for (iSendCount = 0; iSendCount < mSession.sendPacketCount; iSendCount++)
	{
		totalSendBytes += mSession.SendPackets[iSendCount]->GetDataSize();
		CPacket::Free(mSession.SendPackets[iSendCount]);
	}

	// Total Send Bytes
	InterlockedAdd64((long long*)&sendBytes, totalSendBytes);

	// Send Bytes TPS
	InterlockedAdd64((long long*)&sendBytesTPS, totalSendBytes);

	// Total Send Message Count
	InterlockedAdd64((long long*)&sendMsgCount, mSession.sendPacketCount);

	// Send Message TPS
	InterlockedAdd64((long long*)&sendMsgTPS, mSession.sendPacketCount);

	mSession.sendPacketCount = 0;

	// 전송 중 flag를 다시 미전송 상태로 되돌리기
	InterlockedExchange8((char*)&mSession.sendFlag, false);

	// 1회 send 후, sendQ에 쌓여있던 나머지 데이터 모두 send
	if (mSession.sendQ.GetSize() > 0)
	{
		// sendFlag가 false인걸 한번 확인한 다음 진입
		if (mSession.sendFlag == false)
			SendPost();
	}

	return true;
}


bool LanClient::RecvPost()
{
	// recv 걸기 전에 외부에서 disconnect 호출될 수 있음
	// -> recv 안 걸렸을 때 io 취소해도 의미 없으니까 사전에 recvpost 막아버림
	if (mSession.isDisconnected)
		return false;

	// 링버퍼 등록
	WSABUF wsa[2] = { 0 };
	int wsaCnt = 1;
	DWORD flags = 0;

	int freeSize = mSession.recvRingBuffer.GetFreeSize();
	int directEequeueSize = mSession.recvRingBuffer.DirectEnqueueSize();

	if (freeSize == 0)
		return false;

	wsa[0].buf = mSession.recvRingBuffer.GetWriteBufferPtr();
	wsa[0].len = directEequeueSize;

	// 링버퍼 내부에서 빈 공간이 두 섹션으로 나뉠 경우
	if (freeSize > directEequeueSize)
	{
		wsa[1].buf = mSession.recvRingBuffer.GetBufferPtr();
		wsa[1].len = freeSize - directEequeueSize;
		++wsaCnt;
	}

	// recv overlapped I/O 구조체 reset
	ZeroMemory(&mSession.m_stRecvOverlapped, sizeof(OVERLAPPED));

	// recv
	// ioCount : WSARecv 완료 통지가 리턴보다 먼저 떨어질 수 있으므로 WSARecv 호출 전에 증가시켜야 함
	InterlockedIncrement64(&mSession.ioRefCount);
	int recvRet = WSARecv(mSession.m_socketClient, wsa, wsaCnt, NULL, &flags, &mSession.m_stRecvOverlapped, NULL);
	InterlockedIncrement64(&recvCallCount);
	InterlockedIncrement64(&recvCallTPS);

	// 예외처리
	if (recvRet == SOCKET_ERROR)
	{
		int recvError = WSAGetLastError();

		if (recvError != WSA_IO_PENDING)
		{
			if (recvError != ERROR_10054 && recvError != ERROR_10058 && recvError != ERROR_10060)
			{
				// 에러				
				OnError(recvError, L"RecvPost # WSARecv Error\n");
			}

			// Pending이 아닐 경우, 완료 통지 실패
			if (0 == InterlockedDecrement64(&mSession.ioRefCount))
			{
				ReleaseSession();
			}
			return false;
		}
		// Pending일 경우
		else
		{
			InterlockedIncrement64(&recvPendingTPS);

			// Pending 걸렸는데, 이 시점에 disconnect되면 이 때 남아있던 비동기 io 정리해줘야함
			if (mSession.isDisconnected)
			{
				CancelIoEx((HANDLE)mSession.m_socketClient, &mSession.m_stRecvOverlapped);
			}

		}
	}

	return true;
}

bool LanClient::SendPost()
{
	// 1회 송신 제한을 위한 flag 확인 (send call 횟수 줄이려고 -> send call 자체가 느림
	// true면 최조 사용 아님
	// false -> true 면 최초 사용
	// true -> true 면 최초 사용이 아님
	if ((mSession.sendFlag == true) || true == InterlockedExchange8((char*)&mSession.sendFlag, true))
		return false;

	// SendQ가 비어있을 수 있음
	// -> 다른 스레드에서 Dequeue 진행했을 경우
	if (mSession.sendQ.GetSize() <= 0)
	{
		// * 일어날 수 있는 상황
		// 다른 스레드에서 dequeue를 전부 해서 size가 0이 돼서 이 조건문에 진입한건데
		// 이 위치에서 또다른 스레드에서 패킷이 enqueue되고 sendpost가 일어나게 되면
		// 아직 sendFlag가 false로 변경되지 않은 상태이기 때문에 sendpost 함수 상단 조건에 걸려 빠져나가게 됨
		// 그 후, 이 스레드로 다시 돌아오게 될 경우, 
		// sendQ에 패킷이 있는 상태이므로 sendFlas를 false로 바꿔주기만 하고 리턴하는게 아니라
		// 한번 더 sendQ의 size 확인 후 sendpost PQCS 날릴 지 결졍

		InterlockedExchange8((char*)&mSession.sendFlag, false);

		// 그 사이에 SendQ에 Enqueue 됐다면 다시 SendPost Call 
		if (mSession.sendQ.GetSize() > 0)
		{
			// sendFlag가 false인걸 한번 확인한 다음 진입
			if (mSession.sendFlag == false)
				SendPost();
		}
		return false;
	}

	int deqIdx = 0;

	// 링버퍼 등록
	WSABUF wsa[MAX_WSA_BUF] = { 0 };

	while (mSession.sendQ.Dequeue(mSession.SendPackets[deqIdx]))
	{
		// 패킷 시작 포인터 (헤더가 시작점)
		wsa[deqIdx].buf = mSession.SendPackets[deqIdx]->GetLanBufferPtr();

		// 패킷 크기 (헤더 포함)
		wsa[deqIdx].len = mSession.SendPackets[deqIdx]->GetLanDataSize();

		deqIdx++;

		if (deqIdx >= MAX_WSA_BUF)
			break;
	}

 	mSession.sendPacketCount = deqIdx;

	// send overlapped I/O 구조체 reset
	ZeroMemory(&mSession.m_stSendOverlapped, sizeof(OVERLAPPED));

	// send
	// ioCount : WSASend 완료 통지가 리턴보다 먼저 떨어질 수 있으므로 WSASend 호출 전에 증가시켜야 함
	InterlockedIncrement64(&mSession.ioRefCount);
	int sendRet = WSASend(mSession.m_socketClient, wsa, deqIdx, NULL, 0, &mSession.m_stSendOverlapped, NULL);
	InterlockedIncrement64(&sendCallCount);
	InterlockedIncrement64(&sendCallTPS);
	
	// 예외처리
	if (sendRet == SOCKET_ERROR)
	{
		int sendError = WSAGetLastError();

		// default error는 무시
		if (sendError != WSA_IO_PENDING)
		{
			if (sendError != ERROR_10054 && sendError != ERROR_10058 && sendError != ERROR_10060)
			{
				OnError(sendError, L"SendPost # WSASend Error\n");
			}

			// Pending이 아닐 경우, 완료 통지 실패 -> IOCount값 복원
			if (0 == InterlockedDecrement64(&mSession.ioRefCount))
			{
				ReleaseSession();
			}

			return false;
		}
		else
			InterlockedIncrement64(&sendPendingTPS);
	}

	return true;
}


bool LanClient::SendPacket(CPacket* packet)
{
	// 세션 사용 참조카운트 증가 & Release 중인지 동시 확인
	// Release 비트값이 1이면 ReleaseSession 함수에서 ioCount = 0, releaseFlag = 1 인 상태
	// 어처피 다시 release가서 해제될 세션이므로 ioRefCount 감소시키지 않아도 됨
	if (InterlockedIncrement64(&mSession.ioRefCount) & RELEASEMASKING)
	{
		return false;
	}

	// ------------------------------------------------------------------------------------
	// Release 수행 없이 이곳에서만 세션 사용하려는 상태

	// 외부에서 disconnect 하는 상태
	if (mSession.isDisconnected)
	{
		if (0 == InterlockedDecrement64(&mSession.ioRefCount))
		{
			ReleaseSession();
		}

		return false;
	}

	// lan 헤더 셋팅
	packet->SetLanHeader();

	// Enqueue한 패킷을 다른 곳에서 사용하므로 패킷 참조카운트 증가 -> Dequeue할 때 감소
	packet->addRefCnt();

	// packet 포인터 enqueue
	mSession.sendQ.Enqueue(packet);

	// sendFlag가 false인걸 한번 확인한 다음 진입
	if (mSession.sendFlag == false)
		SendPost();

	// sendPacket 함수에서 증가시킨 세션 참조 카운트 감소
	if (0 == InterlockedDecrement64(&mSession.ioRefCount))
	{
		ReleaseSession();

		return false;
	}
}

void LanClient::ReleaseSession()
{
	// 세션 송신 다 마치고 종료해야함 
	// 세션 메모리 정리
	// 소켓 정리
	// 세선 해제
	// ioCount == 0 && releaseFlag == 0 => release = 1 (인터락 함수로 해결)
	// 다른 곳에서 해당 세션을 사용(sendpacket or disconnect)하는지 확인
	if (InterlockedCompareExchange64(&mSession.ioRefCount, RELEASEMASKING, 0) != 0)
	{
		return;
	}

	//-----------------------------------------------------------------------------------
	// Release 실제 진입부
	//-----------------------------------------------------------------------------------
	//ioCount = 0, releaseFlag = 1 인 상태

	uint64_t _sessionID = mSession.sessionID;

	mSession.sessionID = -1;

	SOCKET sock = mSession.m_socketClient;

	// 소켓 Invalid 처리하여 더이상 해당 소켓으로 I/O 못받게 함
	mSession.m_socketClient = INVALID_SOCKET;

	InterlockedExchange8((char*)&mSession.sendFlag, false);

	// recv는 더이상 받으면 안되므로 소켓 close
	closesocket(sock);

	// Send Packet 관련 리소스 정리
	// SendQ에서 Dqeueue하여 SendPacket 배열에 넣었지만 아직 WSASend 못해서 남아있는 패킷 정리
	for (int iSendCount = 0; iSendCount < mSession.sendPacketCount; iSendCount++)
	{
		CPacket::Free(mSession.SendPackets[iSendCount]);
	}

	mSession.sendPacketCount = 0;

	// SendQ에 남아있다는 건 WSABUF에 꽂아넣지도 못한 상태 
	if (mSession.sendQ.GetSize() > 0)
	{
		CPacket* packet = nullptr;
		while (mSession.sendQ.Dequeue(packet))
		{
			CPacket::Free(packet);
		}
	}

	ZeroMemory(mSession.IP_str, sizeof(mSession.IP_str));
	mSession.IP_num = 0;
	mSession.PORT = 0;

	ZeroMemory(&mSession.m_stRecvOverlapped, sizeof(OVERLAPPED));
	ZeroMemory(&mSession.m_stSendOverlapped, sizeof(OVERLAPPED));
	mSession.recvRingBuffer.ClearBuffer();

	mSession.ioRefCount = 0;			// accept 이후 바로 recv 걸어버리기 때문에 항상 default가 1
	
	InterlockedExchange64(&mSession.ioRefCount, 0);
	InterlockedExchange8((char*)&mSession.isDisconnected, false);
	InterlockedExchange8((char*)&mSession.isUsed, false);

	// 사용자(Player) 관련 리소스 해제 (호출 후에 해당 세션이 사용되면 안됨)
	OnClientLeave();
}

bool LanClient::DisconnectSession()
{
	// 세션 사용 참조카운트 증가 & Release 중인지 동시 확인
	// Release 비트값이 1이면 ReleaseSession 함수에서 ioCount = 0, releaseFlag = 1 인 상태
	// 어처피 다시 release가서 해제될 세션이므로 ioRefCount 감소시키지 않아도 됨
	if (InterlockedIncrement64(&mSession.ioRefCount) & RELEASEMASKING)
	{
		return false;
	}

	// Release 수행 없이 이곳에서만 세션 사용하려는 상태

	// 외부에서 disconnect 하는 상태
	if (mSession.isDisconnected)
	{
		if (0 == InterlockedDecrement64(&mSession.ioRefCount))
		{
			ReleaseSession();
			// ReleasePQCS();
		}

		return false;
	}

	// ------------------------ Disconnect 확정 ------------------------
	// 그냥 closesocket을 하게 되면 closesocket 함수와 CancelIoEx 함수 사이에서 해제된 세션이 
	// 재할당되어 다른 세션이 될 수 있음
	// 그때 재할당된 세션의 IO 작업들이 CancelIoEx에 의해 제거되는 문제 발생
	// disconnected flag를 true로 변경하면 sendPacket 과 recvPost 함수 진입을 막음
	InterlockedExchange8((char*)&mSession.isDisconnected, true);

	// 현재 IO 작업 모두 취소
	CancelIoEx((HANDLE)mSession.m_socketClient, NULL);

	// Disconnect 함수에서 증가시킨 세션 참조 카운트 감소
	if (0 == InterlockedDecrement64(&mSession.ioRefCount))
	{
		ReleaseSession();

		return false;
	}

	return true;
}

void LanClient::Stop()
{
	// stop 함수 추후 구현 완료

	// worker thread로 종료 PQCS 날김
	for (int i = 0; i < s_workerThreadCount; i++)
	{
		PostQueuedCompletionStatus(IOCPHandle, 0, 0, 0);
	}


	WaitForMultipleObjects(s_workerThreadCount, &mWorkerThreads[0], TRUE, INFINITE);

	closesocket(mSession.m_socketClient);

	CloseHandle(IOCPHandle);

	for (int i = 0; i < s_workerThreadCount; i++)
		CloseHandle(mWorkerThreads[i]);

	WSACleanup();
}