#pragma once
#ifndef __REDIS_CLASS__
#define __REDIS_CLASS__

#include "PCH.h"

class CRedis
{
public:
	CRedis();
	~CRedis();

	// redis ����
	void Connect(std::wstring IP, unsigned short port);

	// Key-Value ���·� Redis�� ����
	bool syncSet(const std::string& key, const std::string& value, int timeout = 0);

	// Key�� �´� Value�� Redis���� ����
	cpp_redis::reply syncGet(const std::string& key);

	// Key�� �´� Value�� Redis���� ���� (�񵿱�)
	std::future<cpp_redis::reply> asyncGet(const std::string& key);

private:
	cpp_redis::client client;
};

// Multi-Thread �� Redis
class CRedis_TLS : public CRedis
{
public:
	CRedis_TLS(std::wstring IP, unsigned short port);
	~CRedis_TLS();

	bool syncSet(const std::string& key, const std::string& value, int timeout = 0);
	cpp_redis::reply syncGet(const std::string& key);
	std::future<cpp_redis::reply> asyncGet(const std::string& key);

	CRedis* GetCRedisObj();		// CRedis ��ü�� �����ϰų� ����

private:
	DWORD tlsIndex;

	LockFreeStack<CRedis*> redisStack;		// CRedis ��ü���� ��� �����ϱ� ���� �ʿ�

	std::wstring mIP;
	unsigned short mPort;
};



#endif // !__REDIS_CLASS__
