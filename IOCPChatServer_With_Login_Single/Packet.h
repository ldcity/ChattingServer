#ifndef __CHATTING_PACKET__
#define __CHATTING_PACKET__

#include "PCH.h"

// 채팅서버 로그인 응답 패킷
void mpResLogin(CPacket* packet, BYTE status, INT64 accountNo);

// 채팅서버 섹터 이동 결과 패킷
void mpResSectorMove(CPacket* packet, INT64 accountNo, WORD sectorX, WORD sectorY);

// 채팅서버 채팅보내기 응답 패킷
void mpResChatMessage(CPacket* packet, INT64 accountNo, WCHAR* id, WCHAR* nickname, WORD msgLen, WCHAR* msg);

#endif
