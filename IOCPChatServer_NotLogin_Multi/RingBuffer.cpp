#include "PCH.h"

#include "RingBuffer.h"

RingBuffer::RingBuffer() : ringBufferSize(BUFSIZE)
{
	begin = new char[ringBufferSize];
	end = begin + ringBufferSize;
	readPos = writePos = begin;
}

RingBuffer::RingBuffer(int iBufferSize) : ringBufferSize(iBufferSize)
{
	begin = new char[ringBufferSize];
	end = begin + ringBufferSize;
	readPos = writePos = begin;
}

RingBuffer::~RingBuffer()
{
	ClearBuffer();
	if (begin != nullptr)
	{
		delete[] begin;
		begin = nullptr;
	}
}

// 링 버퍼에 데이터 꽉 찼을 때 resize
void RingBuffer::Resize()
{
	// temp 동적 배열 할당 (사용 중인 크기만 임시 버퍼에 copy하면 됨)
	char* temp = new char[ringBufferSize];

	// 데이터 복사
	memcpy_s(temp, ringBufferSize * sizeof(char), begin, ringBufferSize * sizeof(char));

	// 이전 동적 배열 삭제
	delete[] begin;

	begin = new char[ringBufferSize * 2];
	memcpy_s(begin, ringBufferSize * sizeof(char) * 2, temp, ringBufferSize * sizeof(char));

	// temp 동적 배열 삭제
	delete[] temp;
}

int RingBuffer::GetBufferSize(void)
{
	return ringBufferSize - 1;
}

// 현재 사용중인 용량 얻기
int RingBuffer::GetUseSize(void)
{
	if (writePos >= readPos)
		return writePos - readPos;
	else
		return (end - readPos) + (writePos - begin);
}

// 현재 버퍼에 남은 용량 얻기.
int RingBuffer::GetFreeSize(void)
{
	if (writePos >= readPos)
		return (end - writePos - 1) + (readPos - begin);
	else
		return readPos - writePos - 1;

}

// 버퍼 포인터로 외부에서 한방에 읽고, 쓸 수 있는 길이.
// 끊기지 않은 길이
int RingBuffer::DirectEnqueueSize(void)
{
	if (readPos <= writePos)
	{
		if (readPos == begin)
			return end - writePos - 1;
		else
			return end - writePos;
	}
	else
		return readPos - writePos - 1;
}

int RingBuffer::DirectDequeueSize(void)
{
	if (readPos <= writePos)
		return writePos - readPos;
	else
		return end - readPos;
}

//	WritePos 에 데이타 넣음.
	// Parameters: (char *)데이타 포인터. (int)크기.
	// Return: (int)넣은 크기.
int RingBuffer::Enqueue(const char* chpData, int iSize)
{
	if (GetFreeSize() < iSize)
		return 0;

	int directEnqueueSize = DirectEnqueueSize();

	if (iSize <= directEnqueueSize)
	{
		memcpy_s(writePos, iSize, chpData, iSize);
		MoveWritePtr(iSize);

		return iSize;
	}

	// direct enqueue size보다 넣으려는 데이터가 더 크다면
	// 일단 넣을 수 있는만큼 넣고(direct enqueue size) 남은 size만큼 한 번 더 enqueue
	memcpy_s(writePos, directEnqueueSize, chpData, directEnqueueSize);
	MoveWritePtr(directEnqueueSize);

	int remainSize = iSize - directEnqueueSize;
	memcpy_s(writePos, remainSize, chpData + directEnqueueSize, remainSize);
	MoveWritePtr(remainSize);

	return iSize;
}

// ReadPos 에서 데이타 가져옴. ReadPos 이동.
// Parameters: (char *)데이타 포인터. (int)크기.
// Return: (int)가져온 크기.
int RingBuffer::Dequeue(char* chpDest, int iSize)
{
	if (GetUseSize() < iSize)
		return 0;

	int directDequeueSize = DirectDequeueSize();

	if (directDequeueSize >= iSize)
	{
		memcpy_s(chpDest, iSize, readPos, iSize);
		MoveReadPtr(iSize);

		return iSize;
	}

	// direct dequeue size보다 뽑으려는 데이터가 더 크다면
	// 일단 뺄 수 있는만큼 넣고(direct dequeue size) 남은 size만큼 한 번 더 dequeue
	memcpy_s(chpDest, directDequeueSize, readPos, directDequeueSize);
	MoveReadPtr(directDequeueSize);

	int remainSize = iSize - directDequeueSize;
	memcpy_s(chpDest + directDequeueSize, remainSize, readPos, remainSize);
	MoveReadPtr(remainSize);

	return iSize;
}

// ReadPos 에서 데이타 읽어옴. ReadPos 고정.
// Parameters: (char *)데이타 포인터. (int)크기.
// Return: (int)가져온 크기, 실패 : 0
int RingBuffer::Peek(char* chpDest, int iSize)
{
	if (GetUseSize() < iSize) return 0;
	if (DirectDequeueSize() >= iSize)
	{
		memcpy_s(chpDest, iSize, readPos, iSize);
		return iSize;
	}

	// direct dequeue size보다 peek하려는 데이터가 더 크다면 링버퍼에서 두 개로 데이터가 쪼개져 있는 것
	// 일단 뺄 수 있는만큼 빼고(direct dequeue size), 남은 size만큼 한 번 더 peek
	int directDequeueSize = DirectDequeueSize();
	memcpy_s(chpDest, directDequeueSize, readPos, directDequeueSize);

	int remainSize = iSize - directDequeueSize;
	memcpy_s(chpDest + directDequeueSize, remainSize, begin, remainSize);

	return iSize;
}

// 원하는 길이만큼 읽기위치 에서 삭제 / 쓰기 위치 이동
// Return: (int)이동크기
// writePos 위치 이동
int RingBuffer::MoveWritePtr(int iSize)
{
	writePos += iSize;


	if (writePos >= end)
	{
		int overFlow = writePos - end;
		writePos = begin + overFlow;
	}

	return iSize;
}

// readPos 위치 이동
int RingBuffer::MoveReadPtr(int iSize)
{
	readPos += iSize;

	if (readPos >= end)
	{
		int overFlow = readPos - end;
		readPos = begin + overFlow;
	}

	return iSize;
}

// readPos 위치 이동
// Parameters:이동할 위치
int RingBuffer::MoveReadPtr(char* ptr)
{
	if (ptr == nullptr)
	{
		return 0;
	}
	readPos = ptr;

	return 1;
}

// 버퍼의 모든 데이타 삭제.
void RingBuffer::ClearBuffer(void)
{
	readPos = writePos = begin;
}

// 버퍼의 Front 포인터 얻음.
char* RingBuffer::GetReadBufferPtr(void)
{
	return readPos;
}

// 버퍼의 RearPos 포인터 얻음.
char* RingBuffer::GetWriteBufferPtr(void)
{
	return writePos;
}

// 버퍼의 시작 포인터
char* RingBuffer::GetBufferPtr(void)
{
	return begin;
}