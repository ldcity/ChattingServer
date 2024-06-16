#ifndef __PCH__
#define __PCH__

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <windows.h>

#include <cpp_redis/cpp_redis>

#pragma comment(lib, "cpp_redis.lib")
#pragma comment(lib, "tacopie.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "ws2_32.lib")

#include <process.h>

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <iostream>

//#include "NetServer.h"
//#include "LanClient.h"
//#include "MonitoringLanClient.h"

#include "TLSFreeList.h"
#include "LockFreeQueue.h"
#include "LockFreeStack.h"

#include "Define.h"
#include "MonitoringDefine.h"
#include "MonitorProtocol.h"
#include "Exception.h"
#include "SerializingBuffer.h"
#include "RingBuffer.h"
#include "Session.h"
#include "Protocol.h"
#include "Packet.h"
#include "Sector.h"

#include "LOG.h"
#include "Profiling.h"
#include "CrashDump.h"
#include "TextParser.h"
#include "PerformanceMonitor.h"

#include "Redis.h"

#endif // __PCH__
