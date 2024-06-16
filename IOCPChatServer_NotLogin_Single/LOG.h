#pragma once
#ifndef __LOG __H__
#define __LOG __H__

#include "PCH.h"

#define dfLOG_LEVEL_DEBUG		0
#define dfLOG_LEVEL_ERROR		1
#define dfLOG_LEVEL_SYSTEM		2

// 흔하게 발생하는 에러코드
#define ERROR_10054		10054
#define ERROR_10058		10058
#define ERROR_10060		10060

class Log
{
private:
	FILE* fp;
	int g_iLogLevel;				// 출력 저장 대상의 로그 레벨
	wchar_t g_szLogBuff[1024];		// 로그 저장시 필요한 임시 버퍼
	CRITICAL_SECTION cs;
public:
	Log() {};

	Log(const WCHAR* szFileName)
	{
		fp = nullptr;
		g_iLogLevel = -1;
		ZeroMemory(g_szLogBuff, sizeof(wchar_t));

		time_t now;
		struct tm timeinfo;
		time(&now);
		localtime_s(&timeinfo, &now);

		// 로그 폴더 경로 생성
		wchar_t logFolderPath[MAX_PATH];
		GetCurrentDirectoryW(MAX_PATH, logFolderPath);
		wcscat_s(logFolderPath, L"\\LOG");
	
		// 로그 폴더가 없다면 생성
		if (!CreateDirectoryW(logFolderPath, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
		{
			// 폴더 생성에 실패하면 에러 처리
			wprintf(L"Failed to create LOG folder. Error code: %d\n", GetLastError());
			return;
		}

		// 로그 파일 제목을 현재 시간 + szFileName으로 생성
		wchar_t logFileName[MAX_PATH];
		wcsftime(logFileName, sizeof(logFileName) / sizeof(wchar_t), L"%Y%m%d%H%M%S_", &timeinfo);
		wcscat_s(logFileName, sizeof(logFileName) / sizeof(wchar_t), szFileName);
		wcscat_s(logFileName, sizeof(logFileName) / sizeof(wchar_t), L".txt");
	
		// 로그 파일 전체 경로 생성
		wchar_t fullPath[MAX_PATH];
		wcscpy_s(fullPath, logFolderPath);
		wcscat_s(fullPath, L"\\");
		wcscat_s(fullPath, logFileName);

		_wfopen_s(&fp, fullPath, L"w");
		if (fp == nullptr)
		{
			// 파일 생성에 실패하면 에러 처리
			wprintf(L"Failed to open LOG File. Error code: %d\n", GetLastError());
			return;
		}

		InitializeCriticalSection(&cs);
	}

	void logger(int iLogLevel, int lines, const wchar_t* format, ...)
	{
		EnterCriticalSection(&cs);

		if (g_iLogLevel <= iLogLevel)
		{

			// 현재 시간을 가져옵니다.
			SYSTEMTIME st;
			GetLocalTime(&st);

			wchar_t buffer[32];
			swprintf_s(buffer, L"%02d%02d%02d_%02d%02d%02d", st.wYear % 100, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

			

			// 로그 메시지 작성
			va_list args;
			va_start(args, format);
			vswprintf_s(g_szLogBuff, format, args);
			va_end(args);

			if (iLogLevel == dfLOG_LEVEL_DEBUG)
				fwprintf_s(fp, L"%s [DEBUG : %d] : %s\n", buffer, lines, g_szLogBuff);
			else if (iLogLevel == dfLOG_LEVEL_ERROR)
				fwprintf_s(fp, L"%s [ERROR : %d] : %s\n", buffer, lines, g_szLogBuff);
			else if (iLogLevel == dfLOG_LEVEL_SYSTEM)
				fwprintf_s(fp, L"%s [SYSTEM : %d] : %s\n", buffer, lines, g_szLogBuff);

			fflush(fp);
		}
		LeaveCriticalSection(&cs);
	}

	~Log()
	{
		if (fp != nullptr)
			fclose(fp);
	
		DeleteCriticalSection(&cs);
	}
};

#endif