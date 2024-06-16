#ifndef __RING_BUFFER__
#define __RING_BUFFER__

#include "PCH.h"

#define BUFSIZE 512

class RingBuffer
{
public:
	RingBuffer(void);
	RingBuffer(int iBufferSize);
	~RingBuffer();

	void Resize();							// 2배로 링버퍼 크기 늘리기
	int GetBufferSize(void);

	int GetUseSize(void);					// 현재 사용중인 용량 얻기.
	int GetFreeSize(void);					// 현재 버퍼에 남은 용량 얻기.

	// 버퍼 포인터로 외부에서 한방에 읽고, 쓸 수 있는 길이.
	// 끊기지 않은 길이
	int DirectEnqueueSize(void);
	int DirectDequeueSize(void);

	//	WritePos 에 데이타 넣음.
	// Parameters: (char *)데이타 포인터. (int)크기.
	// Return: (int)넣은 크기.
	int Enqueue(const char* chpData, int iSize);

	// ReadPos 에서 데이타 가져옴. ReadPos 이동.
	// Parameters: (char *)데이타 포인터. (int)크기.
	// Return: (int)가져온 크기.
	int Dequeue(char* chpDest, int iSize);

	// ReadPos 에서 데이타 읽어옴. ReadPos 고정.
	// Parameters: (char *)데이타 포인터. (int)크기.
	// Return: (int)가져온 크기.
	int Peek(char* chpDest, int iSize);

	// 원하는 길이만큼 읽기위치 에서 삭제 / 쓰기 위치 이동
	// Return: (int)이동크기
	int MoveWritePtr(int iSize);
	int MoveReadPtr(int iSize);
	int MoveReadPtr(char* ptr);

	// 버퍼의 모든 데이타 삭제.
	void ClearBuffer(void);

	// 버퍼의 Front 포인터 얻음.
	char* GetReadBufferPtr(void);

	// 버퍼의 RearPos 포인터 얻음.
	char* GetWriteBufferPtr(void);

	// 버퍼의 시작 포인터
	char* GetBufferPtr(void);


private:
	int ringBufferSize;
	char* readPos;
	char* writePos;
	char* begin;
	char* end;
};

#endif