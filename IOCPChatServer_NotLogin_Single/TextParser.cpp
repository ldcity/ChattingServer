#pragma warning(disable :4996)

#include "PCH.h"

#include <wchar.h>
#include <stdlib.h>
#include <stack>

#include "TextParser.h"

TextParser::TextParser() : buffer(NULL), parsingTxt(NULL), parsingStart(NULL), blocksTxt(NULL), _size(0) {}


// 탭, 개행, 공백, 주석 처리
bool TextParser::SkipNoneCommand()
{
	wchar_t* initBuffer = buffer;					// 파일에서 읽어온 버퍼
	blocksStart = blocksTxt;
	parsingStart = parsingTxt;						// 최종 파싱된 데이터


	// 버퍼 끝에 도달할 때까지 반복
	while (*buffer != L'\0')
	{
		// 공백, 탭 제거 (엔터는 단어 구분을 위해 필요)
		if (*buffer == L' ' || *buffer == L'\t')
			buffer++;

		// // 주석 제거
		else if (*buffer == L'/' && *(buffer + 1) == L'/')
		{
			// 엔터 나올때까지 반복 (// 주석은 한줄만 처리하기 때문)
			while (*buffer != L'\n' && *buffer != L'\r')
				buffer++;
		}

		// /* 주석 제거
		else if (*buffer == L'/' && *(buffer + 1) == L'*')
		{
			buffer += 2;
			// */ 주석 나올때까지 반복
			while (*buffer != L'*' || *(buffer + 1) != L'/')
				buffer++;

			buffer += 2;
		}
		else
		{
			*parsingTxt = *buffer;
			*blocksTxt = *buffer;

			buffer++;
			parsingTxt++;
			blocksTxt++;
		}
	}
	*parsingTxt = L'\0';
	*blocksTxt = L'\0';

	// 메모리 반환(free)를 위해 초기 포인터 위치로 이동
	buffer = initBuffer;
	parsingTxt = parsingStart;
	blocksTxt = blocksStart;

	return true;
}

void TextParser::split(const wchar_t* szName, wchar_t** tokens, int tokensLen, int keyCnt, const wchar_t delimiter)
{
	int i = 0, j = 0;
	wchar_t temp[128];
	int szNameSize = wcslen(szName);

	wmemcpy_s(temp, sizeof(temp) / sizeof(wchar_t), szName, szNameSize);

	temp[szNameSize] = L'\0';

	wchar_t* start = temp;
	int size = 0;

	while (temp[i])
	{

		if (temp[i] == delimiter)
		{
			temp[i] = L'\0';

			size = wcslen(start) + 1;

			wmemcpy_s(tokens[j], size, start, size);
			start = &temp[i + 1];
			j++;
		}
		i++;
	}

	size = wcslen(start) + 1;
	wmemcpy_s(tokens[j], size, start, size);

}



// 블록 내용 뜯어오는 함수
bool TextParser::SearchBlock(const wchar_t* block)
{
	// blocksTxt에서 단어의 덩어리 ( : { ~~~~~~ } ) 의 길이를 구함
	// blocksTxt에서 단어의 덩어리 크기만큼 임시 버퍼 크기만큼 동적할당 후에 저장
	// blocksTxt 해제 & 해당 덩어리 크기만큼 동적할당
	// blocksTxt에 임시 버퍼 memcpy
	// blocksTxt 위치 시작위치로 초기화

	std::stack<wchar_t> s;

	wchar_t* chpBuff;
	wchar_t chWord[256];

	int iLength;

	int i = 0;

	// {}, 덩어리 규격이 맞는지 확인 (틀리면 false)
	while (blocksTxt[i])
	{
		if (blocksTxt[i] == L',') break;

		if (blocksTxt[i] == L'{')
		{
			// 덩어리 괄호 짝 맞추기 ( {, } )
			s.push(blocksTxt[i]);

		}

		if (blocksTxt[i] == L'}')
		{
			s.pop();
		}

		i++;
	}

	// 스택이 비어있지 않으면 괄호 짝이 안맞는 것이므로 false
	if (!s.empty()) return false;

	// 덩어리 시작지점까지 이동
	while (*blocksTxt)
	{
		if (*blocksTxt == L'{') break;

		blocksTxt++;
	}

	// { 다음 위치
	blocksTxt++;

	// { 다음부터 첫 글자 위치 찾기 ( { 와 첫글자 사이에 여러 공백 문자들이 있을 수 있음)
	while (*blocksTxt)
	{
		if (*blocksTxt != L'\n' && *blocksTxt != L'\r' && *blocksTxt != L' ') break;

		blocksTxt++;
	}

	i = 0;

	// 덩어리의 끝 지점인 } 가 나오기 전까지의 데이터 크기 구함
	int length = 0;
	while (blocksTxt[i] != L'}')
	{
		i++;
		length++;
	}

	// 덩어리 내용 저장할 임시 버퍼
	wchar_t* tempTxt = (wchar_t*)malloc(sizeof(wchar_t) * (length + 1));

	i = 0;
	// 덩어리의 끝 지점인 } 가 나오기 전까지 저장
	while (blocksTxt[i] != L'}')
	{
		tempTxt[i] = blocksTxt[i];
		i++;
	}

	tempTxt[i] = L'\0';

	// 이전 덩어리 해제 및 새 덩어리 크기 할당
	blocksTxt = blocksStart;
	free(blocksTxt);

	blocksTxt = (wchar_t*)malloc(sizeof(wchar_t) * (length + 1));

	// 덩어리 시작주소 다시 셋팅
	blocksStart = blocksTxt;

	// 임시버퍼에 있던 덩어리 복사
	wmemcpy_s(blocksTxt, length + 1, tempTxt, length + 1);


	return true;
}

// .txt 파일에서 받아온 문자열들을 파싱해서 원하는 데이터를 얻음
/*
Example)
// 설정파일.txt
Game:
{
	nickname:"player"
	hp:200
	LV:1
}

// 사용법
int lv;
GetValue("Game.LV", &lv);  // lv의 값은 1이 됨
*/
// 실제 항목의 데이터를 얻어옴
bool TextParser::GetValue(const wchar_t* szName, int* ipValue)
{
	blocksTxt = parsingStart;

	wchar_t* chpBuff, chWord[256];
	int iLength;

	chpBuff = (wchar_t*)malloc(128 * sizeof(wchar_t));


	// . 토큰이 존재하는지 확인 -> . 을 기준으로 key 개수 ex) Network.Version -> 2개
	keyCnt = isExistedToken(szName) + 1;

	tokens = (wchar_t**)malloc(sizeof(wchar_t*) * keyCnt);

	for (int j = 0; j < keyCnt; j++)
	{
		tokens[j] = (wchar_t*)malloc(sizeof(wchar_t) * 16);
	}

	split(szName, tokens, sizeof(tokens), keyCnt, L'.');

	int i = 0;

	// 토큰별로 분리한 단어로 계속 value 찾음
	while (i < keyCnt)
	{
		// key가 하나면 바로 value 찾기 (마지막 토큰은 block key가 아니라 단어 그 자체의 key
		if (i == keyCnt - 1)
		{
			// 찾고자 하는 단어가 나올때까지 계속 찾을 것이므로 while 문으로 검사
			while (GetNextWord(&chpBuff, &iLength))
			{
				// Word 버퍼에 찾은 단어를 저장
				wmemset(chWord, 0, 256);
				wmemcpy(chWord, chpBuff, iLength);

				// 인자로 입력 받은 단어와 같은지 검사
				if (0 == wcscmp(tokens[i], chpBuff))
				{
					// 같으면 바로 뒤에 :를 찾음
					if (GetNextWord(&chpBuff, &iLength))
					{
						wmemset(chWord, 0, 256);
						wmemcpy(chWord, chpBuff, iLength);

						if (0 == wcscmp(chpBuff, L":"))
						{
							// : 다음의 데이터(단어) 부분을 얻음
							if (GetNextWord(&chpBuff, &iLength))
							{
								wmemset(chWord, 0, 256);
								wmemcpy(chWord, chpBuff, iLength);

								// 찾은 단어를 정수형으로 변환 후 저장 (문자열일 경우도 고려해야함)
								*ipValue = _wtoi(chWord);
								return true;
							}
							return false;
						}
					}
					return false;
				}
			}
		}
		// 토큰 분리하면서 다음 토큰을 계속 찾아 나섬
		else
		{
			while (GetNextWord(&chpBuff, &iLength))
			{
				// Word 버퍼에 찾은 단어를 저장
				wmemset(chWord, 0, 256);
				wmemcpy(chWord, chpBuff, iLength);

				// 인자로 입력 받은 단어와 같은지 검사
				if (0 == wcscmp(tokens[i], chpBuff))
				{
					// 같으면 바로 뒤에 :를 찾음
					if (GetNextWord(&chpBuff, &iLength))
					{
						wmemset(chWord, 0, 256);
						wmemcpy(chWord, chpBuff, iLength);

						if (0 == wcscmp(chpBuff, L":"))
						{
							// 덩어리 구하기
							// 덩어리 찾으면 다음 토큰으로 넘기기 (true, blocksTxt 덩어리를 저장
							if (SearchBlock(tokens[i]))
							{
								i++;
								break;
							}
							else return false;
						}
						else return false;
					}
				}
			}
		}
	}

	free(chpBuff);

	return false;
}

bool TextParser::GetValue(const wchar_t* szName, wchar_t* wpValue)
{
	blocksTxt = parsingStart;

	wchar_t* chpBuff, chWord[256];
	int iLength;

	chpBuff = (wchar_t*)malloc(128 * sizeof(wchar_t));


	// . 토큰이 존재하는지 확인 -> . 을 기준으로 key 개수 ex) Network.Version -> 2개
	keyCnt = isExistedToken(szName) + 1;

	tokens = (wchar_t**)malloc(sizeof(wchar_t*) * keyCnt);

	for (int j = 0; j < keyCnt; j++)
	{
		tokens[j] = (wchar_t*)malloc(sizeof(wchar_t) * 16);
	}

	split(szName, tokens, sizeof(tokens), keyCnt, L'.');

	int i = 0;

	// 토큰별로 분리한 단어로 계속 value 찾음
	while (i < keyCnt)
	{
		// key가 하나면 바로 value 찾기 (마지막 토큰은 block key가 아니라 단어 그 자체의 key
		if (i == keyCnt - 1)
		{
			// 찾고자 하는 단어가 나올때까지 계속 찾을 것이므로 while 문으로 검사
			while (GetNextWord(&chpBuff, &iLength))
			{
				// Word 버퍼에 찾은 단어를 저장
				wmemset(chWord, 0, 256);
				wmemcpy(chWord, chpBuff, iLength);

				// 인자로 입력 받은 단어와 같은지 검사
				if (0 == wcscmp(tokens[i], chpBuff))
				{
					// 같으면 바로 뒤에 :를 찾음
					if (GetNextWord(&chpBuff, &iLength))
					{
						wmemset(chWord, 0, 256);
						wmemcpy(chWord, chpBuff, iLength);

						if (0 == wcscmp(chpBuff, L":"))
						{
							// : 다음의 데이터(단어) 부분을 얻음
							if (GetNextWord(&chpBuff, &iLength))
							{
								wmemset(chWord, 0, 256);
								wmemcpy(chWord, chpBuff, iLength);

								// 찾은 단어가 큰따옴표로 둘러싸여 있다면, 큰따옴표를 제거
								if (chWord[0] == L'"' && chWord[iLength - 1] == L'"')
								{
									chWord[iLength - 1] = L'\0';
									wcscpy_s(wpValue, wcslen(chWord), chWord + 1);
								}
								else
								{
									wcscpy_s(wpValue, wcslen(chWord) + 1, chWord);
								}

								return true;
							}
							return false;
						}
					}
					return false;
				}
			}
		}
		// 토큰 분리하면서 다음 토큰을 계속 찾아 나섬
		else
		{
			while (GetNextWord(&chpBuff, &iLength))
			{
				// Word 버퍼에 찾은 단어를 저장
				wmemset(chWord, 0, 256);
				wmemcpy(chWord, chpBuff, iLength);

				// 인자로 입력 받은 단어와 같은지 검사
				if (0 == wcscmp(tokens[i], chpBuff))
				{
					// 같으면 바로 뒤에 :를 찾음
					if (GetNextWord(&chpBuff, &iLength))
					{
						wmemset(chWord, 0, 256);
						wmemcpy(chWord, chpBuff, iLength);

						if (0 == wcscmp(chpBuff, L":"))
						{
							// 덩어리 구하기
							// 덩어리 찾으면 다음 토큰으로 넘기기 (true, blocksTxt 덩어리를 저장
							if (SearchBlock(tokens[i]))
							{
								i++;
								break;
							}
							else return false;
						}
						else return false;
					}
				}
			}
		}
	}

	free(chpBuff);

	return false;
}

// . 토큰이 존재하는지 확인
int TextParser::isExistedToken(const wchar_t* chppBuffer)
{
	int cnt = 0;

	while (*chppBuffer)
	{
		if (*chppBuffer == L'.')
		{
			cnt++;
		}

		chppBuffer++;
	}


	return cnt;
}


// 다음 단어 얻기
bool TextParser::GetNextWord(wchar_t** chppBuffer, int* ipLength)
{
	// szName을 . 토큰을 기준으로 분리 (문자열 안에 . 가 있어야 가능)
	// 토큰 분리한 것을 기준으로 단어 갯수 구함 (Network.Version 은 단어 2개)
	// 단어가 1개가 아니면 덩어리 찾기 반복
	// 첫번째 단어부터 시작해서 해당 단어가 텍스트에 있는지 찾음 (GetNextWord) - blocksTxt
	// 단어가 있을 경우, 덩어리 구하기 (true, blocksTxt 위치는 찾은 단어의 끝)
	// 단어가 없을 경우, 구하기 중단 (false, break)
	// 단어 다음이 :{ 가 아니거나 단어 마지막이 },가 아닐 경우, 잘못된 텍스트 (false, break)
	// blocksTxt에서 단어의 덩어리 ( : { ~~~~~~ } ) 의 길이를 구함
	// blocksTxt에서 단어의 덩어리 크기만큼 임시 버퍼 크기만큼 동적할당 후에 저장
	// blocksTxt 해제 & 해당 덩어리 크기만큼 동적할당
	// blocksTxt에 임시 버퍼 memcpy
	// blocksTxt 위치 시작위치로 초기화
	// 단어가 1개면 덩어리 안 찾고 바로 key에 대응하는 value값 찾기
	// 해당 당어가 텍스트에 있는지 찾음 (GetNextWord) - blocksTxt
	// 단어가 있을 경우, key에 대응하는 value값 찾기 (true, blocksTxt 위치는 찾은 단어의 끝)
	// blocksTxt 에서 바로 다음이 : 가 있어야 함 (없으면 잘못된 텍스트! false)
	// : 다음 위치부터 GetNextWord! (true면 잘 찾아온 것)
	// 잘 못찾아오면 문제

	* ipLength = 0;

	// 첫 글자 나올때까지 반복
	while (*blocksTxt == 0x0a || *blocksTxt == 0x0d || *blocksTxt == L' ')
	{
		blocksTxt++;
	}

	int i = 0;

	if (blocksTxt[i] == L':')
	{
		(*ipLength)++;
		i++;
	}
	else
	{
		// 다음 단어의 길이를 얻기위한 반복문
		while (blocksTxt[i])
		{
			// : 이거나 엔터일 경우, 단어 끝남
			if (blocksTxt[i] == L':' || blocksTxt[i] == 0x0a || blocksTxt[i] == 0x0d || blocksTxt[i] == L' ')
			{
				break;
			}

			(*ipLength)++;
			i++;

		}
	}

	// NULL이면 단어 못찾고 맨 마지막까지 간 거니까 false
	if (blocksTxt[i] == NULL) return false;

	*chppBuffer = (wchar_t*)malloc(sizeof(wchar_t) * (*ipLength + 1));

	int j = 0;

	if (blocksTxt[j] == L':')
	{
		(*chppBuffer)[j] = *blocksTxt;
		blocksTxt++;
		j++;
	}
	else
	{
		while (*blocksTxt)
		{
			// : 이거나 엔터일 경우, 단어 끝남
			if (*blocksTxt == L':' || *blocksTxt == 0x0a || *blocksTxt == 0x0d)
			{
				break;
			}

			(*chppBuffer)[j] = *blocksTxt;

			blocksTxt++;
			j++;
		}
	}
	(*chppBuffer)[j] = L'\0';

	return true;
}



// 파일 전체 읽기
void TextParser::LoadFile(const wchar_t* txtName)
{

	FILE* fp = NULL;
	errno_t err = _wfopen_s(&fp, txtName, L"rb");
	if (err != 0 || fp == NULL)
	{
		wprintf(L"텍스트 파일 읽기에 실패하였습니다\n");
		return;
	}

	// 파일 전체 크기
	fseek(fp, 0, SEEK_END);
	_size = ftell(fp);

	rewind(fp);

	buffer = (wchar_t*)malloc(sizeof(wchar_t) * (_size + 1));
	parsingTxt = (wchar_t*)malloc(sizeof(wchar_t) * (_size + 1));
	blocksTxt = (wchar_t*)malloc(sizeof(wchar_t) * (_size + 1));


	// 파일 데이터 읽어옴
	fread(buffer, sizeof(wchar_t), _size, fp);
	buffer[_size] = L'\0';

	// 탭, 개행, 공백, 주석 제거
	SkipNoneCommand();
}


TextParser::~TextParser()
{
	if (fp != NULL)
		fclose(fp);

	if (buffer != NULL)
		free(buffer);

	if (parsingTxt != NULL)
		free(parsingTxt);

	//if (blocksTxt != NULL)
	//	free(blocksTxt);

	for (int i = 0; i < keyCnt; i++)
	{
		if (tokens[i] != NULL)
		{
			free(tokens[i]);
		}
	}

	if (tokens != NULL) free(tokens);

}