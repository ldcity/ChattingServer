#include "PCH.h"
#include "Exception.h"

const char* SerializingBufferException::what() const noexcept
{
	// 로그 폴더 경로
	const wchar_t* logFolder = L"Exception";

	// 로그 폴더가 없으면 생성
	_wmkdir(logFolder);

	time_t now;
	struct tm timeinfo;
	time(&now);
	localtime_s(&timeinfo, &now);

	// 로그 파일 제목을 현재 시간으로 생성합니다.
	wchar_t logFileName[50];
	wcsftime(logFileName, sizeof(logFileName), L"Exception_%Y%m%d%H%M%S.txt", &timeinfo); // localTime를 &timeinfo로 수정합니다.

	// 로그 파일 경로
	std::wstring logFilePath = std::wstring(logFolder) + L"/" + logFileName; // wstring으로 수정합니다.

	// 로그 파일을 엽니다.
	std::ofstream logFile(logFilePath, std::ios::app);

	if (logFile.is_open())
	{
		logFile << message << std::endl;
		logFile << "Serialize Method Name : " << GetMethodName() << std::endl;
		logFile << "Line : " << GetLine() << std::endl;
		logFile << "Serialize Buffer Address : " << std::showbase << std::hex << (uintptr_t)buffer << std::dec << std::endl;
		logFile.close();
	}
	else {
		std::cerr << "Failed to open log file." << std::endl;
	}

	return message;
}
