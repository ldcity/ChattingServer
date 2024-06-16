#ifndef __LanClient_CLASS__
#define __LanClient_CLASS__

#include "PCH.h"

// 내부 서버간 통신 모듈 클래스
class LanClient
{
public:
	LanClient();

	~LanClient();

	// 연결 -> 재연결 함수 구현 필요 
	// (특정 키 눌러서 재연결 -> 모니터링 서버를 꺼야 할 때 채팅서버도 꺼야하는 불편함 해소)
	bool Connect();

	// 세션 연결 종료
	bool DisconnectSession();

	// 패킷 전송
	bool SendPacket(CPacket* packet);

	// Server Start
	bool Start(const wchar_t* IP, unsigned short PORT, int createWorkerThreadCnt, int runningWorkerThreadCnt, bool nagelOff);

	// Server Stop
	void Stop();

protected:
	// ==========================================================
	// 접속처리 완료 후 호출 
	// [PARAM] __int64 sessionID
	// [RETURN] X
	// ==========================================================
	virtual void OnClientJoin() = 0;

	// ==========================================================
	// 접속해제 후 호출, Player 관련 리소스 해제
	// [PARAM] __int64 sessionID
	// [RETURN] X 
	// ==========================================================
	virtual void OnClientLeave() = 0;

	// ==========================================================
	// 패킷 수신 완료 후
	// [PARAM] __int64 sessionID, CPacket* packet
	// [RETURN] X 
	// ==========================================================
	virtual void OnRecv(CPacket* packet) = 0;

	// ==========================================================
	// Error Check
	// [PARAM] int errorCode, wchar_t* msg
	// [RETURN] X 
	// ==========================================================
	virtual void OnError(int errorCode, const wchar_t* msg) = 0;

private:
	friend unsigned __stdcall LanWorkerThread(void* param);

	bool LanWorkerThread_serv();

	// 송수신 버퍼 등록 후, 송수신 함수 호출
	bool RecvPost();
	bool SendPost();

	bool RecvProc(long cbTransferred);
	bool SendProc(long cbTransferred);

	// 세션 리소스 정리 및 해제
	void ReleaseSession();

	inline void ReleasePQCS()
	{
		PostQueuedCompletionStatus(IOCPHandle, 0, (ULONG_PTR)&mSession, (LPOVERLAPPED)PQCSTYPE::RELEASE);
	}


	// 클라이언트용 변수
private:
	stLanSESSION mSession;

	wchar_t mIP[16];											// Server IP
	unsigned short mPORT;										// Server Port

	bool mOk;

private:
	bool mNagleOff;

	HANDLE IOCPHandle;										// IOCP Handle

	std::vector<HANDLE> mWorkerThreads;							// Worker Threads Count

	long s_workerThreadCount;								// Worker Thread Count (Server)
	long s_runningThreadCount;								// Running Thread Count (Server)

	enum PQCSTYPE
	{
		SENDPOST = 100,
		SENDPOSTDICONN,
		RELEASE,
		STOP,
	};

protected:
	// logging
	Log* logger;

	// 모니터링용 변수 (1초 기준)
	// 이해의 편의를 위해 TPS가 들어간 변수는 1초당 발생하는 건수를 계산, 나머지는 총 누적 합계를 나타냄
	__int64 connectTryTPS;
	__int64 connectSuccessTPS;
	__int64 connectCnt;
	__int64 connectFailCnt;
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
	bool startMonitering;
};

#endif // !__LanClient_CLASS__


