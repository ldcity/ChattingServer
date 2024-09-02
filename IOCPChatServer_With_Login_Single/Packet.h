#ifndef __CHATTING_PACKET__
#define __CHATTING_PACKET__

#include "PCH.h"

// ä�ü��� �α��� ���� ��Ŷ
void MPResLogin(CPacket* packet, BYTE status, INT64 accountNo);

// ä�ü��� ���� �̵� ��� ��Ŷ
void MPResSectorMove(CPacket* packet, INT64 accountNo, WORD sectorX, WORD sectorY);

// ä�ü��� ä�ú����� ���� ��Ŷ
void MPResChatMessage(CPacket* packet, INT64 accountNo, WCHAR* id, WCHAR* nickname, WORD msgLen, WCHAR* msg);

#endif
