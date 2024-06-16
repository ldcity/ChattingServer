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
	// ���μ��� �ڵ��� �޾Ƽ� �ش� ���μ����� ���� �����͸� ����͸��մϴ�. 
	// �ڵ��� ���� ��� ���� ���� ���� ���μ����� ������� �մϴ�.
	PerformanceMonitor(HANDLE hProcess = INVALID_HANDLE_VALUE);

	// �ý��� �� ���μ����� ���� �����͸� �����մϴ�.
	void UpdateMonitorData(void);

	// ������ IP �ּҿ� �ش��ϴ� ��Ʈ��ũ �������̽��� �ε����� ���տ� �߰��մϴ�.
	// GetAdaptersInfo�� ����Ͽ� �ý����� ��Ʈ��ũ ����� ������ ��������, 
	// IP �ּҿ� ��ġ�ϴ� ��Ʈ��ũ �������̽��� ã�Ƽ� �ε����� �����մϴ�.
	void AddInterface(std::string interfaceIp);

	float GetSystemCpuTotal(void) { return _systemCpuTotal; }		// �ý��� ��ü CPU ������ ��ȯ�մϴ�
	float GetSystemCpuUser(void) { return _systemCpuUser; }			// �ý��� ���� ��� CPU ������ ��ȯ�մϴ�
	float GetPSystemCpuKernel(void) { return _systemCpuKernel; }	// �ý��� Ŀ�� ��� CPU ������ ��ȯ�մϴ�

	float GetProcessCpuTotal(void) { return _processCpuTotal; }		// ������ ���μ����� ��ü CPU ������ ��ȯ�մϴ�.
	float GetProcessCpuUser(void) { return _processCpuUser; }		// ������ ���μ����� ���� ��� CPU ������ ��ȯ�մϴ�
	float GetProcessCpuKernel(void) { return _processCpuKernel; }	// ������ ���μ����� Ŀ�� ��� CPU ������ ��ȯ�մϴ�.

	// ���μ����� ���� ��� �޸� ��뷮�� �ް�����Ʈ(MB) ������ ��ȯ�մϴ�
	float GetProcessUserMemoryByMB() { return (float)_pmc.PrivateUsage / 1000000; }

	// ���μ����� ������¡(non-paged) �޸� ��뷮�� �ް�����Ʈ(MB) ������ ��ȯ�մϴ�.
	float GetProcessNonPagedByMB() { return (float)_pmc.QuotaNonPagedPoolUsage / 1000000; }

	// �ý����� ������¡(non-paged) �޸� ��뷮�� �ް�����Ʈ(MB) ������ ��ȯ�մϴ�
	// ������ ũ��(4096����Ʈ)�� ���Ͽ� ���� �޸� ũ��� ��ȯ�˴ϴ�.
	float GetSystemNonPagedByMB() { return (float)_perfInfo.KernelNonpaged * 4096 / 1000000; }

	// �ý����� ��� ������ �޸� ũ�⸦ �Ⱑ����Ʈ(GB) ������ ��ȯ�մϴ�
	float GetSystemAvailMemoryByGB() { return (float)_perfInfo.PhysicalAvailable * 4096 / 1000000000; }

	// �ý����� ��� ������ �޸� ũ�⸦ �ް�����Ʈ(MB) ������ ��ȯ�մϴ�
	float GetSystemAvailMemoryByMB() { return (float)_perfInfo.PhysicalAvailable * 4096 / 1000000; }

	// ��Ʈ��ũ �������̽��� ���� �۽ŵ� ������ ���� ų�ι���Ʈ(KB) ������ ��ȯ�մϴ�.
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

	// ��Ʈ��ũ �������̽��� ���� ���ŵ� ������ ���� ų�ι���Ʈ(KB) ������ ��ȯ�մϴ�.
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

	//cpu ��뷮
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

	//�޸� ��뷮
	PERFORMANCE_INFORMATION _perfInfo;
	PROCESS_MEMORY_COUNTERS_EX _pmc;

	//��Ʈ��ũ ��뷮
	std::set<int> _interfaceIndexSet;

	double _inDataOct = 0;
	double _outDataOct = 0;
	double  _prevOut = 0;
	double _prevIn = 0;
	int _lastNormalOut = 0;
	int _lastNormalIn = 0;
};