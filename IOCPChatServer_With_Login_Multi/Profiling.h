#pragma once
#ifndef __PROFILING__
#define __PROFILING__


//---------------------------------------------------------------------------
// Profiling Program 
//---------------------------------------------------------------------------
#include <Windows.h>
#include <iostream>
#include <locale.h>
#include <list>

#define PROFILE_CHECK
#define PROFILE_NUMMAX 30
#define PROFILE_SAMPLE_MAX 50
#define PROFILE_THREAD_MAX 500

class CSystemLog;

struct PROFILE_DATA
{
	long lFlag;						// 프로파일의 사용 여부. (배열시에만)

	WCHAR szName[64];				// 프로파일 샘플 이름

	LARGE_INTEGER IStartTime;		// 프로파일 샘플 실행 시간
	__int64 iTotalTime;				// 전체 사용시간 카운터 Time. (출력시 호출횟수로 나누어 평균 구함)
	__int64 iMin[2];				// 최소 사용시간 카운터 Time. (초단위로 계산하여 저장 / [0] 가장최소 [1] 다음 최소 [2])
	__int64 iMax[2];				// 최대 사용시간 카운터 Time. (초단위로 계산하여 저장 / [0] 가장최대 [1] 다음최데 [2])

	__int64 iCall;					// 누적 호출 횟수
};

//////////////////////////////////////////////////////////////////////
// 한 쓰레드가 가져야 할 프로파일링 필수 TLS 자료들
//////////////////////////////////////////////////////////////////////
class CProfileThread
{
public:
	DWORD dwThreadID;
	PROFILE_DATA Data[PROFILE_SAMPLE_MAX];

	CProfileThread();
};

class CProfile
{
private:
	CProfile();
	~CProfile();
	DWORD tlsIndex;
	LARGE_INTEGER lFreqency;
	double fMicroFreqeuncy;

	std::list<CProfileThread*> ProfileList;

	CRITICAL_SECTION WriteSection;

public:
	static CProfile* GetInstance()
	{
		static CProfile Profile;
		return &Profile;
	}

	// 쓰레드를  찾아내고, 이름을 찾아낸다
	void ProfileBegin(const WCHAR* _fName);
	void ProfileEnd(const WCHAR* _fName);

	// 모든 스레드의 데이터를 출력한다.
	void ProfileOutText(const WCHAR* szFileName);
	void ProfileReset();

private:
	void ListExistAddCounter(CProfileThread* pThread, const WCHAR* _fName);
};

#ifdef PROFILE_CHECK
#define PRO_BEGIN(x)  CProfile::GetInstance()->ProfileBegin(x)
#define PRO_END(x)  CProfile::GetInstance()->ProfileEnd(x)
#define PRO_TEXT(x)	CProfile::GetInstance()->ProfileOutText(x)
#define PRO_RESET() CProfile::GetInstance()->ProfileReset()
#else
#define PRO_BEGIN(x)
#define PRO_END(x)
#endif

#endif // !__PROFILING__