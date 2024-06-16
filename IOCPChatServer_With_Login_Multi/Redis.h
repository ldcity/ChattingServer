#pragma once
#ifndef __REDIS_CLASS__
#define __REDIS_CLASS__

#include "PCH.h"

class CRedis
{
public:
	CRedis();
	~CRedis();

	// redis 연결
	void Connect(std::wstring IP, unsigned short port);

	// Key-Value 형태로 Redis에 저장
	bool syncSet(const std::string& key, const std::string& value, int timeout = 0);

	// Key에 맞는 Value를 Redis에서 얻어옴
	cpp_redis::reply syncGet(const std::string& key);

	// Key에 맞는 Value를 Redis에서 얻어옴 (비동기)
	std::future<cpp_redis::reply> asyncGet(const std::string& key);

private:
	cpp_redis::client client;
};

// Multi-Thread 용 Redis
class CRedis_TLS : public CRedis
{
public:
	CRedis_TLS(std::wstring IP, unsigned short port);
	~CRedis_TLS();

	bool syncSet(const std::string& key, const std::string& value, int timeout = 0);
	cpp_redis::reply syncGet(const std::string& key);
	std::future<cpp_redis::reply> asyncGet(const std::string& key);

	CRedis* GetCRedisObj();		// CRedis 객체를 생성하거나 얻어옴

private:
	DWORD tlsIndex;

	LockFreeStack<CRedis*> redisStack;		// CRedis 객체들을 모두 해제하기 위해 필요

	std::wstring mIP;
	unsigned short mPort;
};



#endif // !__REDIS_CLASS__
