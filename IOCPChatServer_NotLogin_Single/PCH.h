#ifndef __PCH__
#define __PCH__

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winmm.lib")

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <windows.h>

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

#endif // __PCH__
