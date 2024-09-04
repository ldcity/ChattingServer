#include "PCH.h"

#include "SerializingBuffer.h"

CPacket::CPacket() : _iDataSize(0), _iBufferSize(eBUFFER_DEFAULT), _chpBuffer(nullptr), isEncoded(false), ref_cnt(0)
{
	_chpBuffer = new char[_iBufferSize + 1];

	lanHeaderPtr = _chpBuffer + WAN_HEADER_SIZE - LAN_HEADER_SIZE;
	readPos = writePos = _chpBuffer + DEFAULT_HEADER_SIZE;
}

CPacket::CPacket(int iBufferSize) : _iDataSize(0), _iBufferSize(iBufferSize), _chpBuffer(nullptr), isEncoded(false), ref_cnt(0)
{
	_chpBuffer = new char[_iBufferSize + 1];

	lanHeaderPtr = _chpBuffer + WAN_HEADER_SIZE - LAN_HEADER_SIZE;
	readPos = writePos = _chpBuffer + DEFAULT_HEADER_SIZE;
}

CPacket::CPacket(CPacket& clSrcPacket) : _iDataSize(0), _iBufferSize(eBUFFER_DEFAULT), _chpBuffer(nullptr), isEncoded(false)
{
	if (this != &clSrcPacket)
	{
		// ���� ������ clear
		Clear();

		int copyBufferSize;

		// source ���� ũ�Ⱑ dest ���� ũ�⺸�� Ŭ ���, ������ ũ��� dest ���� ũ��
		if (_iBufferSize < clSrcPacket._iBufferSize)
			copyBufferSize = _iBufferSize;
		else
			copyBufferSize = clSrcPacket._iBufferSize;

		memcpy_s(_chpBuffer, _iBufferSize, clSrcPacket.GetBufferPtr(), copyBufferSize);

		_iDataSize = clSrcPacket._iDataSize;
		_iBufferSize = clSrcPacket._iBufferSize;

		lanHeaderPtr = clSrcPacket.lanHeaderPtr;

		readPos = clSrcPacket.readPos;
		writePos = clSrcPacket.writePos;
	}
}

CPacket::~CPacket()
{
	Clear();
	if (_chpBuffer != nullptr)
	{
		delete[] _chpBuffer;
		_chpBuffer = nullptr;
	}
}

void CPacket::Resize(const char* methodName, int size)
{
	// ���� ����ȭ ���ۿ� �����ִ� ������ �ӽ� ���ۿ� ����
	char* temp = new char[_iDataSize];
	
	memcpy_s(temp, _iDataSize, _chpBuffer, _iDataSize);

	// ���� ����ȭ ���� delete
	delete[] _chpBuffer;

	int oldSize = _iBufferSize;

	// default ����ȭ ���� ũ��(�����ִ� ������ ũ�⿡�� �ʿ���ϴ� ������ ũ�⸸ŭ ���ϰ� 2��� �ø�
	_iBufferSize = (oldSize + size) * 2;

	// ���ο� ����ȭ ���� �Ҵ� & �ӽ� ���ۿ� �ִ� ������ ����
	_chpBuffer = new char[_iBufferSize];
	memcpy_s(_chpBuffer, _iBufferSize, temp, _iDataSize);

	lanHeaderPtr = _chpBuffer + WAN_HEADER_SIZE - LAN_HEADER_SIZE;
	readPos = writePos = _chpBuffer + DEFAULT_HEADER_SIZE;

	// �ӽ� ���� delete
	delete[] temp;
}


CPacket& CPacket::operator = (CPacket& clSrcPacket)
{
	if (this == &clSrcPacket) return *this;

	// ���� ������ clear
	Clear();

	int copyBufferSize;

	// source ���� ũ�Ⱑ dest ���� ũ�⺸�� Ŭ ���, ������ ũ��� dest ���� ũ��
	if (_iBufferSize < clSrcPacket._iBufferSize)
		copyBufferSize = _iBufferSize;
	else
		copyBufferSize = clSrcPacket._iBufferSize;

	memcpy_s(_chpBuffer, _iBufferSize, clSrcPacket.GetBufferPtr(), copyBufferSize);

	_iDataSize = clSrcPacket._iDataSize;
	_iBufferSize = clSrcPacket._iBufferSize;

	lanHeaderPtr = _chpBuffer + WAN_HEADER_SIZE - LAN_HEADER_SIZE;

	readPos = clSrcPacket.readPos;
	writePos = clSrcPacket.writePos;

	return *this;
}

// ���ڵ�
void CPacket::Encoding()
{
	if (true == InterlockedExchange8((char*)&isEncoded, true))
	{
		return;
	}

	NetHeader netHeader;
	netHeader.code = _code;
	netHeader.len = _iDataSize;

	uint64_t checksum = 0;

	// üũ�� ���
	for (unsigned char i = 0; i < _iDataSize; i++)
	{
		checksum += *((unsigned char*)readPos + i);
	}

	checksum %= 256;
	netHeader.checkSum = checksum;
	srand(checksum);
	netHeader.randKey = rand();

	memcpy_s(_chpBuffer, sizeof(NetHeader), &netHeader, sizeof(NetHeader));

	unsigned char randKey = *(unsigned char*)(_chpBuffer + 3);
	unsigned char* d = (unsigned char*)(_chpBuffer + 4);
	unsigned char p = 0;
	unsigned char e = 0;
	unsigned char size = _iDataSize + 1;			// üũ�� ���� ���̷ε�

	for (unsigned char i = 0; i < size; i++)
	{
		p = *(d + i) ^ (p + randKey + i + 1);
		e = p ^ (e + _key + i + 1);
		*(d + i) = e;
	}
}

// ���ڵ�
bool CPacket::Decoding()
{
	unsigned char randKey = *((unsigned char*)_chpBuffer + 3);
	unsigned char* d = (unsigned char*)_chpBuffer + 4;
	unsigned char size = *((unsigned short*)(_chpBuffer + 1)) + 1;			// üũ�� ���� ���̷ε�

	for (unsigned char i = size - 1; i >= 1; i--)
	{
		*(d + i) ^= (*(d + (i - 1)) + _key + (i + 1));
	}

	*d ^= (_key + 1);

	for (unsigned char i = size - 1; i >= 1; i--)
	{
		*(d + i) ^= (*(d + (i - 1)) + randKey + (i + 1));
	}

	*d ^= (randKey + 1);

	uint64_t checksum = 0;

	char* payloadPtr = _chpBuffer + sizeof(NetHeader);

	// üũ�� ���
	for (unsigned char i = 0; i < _iDataSize; i++)
	{
		checksum += *((unsigned char*)payloadPtr + i);
	}

	checksum %= 256;

	// ���� üũ��� ��ȣȭ�� üũ���� �ٸ� ���
	if (checksum != *d)
		return false;

	return true;
}