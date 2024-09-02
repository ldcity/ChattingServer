#pragma once
#ifndef __REDIS_CLASS__
#define __REDIS_CLASS__

#include "PCH.h"

class CRedis
{
public:
	CRedis();
	~CRedis();

	void Connect(std::wstring IP, unsigned short port);

	// 동기
	bool SyncSet(const std::string& key, const std::string& value, int timeout = 0);
	cpp_redis::reply SyncGet(const std::string& key);

	// 비동기
	void AsyncSet(const std::string& key, const std::string& value, int timeout, std::function<void(const cpp_redis::reply&)> callback);
	void AsyncGet(const std::string& key, std::function<void(const cpp_redis::reply&)> callback);

private:
	cpp_redis::client client;
};

#endif