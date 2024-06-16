#pragma once
#pragma comment(lib, "IPHLPAPI.lib")

#include "PCH.h"
#include <psapi.h>
#include <set>
#include <string>
#include <iphlpapi.h>
#include <stdlib.h>

using namespace std;

class PerformanceMonitor
{
public:
	// 프로세스 핸들을 받아서 해당 프로세스의 성능 데이터를 모니터링합니다. 
	// 핸들이 없는 경우 현재 실행 중인 프로세스를 대상으로 합니다.
	PerformanceMonitor(HANDLE hProcess = INVALID_HANDLE_VALUE);

	// 시스템 및 프로세스의 성능 데이터를 갱신합니다.
	void UpdateMonitorData(void);

	// 지정된 IP 주소에 해당하는 네트워크 인터페이스의 인덱스를 집합에 추가합니다.
	// GetAdaptersInfo를 사용하여 시스템의 네트워크 어댑터 정보를 가져오고, 
	// IP 주소와 일치하는 네트워크 인터페이스를 찾아서 인덱스를 저장합니다.
	void AddInterface(std::string interfaceIp);

	float GetSystemCpuTotal(void) { return _systemCpuTotal; }		// 시스템 전체 CPU 사용률을 반환합니다
	float GetSystemCpuUser(void) { return _systemCpuUser; }			// 시스템 유저 모드 CPU 사용률을 반환합니다
	float GetPSystemCpuKernel(void) { return _systemCpuKernel; }	// 시스템 커널 모드 CPU 사용률을 반환합니다

	float GetProcessCpuTotal(void) { return _processCpuTotal; }		// 지정된 프로세스의 전체 CPU 사용률을 반환합니다.
	float GetProcessCpuUser(void) { return _processCpuUser; }		// 지정된 프로세스의 유저 모드 CPU 사용률을 반환합니다
	float GetProcessCpuKernel(void) { return _processCpuKernel; }	// 지정된 프로세스의 커널 모드 CPU 사용률을 반환합니다.

	// 프로세스의 유저 모드 메모리 사용량을 메가바이트(MB) 단위로 반환합니다
	float GetProcessUserMemoryByMB() { return (float)_pmc.PrivateUsage / 1000000; }

	// 프로세스의 논페이징(non-paged) 메모리 사용량을 메가바이트(MB) 단위로 반환합니다.
	float GetProcessNonPagedByMB() { return (float)_pmc.QuotaNonPagedPoolUsage / 1000000; }

	// 시스템의 논페이징(non-paged) 메모리 사용량을 메가바이트(MB) 단위로 반환합니다
	// 페이지 크기(4096바이트)로 곱하여 실제 메모리 크기로 변환됩니다.
	float GetSystemNonPagedByMB() { return (float)_perfInfo.KernelNonpaged * 4096 / 1000000; }

	// 시스템의 사용 가능한 메모리 크기를 기가바이트(GB) 단위로 반환합니다
	float GetSystemAvailMemoryByGB() { return (float)_perfInfo.PhysicalAvailable * 4096 / 1000000000; }

	// 시스템의 사용 가능한 메모리 크기를 메가바이트(MB) 단위로 반환합니다
	float GetSystemAvailMemoryByMB() { return (float)_perfInfo.PhysicalAvailable * 4096 / 1000000; }

	// 네트워크 인터페이스를 통해 송신된 데이터 양을 킬로바이트(KB) 단위로 반환합니다.
	int GetOutDataSizeByKB()
	{
		int ret = _outDataOct - _prevOut;
		if (ret >= 0)
		{
			_lastNormalOut = ret;
			return ret;
		}
		return _lastNormalOut;
	}

	// 네트워크 인터페이스를 통해 수신된 데이터 양을 킬로바이트(KB) 단위로 반환합니다.
	int GetInDataSizeByKB()
	{
		int ret = _inDataOct - _prevIn;
		if (ret >= 0)
		{
			_lastNormalIn = ret;
			return ret;
		}
		return _lastNormalIn;
	}

	void PrintMonitorData();
private:
	HANDLE _hProcess;

	//cpu 사용량
	int _iNumberOfProcessors;

	float    _systemCpuTotal;
	float    _systemCpuUser;
	float    _systemCpuKernel;

	float _processCpuTotal;
	float _processCpuUser;
	float _processCpuKernel;

	ULARGE_INTEGER _systemLastTimeKernel;
	ULARGE_INTEGER _systemLastTimeUser;
	ULARGE_INTEGER _systemLastTimeIdle;

	ULARGE_INTEGER _processLastTimeKernel;
	ULARGE_INTEGER _processLastTimeUser;
	ULARGE_INTEGER _processLastTimeTotal;

	//메모리 사용량
	PERFORMANCE_INFORMATION _perfInfo;
	PROCESS_MEMORY_COUNTERS_EX _pmc;

	//네트워크 사용량
	std::set<int> _interfaceIndexSet;

	double _inDataOct = 0;
	double _outDataOct = 0;
	double  _prevOut = 0;
	double _prevIn = 0;
	int _lastNormalOut = 0;
	int _lastNormalIn = 0;
};