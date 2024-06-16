#include "PCH.h"

#include "Redis.h"


// Redis Class
CRedis::CRedis()
{

}

CRedis::~CRedis()
{

}

// redis ����
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
					// ���� ó�� �ڵ� �߰�
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

	// TLS Index �Ҵ� �� ���� ó��
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

	// LockFreeStack�� ����� Redis ��ü�� pop�Ͽ� ����
	while (redisStack.Pop(&redisObj))
	{
		delete redisObj;
	}

	// TLS Index ����
	TlsFree(tlsIndex);
}

bool CRedis_TLS::syncSet(const std::string& key, const std::string& value, int timeout)
{
	// TLS���� redis ��ü ������
	CRedis* redisObj = GetCRedisObj();

	// redis �Լ� ȣ��
	return redisObj->syncSet(key, value, timeout);
}

cpp_redis::reply CRedis_TLS::syncGet(const std::string& key)
{
	CRedis* redisObj = GetCRedisObj();

	return redisObj->syncGet(key);
}

// Key�� �´� Value�� Redis���� ���� (�񵿱�)
std::future<cpp_redis::reply> CRedis_TLS::asyncGet(const std::string& key)
{
	CRedis* redisObj = GetCRedisObj();

	return redisObj->asyncGet(key);
}

CRedis* CRedis_TLS::GetCRedisObj()
{
	CRedis* redis = (CRedis*)TlsGetValue(tlsIndex);

	// redis ��ü�� tls�� ���� ��� ���� ����
	if (redis == nullptr)
	{
		redis = new CRedis;

		redisStack.Push(redis);
		TlsSetValue(tlsIndex, redis);

		// redis server�� ����
		redis->Connect(mIP, mPort);
	}

	return redis;
}







