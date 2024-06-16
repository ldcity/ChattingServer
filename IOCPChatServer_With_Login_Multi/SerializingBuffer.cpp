#include "PCH.h"

#include "SerializingBuffer.h"

CPacket::CPacket() : m_iDataSize(0), m_iBufferSize(eBUFFER_DEFAULT), m_chpBuffer(nullptr), isEncoded(false), ref_cnt(0)
{
	m_chpBuffer = new char[m_iBufferSize + 1];

	lanHeaderPtr = m_chpBuffer + WAN_HEADER_SIZE - LAN_HEADER_SIZE;
	readPos = writePos = m_chpBuffer + DEFAULT_HEADER_SIZE;
}

CPacket::CPacket(int iBufferSize) : m_iDataSize(0), m_iBufferSize(iBufferSize), m_chpBuffer(nullptr), isEncoded(false), ref_cnt(0)
{
	m_chpBuffer = new char[m_iBufferSize + 1];

	lanHeaderPtr = m_chpBuffer + WAN_HEADER_SIZE - LAN_HEADER_SIZE;
	readPos = writePos = m_chpBuffer + DEFAULT_HEADER_SIZE;
}

CPacket::CPacket(CPacket& clSrcPacket) : m_iDataSize(0), m_iBufferSize(eBUFFER_DEFAULT), m_chpBuffer(nullptr), isEncoded(false)
{
	if (this != &clSrcPacket)
	{
		// 기존 데이터 clear
		Clear();

		int copyBufferSize;

		// source 버퍼 크기가 dest 버퍼 크기보다 클 경우, 복사할 크기는 dest 버퍼 크기
		if (m_iBufferSize < clSrcPacket.m_iBufferSize)
			copyBufferSize = m_iBufferSize;
		else
			copyBufferSize = clSrcPacket.m_iBufferSize;

		memcpy_s(m_chpBuffer, m_iBufferSize, clSrcPacket.GetBufferPtr(), copyBufferSize);

		m_iDataSize = clSrcPacket.m_iDataSize;
		m_iBufferSize = clSrcPacket.m_iBufferSize;

		lanHeaderPtr = clSrcPacket.lanHeaderPtr;

		readPos = clSrcPacket.readPos;
		writePos = clSrcPacket.writePos;
	}
}

CPacket::~CPacket()
{
	Clear();
	if (m_chpBuffer != nullptr)
	{
		delete[] m_chpBuffer;
		m_chpBuffer = nullptr;
	}
}

void CPacket::Resize(const char* methodName, int size)
{
	// 기존 직렬화 버퍼에 남아있던 데이터 임시 버퍼에 복사
	char* temp = new char[m_iDataSize];
	
	memcpy_s(temp, m_iDataSize, m_chpBuffer, m_iDataSize);

	// 기존 직렬화 버퍼 delete
	delete[] m_chpBuffer;

	int oldSize = m_iBufferSize;

	// default 직렬화 버퍼 크기(남아있는 데이터 크기에서 필요로하는 데이터 크기만큼 더하고 2배로 늘림
	m_iBufferSize = (oldSize + size) * 2;

	// 새로운 직렬화 버퍼 할당 & 임시 버퍼에 있던 데이터 복사
	m_chpBuffer = new char[m_iBufferSize];
	memcpy_s(m_chpBuffer, m_iBufferSize, temp, m_iDataSize);

	lanHeaderPtr = m_chpBuffer + WAN_HEADER_SIZE - LAN_HEADER_SIZE;
	readPos = writePos = m_chpBuffer + DEFAULT_HEADER_SIZE;

	// 임시 버퍼 delete
	delete[] temp;
}


CPacket& CPacket::operator = (CPacket& clSrcPacket)
{
	if (this == &clSrcPacket) return *this;

	// 기존 데이터 clear
	Clear();

	int copyBufferSize;

	// source 버퍼 크기가 dest 버퍼 크기보다 클 경우, 복사할 크기는 dest 버퍼 크기
	if (m_iBufferSize < clSrcPacket.m_iBufferSize)
		copyBufferSize = m_iBufferSize;
	else
		copyBufferSize = clSrcPacket.m_iBufferSize;

	memcpy_s(m_chpBuffer, m_iBufferSize, clSrcPacket.GetBufferPtr(), copyBufferSize);

	m_iDataSize = clSrcPacket.m_iDataSize;
	m_iBufferSize = clSrcPacket.m_iBufferSize;

	lanHeaderPtr = m_chpBuffer + WAN_HEADER_SIZE - LAN_HEADER_SIZE;

	readPos = clSrcPacket.readPos;
	writePos = clSrcPacket.writePos;

	return *this;
}

// 인코딩
void CPacket::Encoding()
{
	if (true == InterlockedExchange8((char*)&isEncoded, true))
	{
		return;
	}

	NetHeader netHeader;
	netHeader.code = m_code;
	netHeader.len = m_iDataSize;

	uint64_t checksum = 0;

	// 체크섬 계산
	for (unsigned char i = 0; i < m_iDataSize; i++)
	{
		checksum += *((unsigned char*)readPos + i);
	}

	checksum %= 256;
	netHeader.checkSum = checksum;
	srand(checksum);
	netHeader.randKey = rand();

	memcpy_s(m_chpBuffer, sizeof(NetHeader), &netHeader, sizeof(NetHeader));

	unsigned char randKey = *(unsigned char*)(m_chpBuffer + 3);
	unsigned char* d = (unsigned char*)(m_chpBuffer + 4);
	unsigned char p = 0;
	unsigned char e = 0;
	unsigned char size = m_iDataSize + 1;			// 체크썸 포함 페이로드

	for (unsigned char i = 0; i < size; i++)
	{
		p = *(d + i) ^ (p + randKey + i + 1);
		e = p ^ (e + m_key + i + 1);
		*(d + i) = e;
	}
}

// 디코딩
bool CPacket::Decoding()
{
	unsigned char randKey = *((unsigned char*)m_chpBuffer + 3);
	unsigned char* d = (unsigned char*)m_chpBuffer + 4;
	unsigned char size = *((unsigned short*)(m_chpBuffer + 1)) + 1;			// 체크썸 포함 페이로드

	for (unsigned char i = size - 1; i >= 1; i--)
	{
		*(d + i) ^= (*(d + (i - 1)) + m_key + (i + 1));
	}

	*d ^= (m_key + 1);

	for (unsigned char i = size - 1; i >= 1; i--)
	{
		*(d + i) ^= (*(d + (i - 1)) + randKey + (i + 1));
	}

	*d ^= (randKey + 1);

	uint64_t checksum = 0;

	char* payloadPtr = m_chpBuffer + sizeof(NetHeader);

	// 체크섬 계산
	for (unsigned char i = 0; i < m_iDataSize; i++)
	{
		checksum += *((unsigned char*)payloadPtr + i);
	}

	checksum %= 256;

	// 기존 체크썸과 복호화된 체크썸이 다를 경우
	if (checksum != *d)
		return false;

	return true;
}