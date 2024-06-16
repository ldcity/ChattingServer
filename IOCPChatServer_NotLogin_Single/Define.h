#ifndef __SERVER_DEFINE__
#define __SERVER_DEFINE__

#include "PCH.h"

#define ID_MAX_LEN			20
#define NICKNAME_MAX_LEN	20
#define MSG_MAX_LEN			64

#define MAX_PACKET_LEN		348

#define PACKET_CODE			0x56
#define KEY					0xa9

#pragma pack(1)
// LAN Header
struct LANHeader
{
	// Payload Len
	unsigned short len;
};

// Net Header
struct NetHeader
{
	unsigned char code;		// 고유 키 (공격 방지)
	short len;				// 페이로드 길이
	unsigned char randKey;	// chacksum 생성을 위한 랜덤 키
	unsigned char checkSum;	// 체크섬
};

#pragma pack()

struct Player
{
	ULONG64 sessionID;								// 세션 ID
	INT64 accountNo;								// 회원 번호
	char sessionKey[64];							// 인증토큰
	wchar_t ID[ID_MAX_LEN];							// ID
	wchar_t nickname[NICKNAME_MAX_LEN];				// 닉네임

	short sectorX;
	short sectorY;
	
	DWORD recvLastTime;						// 하트비트 시간

	Player() : sessionID(-1), accountNo(-1), sessionKey{ 0 }, ID{ 0 }, nickname{ 0 }, 
		sectorX(-1), sectorY(-1), recvLastTime(0) {}
	Player(ULONG64 _sessionID, INT64 _accountNo, const wchar_t* _ID, const wchar_t* _nickname,
		short _sectorX, short _sectorY) :
		sessionID(_sessionID), accountNo(_accountNo), sectorX(_sectorX), sectorY(_sectorY), sessionKey{ 0 }, recvLastTime(0)
	{
		wmemcpy_s(ID, ID_MAX_LEN, _ID, ID_MAX_LEN);
		wmemcpy_s(nickname, NICKNAME_MAX_LEN, _nickname, NICKNAME_MAX_LEN);
	}

};


#endif // !__SERVER_DEFINE__
