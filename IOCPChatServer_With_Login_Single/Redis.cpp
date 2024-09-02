#include "PCH.h"

#include "Redis.h"


// Redis Class
CRedis::CRedis()
{

}

CRedis::~CRedis()
{

}

// redis 연결
void CRedis::Connect(std::wstring IP, unsigned short port)
{
	std::string IPStr(IP.begin(), IP.end());
	client.connect(IPStr, port);
}

// 동기
bool CRedis::SyncSet(const std::string& key, const std::string& value, int timeout)
{
	if (timeout > 0)
	{
		client.setex(key, timeout, value);
		client.sync_commit();
	}
	else
	{
		client.set(key, value);
		client.sync_commit();
	}
	return true;
}

cpp_redis::reply CRedis::SyncGet(const std::string& key)
{
	std::future<cpp_redis::reply> get_reply = client.get(key);

	client.sync_commit();

	return get_reply.get();
}

// 비동기
void CRedis::AsyncSet(const std::string & key, const std::string & value, int timeout, std::function<void(const cpp_redis::reply&)> callback)
{
	if (timeout > 0)
	{
		client.setex(key, timeout, value, [callback](const cpp_redis::reply& reply)
		{
			callback(reply);
		}).commit();
	}
	else
	{
		client.set(key, value, [callback](const cpp_redis::reply& reply) mutable
		{
			callback(reply);
		}).commit();
	}

}

void CRedis::AsyncGet(const std::string& key, std::function<void(const cpp_redis::reply&)> callback)
{
	client.get(key, [callback](const cpp_redis::reply& reply) {
		callback(reply);
	}).commit();
}



