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


bool CRedis::syncSet(const std::string& key, const std::string& value, int timeout)
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

cpp_redis::reply CRedis::syncGet(const std::string& key)
{
	std::future<cpp_redis::reply> get_reply = client.get(key);

	client.sync_commit();

	return get_reply.get();
}

std::future<cpp_redis::reply> CRedis::asyncGet(const std::string& key)
{
	auto promise = std::make_shared<std::promise<cpp_redis::reply>>();
	auto future = promise->get_future();
	
		client.get(key, [promise](const cpp_redis::reply& reply) mutable
			{
				try {
					promise->set_value(reply);
				}
				catch (const std::exception& e) {
					// 예외 처리 코드 추가
					std::cerr << "Exception in promise set_value: " << e.what() << std::endl;
					promise->set_exception(std::current_exception());
				}

			}).commit();

			return future;
}

// Redis TLS Class
CRedis_TLS::CRedis_TLS(std::wstring IP, unsigned short port)
{
	mIP = IP;
	mPort = port;

	// TLS Index 할당 및 예외 처리
	tlsIndex = TlsAlloc();
	if (tlsIndex == TLS_OUT_OF_INDEXES)
	{
		wprintf(L"CRedis_TLS : TLS_OUT_OF_INDEXES\n");
		CRASH();
	}
}

CRedis_TLS::~CRedis_TLS()
{
	CRedis* redisObj;

	// LockFreeStack에 저장된 Redis 객체를 pop하여 해제
	while (redisStack.Pop(&redisObj))
	{
		delete redisObj;
	}

	// TLS Index 해제
	TlsFree(tlsIndex);
}

bool CRedis_TLS::syncSet(const std::string& key, const std::string& value, int timeout)
{
	// TLS에서 redis 객체 가져옴
	CRedis* redisObj = GetCRedisObj();

	// redis 함수 호출
	return redisObj->syncSet(key, value, timeout);
}

cpp_redis::reply CRedis_TLS::syncGet(const std::string& key)
{
	CRedis* redisObj = GetCRedisObj();

	return redisObj->syncGet(key);
}

// Key에 맞는 Value를 Redis에서 얻어옴 (비동기)
std::future<cpp_redis::reply> CRedis_TLS::asyncGet(const std::string& key)
{
	CRedis* redisObj = GetCRedisObj();

	return redisObj->asyncGet(key);
}

CRedis* CRedis_TLS::GetCRedisObj()
{
	CRedis* redis = (CRedis*)TlsGetValue(tlsIndex);

	// redis 객체가 tls에 없을 경우 새로 생성
	if (redis == nullptr)
	{
		redis = new CRedis;

		redisStack.Push(redis);
		TlsSetValue(tlsIndex, redis);

		// redis server에 연결
		redis->Connect(mIP, mPort);
	}

	return redis;
}







