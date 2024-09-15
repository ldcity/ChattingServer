#pragma once
#ifndef __SESSION__
#define __SESSION__

#include "RingBuffer.h"
#include "LockFreeQueue.h"

#define MAX_WSA_BUF 100
#define RELEASEMASKING 0x80000000				// ioRefCount에서 ReleaseFlag만 뽑아냄

const char SESSION_ID_BITS = 47;
const __int64 SESSION_INDEX_MASK = 0x00007FFFFFFFFFFF;

// Session Struct
class stSESSION
{
public:
	stSESSION() {}

	void Init(uint64_t sessionID, SOCKET socket, SOCKADDR_IN& addr)
	{
		sessionID = sessionID;
		_socketClient = socket;
		_addr = addr;

		_timeout = 0;
		_timer = 0;

		_sendPacketCount = 0;

		ZeroMemory(&_stRecvOverlapped, sizeof(OVERLAPPED));
		ZeroMemory(&_stSendOverlapped, sizeof(OVERLAPPED));
		_recvRingBuffer.ClearBuffer();

		// 세션 참조카운트(ioRefCount) 증가
		// accept 단계에서 소켓 얻은 후, 세션 사용을 할 때, 세션이 해제되는 걸 방지하기 위한 참조카운트! 
		// increment가 아니라 exchange하는 이유 
		// -> 무조건 default 참조 카운트를 1로 제한하기 위함

		// * 일어날 수 있는 상황
		// recvpost 함수에 진입하여 refcount 증가 후, WSARecv까지 걸어주고 IO_PENDING 에러가 떴는데,
		// 완료 통지가 오기 전에 이 위치에서 rst or 컨텐츠에서 disconnect되면 refcount가 0이 되어 이 시점에 relese됨
		// 그리고 재할당되어 다른 세션이 되어있을 수 있어 완료 통지가 재할당된 세션으로 올 수 있음
		InterlockedExchange64(&_ioRefCount, 1);

		// flag 셋팅
		InterlockedExchange8((char*)&_sendFlag, false);
		InterlockedExchange8((char*)&_isDisconnected, false);
		InterlockedExchange8((char*)&_sendDisconnFlag, false);
	}

	void Release()
	{
		InterlockedExchange8((char*)&_isDisconnected, false);
		InterlockedExchange8((char*)&_sendFlag, false);

		_sessionID = -1;

		// 소켓 Invalid 처리하여 더이상 해당 소켓으로 I/O 못받게 함
		_socketClient = INVALID_SOCKET;

		// Send Packet 관련 리소스 정리
		// SendQ에서 Dqeueue하여 SendPacket 배열에 넣었지만 아직 WSASend 못해서 남아있는 패킷 정리
		for (int iSendCount = 0; iSendCount < _sendPacketCount; iSendCount++)
			CPacket::Free(_sendPackets[iSendCount]);

		_sendPacketCount = 0;

		// SendQ에 남아있다는 건 WSABUF에 꽂아넣지도 못한 상태 
		if (_sendQ.GetSize() > 0)
		{
			CPacket* packet = nullptr;

			// PacketPool에 반환
			while (_sendQ.Dequeue(packet))
				CPacket::Free(packet);
		}
	}

	~stSESSION()
	{
	}

public:
	uint64_t _sessionID;										// Session ID
	SOCKET _socketClient;										// Client Socket

	SOCKADDR_IN _addr;											// Socket Addr

	DWORD _timeout;												// Last Recv Time
	DWORD _timer;												// Timeout Timer

	OVERLAPPED _stRecvOverlapped;								// Recv Overlapped I/O Struct
	OVERLAPPED _stSendOverlapped;								// Send Overlapped I/O Struct

	RingBuffer _recvRingBuffer;									// Recv RingBuffer
	LockFreeQueue<CPacket*> _sendQ;								// Send LockFreeQueue

	CPacket* _sendPackets[MAX_WSA_BUF] = { nullptr };			// Send Packets 배열

	alignas(64) int _sendPacketCount;							// WSABUF Count
	alignas(64) __int64 _ioRefCount;							// I/O Count & Session Ref Count
	alignas(64) bool _sendFlag;									// Sending Message Check
	alignas(64) bool _isDisconnected;							// Session Disconnected
	alignas(64) bool _sendDisconnFlag;

};


class stLanSESSION
{
public:
	stLanSESSION() : _sessionID(-1), _socketClient(INVALID_SOCKET), _sendPacketCount(0), _ioRefCount(0), _sendFlag(false), _isDisconnected(false), _isUsed(false)
	{
		ZeroMemory(&_stRecvOverlapped, sizeof(OVERLAPPED));
		ZeroMemory(&_stSendOverlapped, sizeof(OVERLAPPED));

		_recvRingBuffer.ClearBuffer();

	}

	~stLanSESSION()
	{
	}


	void Init()
	{
		_sendPacketCount = 0;

		ZeroMemory(&_stRecvOverlapped, sizeof(OVERLAPPED));
		ZeroMemory(&_stSendOverlapped, sizeof(OVERLAPPED));
		_recvRingBuffer.ClearBuffer();

		// 세션 참조카운트(ioRefCount) 증가
		// accept 단계에서 소켓 얻은 후, 세션 사용을 할 때, 세션이 해제되는 걸 방지하기 위한 참조카운트! 
		// increment가 아니라 exchange하는 이유 
		// -> 무조건 default 참조 카운트를 1로 제한하기 위함

		// * 일어날 수 있는 상황
		// recvpost 함수에 진입하여 refcount 증가 후, WSARecv까지 걸어주고 IO_PENDING 에러가 떴는데,
		// 완료 통지가 오기 전에 이 위치에서 rst or 컨텐츠에서 disconnect되면 refcount가 0이 되어 이 시점에 relese됨
		// 그리고 재할당되어 다른 세션이 되어있을 수 있어 완료 통지가 재할당된 세션으로 올 수 있음
		InterlockedExchange64(&_ioRefCount, 1);

		// flag 셋팅
		InterlockedExchange8((char*)&_sendFlag, false);
		InterlockedExchange8((char*)&_isDisconnected, false);
		InterlockedExchange8((char*)&_isUsed, false);
	}

	void Release()
	{
		InterlockedExchange8((char*)&_isDisconnected, false);
		InterlockedExchange8((char*)&_sendFlag, false);
		InterlockedExchange8((char*)&_isUsed, false);

		_sessionID = -1;

		// 소켓 Invalid 처리하여 더이상 해당 소켓으로 I/O 못받게 함
		_socketClient = INVALID_SOCKET;

		// Send Packet 관련 리소스 정리
		// SendQ에서 Dqeueue하여 SendPacket 배열에 넣었지만 아직 WSASend 못해서 남아있는 패킷 정리
		for (int iSendCount = 0; iSendCount < _sendPacketCount; iSendCount++)
			CPacket::Free(_sendPackets[iSendCount]);

		_sendPacketCount = 0;

		// SendQ에 남아있다는 건 WSABUF에 꽂아넣지도 못한 상태 
		if (_sendQ.GetSize() > 0)
		{
			CPacket* packet = nullptr;

			// PacketPool에 반환
			while (_sendQ.Dequeue(packet))
				CPacket::Free(packet);
		}
	}

public:
	uint64_t _sessionID;										// Session ID
	SOCKET _socketClient;										// Client Socket

	OVERLAPPED _stRecvOverlapped;								// Recv Overlapped I/O Struct
	OVERLAPPED _stSendOverlapped;								// Send Overlapped I/O Struct

	RingBuffer _recvRingBuffer;									// Recv RingBuffer
	LockFreeQueue<CPacket*> _sendQ;								// Send LockFreeQueue

	CPacket* _sendPackets[MAX_WSA_BUF] = { nullptr };			// Send Packets 배열

	alignas(64) int _sendPacketCount;							// WSABUF Count
	alignas(64) __int64 _ioRefCount;							// I/O Count & Session Ref Count
	alignas(64) bool _sendFlag;									// Sending Message Check
	alignas(64) bool _isDisconnected;							// Session Disconnected
	alignas(8) bool _isUsed;									// Session Used

};

#endif // !__SESSION__
