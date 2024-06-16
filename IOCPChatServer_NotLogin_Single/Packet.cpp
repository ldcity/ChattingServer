#include "PCH.h"
#include "Packet.h"

// 채팅서버 로그인 응답 패킷
void mpResLogin(CPacket* packet, BYTE status, INT64 accountNo)
{
	WORD type = en_PACKET_CS_CHAT_RES_LOGIN;
	*packet << type << status << accountNo;
}

// 채팅서버 섹터 이동 결과 패킷
void mpResSectorMove(CPacket* packet, INT64 accountNo, WORD sectorX, WORD sectorY)
{
	WORD type = en_PACKET_CS_CHAT_RES_SECTOR_MOVE;
	*packet << type << accountNo << sectorX << sectorY;
}

// 채팅서버 채팅보내기 응답 패킷
void mpResChatMessage(CPacket* packet, INT64 accountNo, WCHAR* id, WCHAR* nickname, WORD msgLen, WCHAR* msg)
{
	WORD type = en_PACKET_CS_CHAT_RES_MESSAGE;
	*packet << type << accountNo;
	packet->PutData((char*)id, ID_MAX_LEN * sizeof(wchar_t));
	packet->PutData((char*)nickname, NICKNAME_MAX_LEN * sizeof(wchar_t));
	*packet << msgLen;
	packet->PutData((char*)msg, msgLen);
}