#ifndef __NET_CHATSERVER_CLASS__
#define __NET_CHATSERVER_CLASS__

#include "PCH.h"
#include "NetServer.h"
#include "MonitoringLanClient.h"
#include "RedisJobThread.h"

class ChatServer : public NetServer
{
public:
	//--------------------------------------------------------------------------------------
	// Job Info
	//--------------------------------------------------------------------------------------
	enum JobType
	{
		NEW_CONNECT,	// �� ����
		DISCONNECT,		// ���� ����
		MSG_PACKET,		// ���ŵ� ��Ŷ ó��
		LOGIN_RES,		// �α��� ���� ��Ŷ ó��
		TIMEOUT			// Ÿ�Ӿƿ�
	};

	enum ErrorCode
	{
		REDISSETERROR,
	};

	// Job ����ü
	struct ChatJob
	{
		// Session ���� ID
		uint64_t sessionID;

		// Job Type (�� ����, ��Ŷ �޽���, ���� ���� ��)
		WORD type;

		// ��Ŷ ������
		CPacket* packet;
	};

public:
	void SendJob(uint64_t sessionID, WORD type, CPacket* packet);

public:
	ChatServer();
	~ChatServer();

	bool ChatServer_Start();
	bool ChatServer_Stop();

	bool OnConnectionRequest(const wchar_t* IP, unsigned short PORT) override;
	void OnClientJoin(uint64_t sessionID) override;
	void OnClientLeave(uint64_t sessionID) override;
	void OnRecv(uint64_t sessionID, CPacket* packet) override;
	void OnTimeout(uint64_t sessionID) override;
	void OnError(int errorCode, const wchar_t* msg) override;

	//--------------------------------------------------------------------------------------
	// player ���� �Լ�
	//--------------------------------------------------------------------------------------
	Player* FindPlayer(uint64_t sessionID)							// player �˻�
	{
		Player* player = nullptr;

		auto iter = _mapPlayer.find(sessionID);

		if (iter == _mapPlayer.end())
		{
			return nullptr;
		}

		return iter->second;
	}

	bool CreatePlayer(uint64_t sessionID);									// player ����
	bool DeletePlayer(uint64_t sessionID);									// player ����

	// player �ߺ� üũ
	bool CheckPlayer(uint64_t sessionID, INT64 accountNo)
	{
		// accountNo �ߺ�üũ
		auto accountIter = _accountNo.find(accountNo);
		if (accountIter != _accountNo.end())
		{
			Player* dupPlayer = FindPlayer(accountIter->second);

			if (dupPlayer == nullptr)
			{
				return false;
			}

			DisconnectSession(dupPlayer->sessionID);
		}

		_accountNo.insert({ accountNo, sessionID });

		return true;
	}

	//--------------------------------------------------------------------------------------
	// Packet Proc
	//--------------------------------------------------------------------------------------
	bool Packet_Proc(uint64_t sessionID, CPacket* packet);
	void NetPacketProc_Login(uint64_t sessionID, CPacket* packet);			// �α��� ��û
	void NetPacketProc_ResLogin(uint64_t sessionID, CPacket* packet);		// �α��� ����
	void NetPacketProc_SectorMove(uint64_t sessionID, CPacket* packet);		// ���� �̵� ��û
	void NetPacketProc_Chatting(uint64_t sessionID, CPacket* packet);		// ä�� ������
	void NetPacketProc_HeartBeat(uint64_t sessionID, CPacket* packet);		// ��Ʈ��Ʈ

	friend unsigned __stdcall JobWorkerThread(PVOID param);					// Job �� ó�� ������
	friend unsigned __stdcall MoniteringThread(void* param);

	bool JobWorkerThread_Serv();
	bool MoniterThread_Serv();

private:
	RedisWorkerThread* redisWorkerThread;

private:
	Log* _chatLog;

	int _userMAXCnt;										
	int _timeout;										// Ÿ�Ӿƿ� �ð�

	HANDLE _jobHandle;
	HANDLE _jobEvent;

	HANDLE _moniteringThread;							// Monitering Thread

	HANDLE _moniterEvent;								// Monitering Event
	HANDLE _runEvent;									// Thread Start Event

	TLSObjectPool<Player> _playerPool = TLSObjectPool<Player>(190);

	TLSObjectPool<ChatJob> _jobPool = TLSObjectPool<ChatJob>(290);
	LockFreeQueue<ChatJob*> _chatJobQ = LockFreeQueue<ChatJob*>(15000);

	std::unordered_map<uint64_t, Player*> _mapPlayer;							// ��ü Player ��ü
	std::unordered_set<Player*> _sector[dfSECTOR_Y_MAX][dfSECTOR_X_MAX];		// �� ���Ϳ� �����ϴ� Player ��ü
	std::unordered_map<int64_t, uint64_t> _accountNo;

// ����͸� ���� ������
private:
	__int64 _totalPlayerCnt;												
	__int64 _loginPlayerCnt;
	__int64 _loginCount;
	__int64 _loginTPS;
	__int64 _jobUpdatecnt;												
	__int64 _jobUpdateTPS;			
	__int64 _loginPacketTPS;
	__int64 _sectorMovePacketTPS;
	__int64 _chattingReqTPS;
	__int64 _chattingResTPS;
	__int64 _deletePlayerCnt;
	__int64 _deletePlayerTPS;
	__int64 _redisJobThreadUpdateTPS;
	__int64 _jobThreadUpdateCnt;
	__int64 _loginResJobUpdateTPS;

private:
	PerformanceMonitor _performMoniter;
	MonitoringLanClient _lanClient;

	std::wstring _tempIp;
	std::string _ip;

private:
	bool _startFlag;
};


#endif