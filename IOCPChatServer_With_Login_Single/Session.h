#pragma once
#ifndef __SESSION__
#define __SESSION__

#include "RingBuffer.h"
#include "LockFreeQueue.h"

#define MAX_WSA_BUF 100
#define RELEASEMASKING 0x80000000				// ioRefCount���� ReleaseFlag�� �̾Ƴ�

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

		// ���� ����ī��Ʈ(ioRefCount) ����
		// accept �ܰ迡�� ���� ���� ��, ���� ����� �� ��, ������ �����Ǵ� �� �����ϱ� ���� ����ī��Ʈ! 
		// increment�� �ƴ϶� exchange�ϴ� ���� 
		// -> ������ default ���� ī��Ʈ�� 1�� �����ϱ� ����

		// * �Ͼ �� �ִ� ��Ȳ
		// recvpost �Լ��� �����Ͽ� refcount ���� ��, WSARecv���� �ɾ��ְ� IO_PENDING ������ ���µ�,
		// �Ϸ� ������ ���� ���� �� ��ġ���� rst or ���������� disconnect�Ǹ� refcount�� 0�� �Ǿ� �� ������ relese��
		// �׸��� ���Ҵ�Ǿ� �ٸ� ������ �Ǿ����� �� �־� �Ϸ� ������ ���Ҵ�� �������� �� �� ����
		InterlockedExchange64(&_ioRefCount, 1);

		// flag ����
		InterlockedExchange8((char*)&_sendFlag, false);
		InterlockedExchange8((char*)&_isDisconnected, false);
		InterlockedExchange8((char*)&_sendDisconnFlag, false);
	}

	void Release()
	{
		InterlockedExchange8((char*)&_isDisconnected, false);
		InterlockedExchange8((char*)&_sendFlag, false);

		_sessionID = -1;

		// ���� Invalid ó���Ͽ� ���̻� �ش� �������� I/O ���ް� ��
		_socketClient = INVALID_SOCKET;

		// Send Packet ���� ���ҽ� ����
		// SendQ���� Dqeueue�Ͽ� SendPacket �迭�� �־����� ���� WSASend ���ؼ� �����ִ� ��Ŷ ����
		for (int iSendCount = 0; iSendCount < _sendPacketCount; iSendCount++)
			CPacket::Free(_sendPackets[iSendCount]);

		_sendPacketCount = 0;

		// SendQ�� �����ִٴ� �� WSABUF�� �ȾƳ����� ���� ���� 
		if (_sendQ.GetSize() > 0)
		{
			CPacket* packet = nullptr;

			// PacketPool�� ��ȯ
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

	CPacket* _sendPackets[MAX_WSA_BUF] = { nullptr };			// Send Packets �迭

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

		// ���� ����ī��Ʈ(ioRefCount) ����
		// accept �ܰ迡�� ���� ���� ��, ���� ����� �� ��, ������ �����Ǵ� �� �����ϱ� ���� ����ī��Ʈ! 
		// increment�� �ƴ϶� exchange�ϴ� ���� 
		// -> ������ default ���� ī��Ʈ�� 1�� �����ϱ� ����

		// * �Ͼ �� �ִ� ��Ȳ
		// recvpost �Լ��� �����Ͽ� refcount ���� ��, WSARecv���� �ɾ��ְ� IO_PENDING ������ ���µ�,
		// �Ϸ� ������ ���� ���� �� ��ġ���� rst or ���������� disconnect�Ǹ� refcount�� 0�� �Ǿ� �� ������ relese��
		// �׸��� ���Ҵ�Ǿ� �ٸ� ������ �Ǿ����� �� �־� �Ϸ� ������ ���Ҵ�� �������� �� �� ����
		InterlockedExchange64(&_ioRefCount, 1);

		// flag ����
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

		// ���� Invalid ó���Ͽ� ���̻� �ش� �������� I/O ���ް� ��
		_socketClient = INVALID_SOCKET;

		// Send Packet ���� ���ҽ� ����
		// SendQ���� Dqeueue�Ͽ� SendPacket �迭�� �־����� ���� WSASend ���ؼ� �����ִ� ��Ŷ ����
		for (int iSendCount = 0; iSendCount < _sendPacketCount; iSendCount++)
			CPacket::Free(_sendPackets[iSendCount]);

		_sendPacketCount = 0;

		// SendQ�� �����ִٴ� �� WSABUF�� �ȾƳ����� ���� ���� 
		if (_sendQ.GetSize() > 0)
		{
			CPacket* packet = nullptr;

			// PacketPool�� ��ȯ
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

	CPacket* _sendPackets[MAX_WSA_BUF] = { nullptr };			// Send Packets �迭

	alignas(64) int _sendPacketCount;							// WSABUF Count
	alignas(64) __int64 _ioRefCount;							// I/O Count & Session Ref Count
	alignas(64) bool _sendFlag;									// Sending Message Check
	alignas(64) bool _isDisconnected;							// Session Disconnected
	alignas(8) bool _isUsed;									// Session Used

};

#endif // !__SESSION__
