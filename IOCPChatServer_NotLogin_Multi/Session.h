#pragma once
#ifndef __SESSION__
#define __SESSION__

#include "PCH.h"

#define MAX_WSA_BUF 100
#define RELEASEMASKING 0x80000000				// ioRefCount���� ReleaseFlag�� �̾Ƴ�

const char SESSION_ID_BITS = 47;
const __int64 SESSION_INDEX_MASK = 0x00007FFFFFFFFFFF;

// Session Struct
struct stSESSION
{
	uint64_t sessionID;									// Session ID
	SOCKET m_socketClient;								// Client Socket
	uint32_t IP_num;									// Server IP

	wchar_t IP_str[20];									// String IP

	unsigned short PORT;								// Server PORT

	DWORD Timeout;												// Last Recv Time
	DWORD Timer;												// Timeout Timer

	OVERLAPPED m_stRecvOverlapped;						// Recv Overlapped I/O Struct
	OVERLAPPED m_stSendOverlapped;						// Send Overlapped I/O Struct

	RingBuffer recvRingBuffer;							// Recv RingBuffer
	LockFreeQueue<CPacket*> sendQ;						// Send LockFreeQueue

	CPacket* SendPackets[MAX_WSA_BUF] = { nullptr };	// Send Packets Array

	alignas(64) int sendPacketCount;					// WSABUF Count
	alignas(64) __int64 ioRefCount;						// I/O Count & Session Ref Count
	alignas(64) bool sendFlag;							// Sending Message Check
	alignas(8) bool isDisconnected;						// Session Disconnected
	alignas(64) bool sendDisconnFlag;

	stSESSION()
	{
		sessionID = -1;
		m_socketClient = INVALID_SOCKET;
		ZeroMemory(IP_str, sizeof(IP_str));
		IP_num = 0;
		PORT = 0;
		
		Timeout = 0;
		Timer = 0;

		ZeroMemory(&m_stRecvOverlapped, sizeof(OVERLAPPED));
		ZeroMemory(&m_stSendOverlapped, sizeof(OVERLAPPED));
		recvRingBuffer.ClearBuffer();

		sendPacketCount = 0;
		ioRefCount = 0;			// accept ���� �ٷ� recv �ɾ������ ������ �׻� default�� 1
		sendFlag = false;
		isDisconnected = false;
		sendDisconnFlag = false;
	}

	~stSESSION()
	{
	}
};


struct stLanSESSION
{
	uint64_t sessionID;											// Session ID
	SOCKET m_socketClient;										// Client Socket
	uint32_t IP_num;											// Server IP

	wchar_t IP_str[20];											// String IP

	unsigned short PORT;										// Server PORT

	OVERLAPPED m_stRecvOverlapped;								// Recv Overlapped I/O Struct
	OVERLAPPED m_stSendOverlapped;								// Send Overlapped I/O Struct

	RingBuffer recvRingBuffer;									// Recv RingBuffer
	LockFreeQueue<CPacket*> sendQ;								// Send LockFreeQueue

	CPacket* SendPackets[MAX_WSA_BUF] = { nullptr };			// Send Packets �迭

	alignas(64) int sendPacketCount;							// WSABUF Count
	alignas(64) __int64 ioRefCount;								// I/O Count & Session Ref Count
	alignas(64) bool sendFlag;									// Sending Message Check
	alignas(8) bool isDisconnected;								// Session Disconnected
	alignas(8) bool isUsed;										// Session Used

	stLanSESSION()
	{
		sessionID = -1;
		m_socketClient = INVALID_SOCKET;
		ZeroMemory(IP_str, sizeof(IP_str));
		IP_num = 0;
		PORT = 0;
		//LastRecvTime = 0;

		ZeroMemory(&m_stRecvOverlapped, sizeof(OVERLAPPED));
		ZeroMemory(&m_stSendOverlapped, sizeof(OVERLAPPED));
		recvRingBuffer.ClearBuffer();

		sendPacketCount = 0;
		ioRefCount = 0;			// accept ���� �ٷ� recv �ɾ������ ������ �׻� default�� 1
		sendFlag = false;
		isDisconnected = false;
		isUsed = false;
	}

	~stLanSESSION()
	{
	}
};

#endif // !__SESSION__