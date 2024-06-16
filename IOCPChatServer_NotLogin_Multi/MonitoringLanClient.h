#ifndef __LAN_MONITORINGCLIENT_CLASS__
#define __LAN_MONITORINGCLIENT_CLASS__

#include "LanClient.h"

class MonitoringLanClient : public LanClient
{
public:
	MonitoringLanClient();
	~MonitoringLanClient();

	bool MonitoringLanClientStart();
	bool MonitoringLanClientStop();

	bool ConnectThread_serv();

	//--------------------------------------------------------------------------------------
	// Make Packet
	//--------------------------------------------------------------------------------------
	void mpLoginToMonitorServer(BYTE serverNo, CPacket* packet);
	void mpUpdateDataToMonitorServer(BYTE serverNo, BYTE dataType, int dataValue, int timeStamp, CPacket* packet);

	void SendDataToMonitorServer(BYTE serverNo, BYTE dataType, int dataValue, int timeStamp);


	//--------------------------------------------------------------------------------------
	// Contents Logic
	//--------------------------------------------------------------------------------------
	void OnClientJoin();
	void OnClientLeave();
	void OnRecv(CPacket* packet);
	void OnError(int errorCode, const wchar_t* msg);

private:
	friend unsigned __stdcall ConnectThread(LPVOID param);

	HANDLE connectThread;
};

#endif