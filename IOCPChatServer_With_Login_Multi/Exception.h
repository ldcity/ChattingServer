#ifndef __EXCEPTION__
#define __EXCEPTION__

#include <iostream>
#include <exception>
#include <ctime>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <sys/stat.h> // mkdir 함수를 사용하기 위해 필요한 헤더

class SerializingBufferException : public std::exception
{
private:
	const char* message;
	const char* methodName;
	int lineNumber;
	char* buffer;

public:
	SerializingBufferException(const char* msg, const char* _methodName, int line, char* buf)
		: message(msg), methodName(_methodName), lineNumber(line), buffer(buf) { }

	virtual const char* what() const noexcept override;


	int GetLine() const
	{
		return lineNumber;
	}

	const char* GetMethodName() const
	{
		return methodName;
	}

	const char* GetBuffer() const
	{
		return buffer;
	}
};

#endif