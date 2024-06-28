#pragma once
#ifndef __NetServer_CLASS__
#define __NetServer_CLASS__

#include "PCH.h"

#include <process.h>
#include <queue>

#define MAX_SESSION 500

// 외부 통신 모듈 클래스
class NetServer
{
public:
	NetServer();

	~NetServer();

public:
	// 세션 연결 종료
	bool DisconnectSession(uint64_t sessionID);

	// 패킷 전송
	bool SendPacket(uint64_t sessionID, CPacket* packet);

	// Server Start
	bool Start(const wchar_t* IP, unsigned short PORT, int createWorkerThreadCnt, int runningWorkerThreadCnt, bool nagelOff, bool zeroCopyOff, int maxAcceptCnt, unsigned char packet_code, unsigned char packet_key, DWORD timeout);

	// Server Stop
	void Stop();

protected:
	// ==========================================================
	// White IP Check 
	// [PARAM] const wchar_t* IP, unsigned short PORT
	// [RETURN] TRUE : 접속 허용 / FALSE : 접속 거부
	// ==========================================================
	virtual bool OnConnectionRequest(const wchar_t* IP, unsigned short PORT) = 0;

	// ==========================================================
	// 접속처리 완료 후 호출 
	// [PARAM] __int64 sessionID
	// [RETURN] X
	// ==========================================================
	virtual void OnClientJoin(uint64_t sessionID) = 0;

	// ==========================================================
	// 접속해제 후 호출, Player 관련 리소스 해제
	// [PARAM] __int64 sessionID
	// [RETURN] X 
	// ==========================================================
	virtual void OnClientLeave(uint64_t sessionID) = 0;

	// ==========================================================
	// 패킷 수신 완료 후 호출
	// [PARAM] __int64 sessionID, CPacket* packet
	// [RETURN] X 
	// ==========================================================
	virtual void OnRecv(uint64_t sessionID, CPacket* packet) = 0;

	// ==========================================================
	// 세션 타임아웃 관련 처리
	// [PARAM] __int64 sessionID
	// [RETURN] X 
	// ==========================================================
	virtual void OnTimeout(uint64_t sessionID) = 0;

	// ==========================================================
	// Error Check
	// [PARAM] int errorCode, wchar_t* msg
	// [RETURN] X 
	// ==========================================================
	virtual void OnError(int errorCode, const wchar_t* msg) = 0;

private:
	friend unsigned __stdcall AcceptThread(void* param);
	friend unsigned __stdcall WorkerThread(void* param);
	friend unsigned __stdcall TimeoutThread(void* param);

	bool AcceptThread_serv();
	bool mWorkerThread_serv();
	bool TimeoutThread_serv();

	// 완료통지 후, 작업 처리
	bool RecvProc(stSESSION* pSession, long cbTransferred);
	bool SendProc(stSESSION* pSession, long cbTransferred);

	// 송수신 버퍼 등록 후, 송수신 함수 호출
	bool RecvPost(stSESSION* pSession);
	bool SendPost(stSESSION* pSession);

	// 세션 리소스 정리 및 해제
	void ReleaseSession(stSESSION* pSession);

	// uniqueID와 index로 고유한 SessionID 생성
	 uint64_t  CreateSessionID(uint64_t uniqueID, int index)
	{
		return (uint64_t)((uint64_t)index | (uniqueID << SESSION_ID_BITS));
	}

	 void ReleasePQCS(stSESSION* pSession)
	{
		PostQueuedCompletionStatus(IOCPHandle, 0, (ULONG_PTR)pSession, (LPOVERLAPPED)PQCSTYPE::RELEASE);
	}

	 int GetSessionIndex(uint64_t sessionID)
	{
		return (int)(sessionID & SESSION_INDEX_MASK);
	}

	 uint64_t GetSessionID(uint64_t sessionID)
	{
		return (uint64_t)(sessionID >> SESSION_ID_BITS);
	}

	 // 타임아웃 주기 : 현재 시간 ~ 서버의 타임아웃 시간 (ms 단위)
	 void SetTimeout(stSESSION* session)
	 {
		 InterlockedExchange(&session->Timer, timeGetTime() + mTimeout);
	 }

	 // 타임아웃 주기 : 현재 시간 ~ 매개변수로 받은 타임아웃 시간 (ms 단위)
	 void SetTimeout(stSESSION* session, DWORD timeout)
	 {
		 InterlockedExchange(&session->Timer, timeGetTime() + timeout);
	 }

private:
	// 서버용 변수
	SOCKET ListenSocket;								// Listen Socket
	unsigned short ServerPort;							// Server Port

	HANDLE IOCPHandle;									// IOCP Handle
	HANDLE mAcceptThread;								// Accept Thread
	HANDLE mTimeoutThread;								// Timeout Thread
	vector<HANDLE> mWorkerThreads;						// Worker Threads Count
	
	long s_workerThreadCount;							// Worker Thread Count (Server)
	long s_runningThreadCount;							// Running Thread Count (Server)

	long s_maxAcceptCount;								// Max Accept Count

	stSESSION* SessionArray;					// Session Array			
	LockFreeStack<int> AvailableIndexStack;		// Available Session Array Index

	DWORD mServerTime;									// Server Time
	DWORD mTimeout;										// 외부 contents 단에서 설정한 타임아웃

	// PQCS 호출할 때 사용
	enum PQCSTYPE
	{
		SENDPOST = 100,		// Send 등록
		SENDPOSTDICONN,		// Send 등록 후 Disconnect
		RELEASE,			// Release
		TIMEOUT,
		STOP,				// Stop
	};

protected:
	// logging
	Log* logger;

	// 모니터링용 변수 (1초 기준)
	// 이해의 편의를 위해 TPS가 들어간 변수는 1초당 발생하는 건수를 계산, 나머지는 총 누적 합계를 나타냄

	__int64 acceptCount;							// Accept Session Count.
	__int64 acceptTPS;								// Accept TPS
	__int64 sessionCnt;								// Session Total Cnt
	__int64 releaseCount;							// Release Session Count
	__int64 releaseTPS;								// Release TPS
	__int64 recvMsgTPS;								// Recv Packet TPS
	__int64 sendMsgTPS;								// Send Packet TPS
	__int64 recvMsgCount;							// Total Recv Packet Count
	__int64 sendMsgCount;							// Total Send Packet Count
	__int64 recvCallTPS;							// Recv Call TPS
	__int64 sendCallTPS;							// Send Call TPS
	__int64 recvCallCount;							// Total Recv Call Count
	__int64 sendCallCount;							// Total Send Call Count
	__int64 recvPendingTPS;							// Recv Pending TPS
	__int64 sendPendingTPS;							// Send Pending TPS
	__int64 recvBytesTPS;							// Recv Bytes TPS
	__int64 sendBytesTPS;							// Send Bytes TPS
	__int64 recvBytes;								// Total Recv Bytes
	__int64 sendBytes;								// Total Send Bytes
	__int64 workerThreadCount;						// Worker Thread Count (Monitering)
	__int64 runningThreadCount;						// Running Thread Count (Monitering)

	__int64 pqcsCallTotal;							// pqcs call Total
	__int64 pqcsCallTPS;							// pqcs call TPS
	__int64 sendpacketTotal;
	__int64 sendpacketTPS;
	__int64 eqTotal;
	__int64 eqTPS;
	__int64 dqTotal;
	__int64 dqTPS;

	bool startMonitering;
};

#endif // !__NetServer_CLASS__


