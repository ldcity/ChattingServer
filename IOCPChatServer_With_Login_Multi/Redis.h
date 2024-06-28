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

	// ---------------------------------------------------
	// ����
	// ---------------------------------------------------
	bool syncSet(const std::string& key, const std::string& value, int timeout = 0);
	cpp_redis::reply syncGet(const std::string& key);

	// ---------------------------------------------------
	// �񵿱�
	// ---------------------------------------------------
	void asyncSet(const std::string& key, const std::string& value, int timeout, std::function<void(const cpp_redis::reply&)> callback);
	void asyncGet(const std::string& key, std::function<void(const cpp_redis::reply&)> callback);

private:
	cpp_redis::client client;
};



#endif // !__REDIS_CLASS__
