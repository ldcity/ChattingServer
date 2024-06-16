#ifndef __MONITOR_DEFINE__
#define __MONITOR_DEFINE__

#include <limits.h>

#define MONITORARRCNF			20


enum ERRORCODE
{
	EMPTYMAPDATA = 100,			// map에 없는 데이터 find
	LOGINEXIST,					// 로그인 error (이미 존재함)
	TYPEERROR					// type error
};

enum SERVERTYPE
{
	LOGIN_SERVER_TYPE = 0,
	GAME_SERVER_TYPE,
	CHAT_SERVER_TYPE,
	TOTAL
};

// dataType
enum LOGINMONITORTYPE
{
	MONITOR_DATA_TYPE_LOGIN_SERVER_RUN = 0,		// 로그인서버 실행여부 ON / OFF
	MONITOR_DATA_TYPE_LOGIN_SERVER_CPU = 1,		// 로그인서버 CPU 사용률
	MONITOR_DATA_TYPE_LOGIN_SERVER_MEM = 2,		// 로그인서버 메모리 사용 MByte
	MONITOR_DATA_TYPE_LOGIN_SESSION = 3,			// 로그인서버 세션 수 (컨넥션 수)
	MONITOR_DATA_TYPE_LOGIN_AUTH_TPS = 4,			// 로그인서버 인증 처리 초당 횟수
	MONITOR_DATA_TYPE_LOGIN_PACKET_POOL = 5,		// 로그인서버 패킷풀 사용량
};

enum GAMEMONITORTYPE
{

	MONITOR_DATA_TYPE_GAME_SERVER_RUN = 0,		// GameServer 실행 여부 ON / OFF
	MONITOR_DATA_TYPE_GAME_SERVER_CPU = 1,		// GameServer CPU 사용률
	MONITOR_DATA_TYPE_GAME_SERVER_MEM = 2,		// GameServer 메모리 사용 MByte
	MONITOR_DATA_TYPE_GAME_SESSION = 3,			// 게임서버 세션 수 (컨넥션 수)
	MONITOR_DATA_TYPE_GAME_AUTH_PLAYER = 4,		// 게임서버 AUTH MODE 플레이어 수
	MONITOR_DATA_TYPE_GAME_GAME_PLAYER = 5,		// 게임서버 GAME MODE 플레이어 수
	MONITOR_DATA_TYPE_GAME_ACCEPT_TPS = 6,		// 게임서버 Accept 처리 초당 횟수
	MONITOR_DATA_TYPE_GAME_PACKET_RECV_TPS = 7,	// 게임서버 패킷처리 초당 횟수
	MONITOR_DATA_TYPE_GAME_PACKET_SEND_TPS = 8,	// 게임서버 패킷 보내기 초당 완료 횟수
	MONITOR_DATA_TYPE_GAME_DB_WRITE_TPS = 9,		// 게임서버 DB 저장 메시지 초당 처리 횟수
	MONITOR_DATA_TYPE_GAME_DB_WRITE_MSG = 10,		// 게임서버 DB 저장 메시지 큐 개수 (남은 수)
	MONITOR_DATA_TYPE_GAME_AUTH_THREAD_FPS = 11,	// 게임서버 AUTH 스레드 초당 프레임 수 (루프 수)
	MONITOR_DATA_TYPE_GAME_GAME_THREAD_FPS = 12,	// 게임서버 GAME 스레드 초당 프레임 수 (루프 수)
	MONITOR_DATA_TYPE_GAME_PACKET_POOL = 13,		// 게임서버 패킷풀 사용량
};

enum CHATMONITORTYPE
{
	MONITOR_DATA_TYPE_CHAT_SERVER_RUN = 0,		// 채팅서버 ChatServer 실행 여부 ON / OFF
	MONITOR_DATA_TYPE_CHAT_SERVER_CPU = 1,		// 채팅서버 ChatServer CPU 사용률
	MONITOR_DATA_TYPE_CHAT_SERVER_MEM = 2,		// 채팅서버 ChatServer 메모리 사용 MByte
	MONITOR_DATA_TYPE_CHAT_SESSION = 3,			// 채팅서버 세션 수 (컨넥션 수)
	MONITOR_DATA_TYPE_CHAT_PLAYER = 4,			// 채팅서버 인증성공 사용자 수 (실제 접속자)
	MONITOR_DATA_TYPE_CHAT_UPDATE_TPS = 5,		// 채팅서버 UPDATE 스레드 초당 초리 횟수
	MONITOR_DATA_TYPE_CHAT_PACKET_POOL = 6,		// 채팅서버 패킷풀 사용량
	MONITOR_DATA_TYPE_CHAT_UPDATEMSG_POOL = 7,	// 채팅서버 UPDATE MSG 풀 사용량
};

struct ClientInfo
{
	uint64_t sessionID;
	BYTE serverNo;
	wchar_t ip[16];
	unsigned short port;
};

struct MonitoringInfo
{
	BYTE iServerNo;
	int type;
	int iSum;				// avr 계산 용도
	int iCount;				// avr 계산 용도
	int iMin;
	int iMax;

	MonitoringInfo() : iServerNo(-1), type(-1), iSum(0), iCount(0), iMin(INT_MAX), iMax(INT_MIN) {}
};

#endif