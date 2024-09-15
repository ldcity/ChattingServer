#ifndef __TEXT_PARSER__
#define __TEXT_PARSER__

class TextParser
{
private:
	wchar_t* buffer;					// 텍스트 내용
	wchar_t* parsingTxt;
	wchar_t* parsingStart;
	wchar_t* blocksStart;				// 블럭의 시작 위치
	wchar_t** tokens;					// . 토큰 분리한 문자열
	wchar_t* blocksTxt;					// 블록(덩어리) 내용
	int keyCnt;							// tokens 개수
	unsigned long _size;				// 텍스트 전체 크기
	FILE* fp;
public:
	TextParser();

	bool SearchBlock(const wchar_t*  blockName);

	void split(const wchar_t* szName, wchar_t** tokens, int tokensLen, int keyCnt, const wchar_t delimiter);

	// 탭, 개행, 공백, 주석 처리
	bool SkipNoneCommand();

	// . 토큰이 존재하는지 확인
	int isExistedToken(const wchar_t* chppBuffer);

	// 실제 항목의 데이터를 얻어옴
	bool GetValue(const wchar_t* szName, int* ipValue);

	bool GetValue(const wchar_t* szName, wchar_t* wpValue);

	// 다음 단어 얻기
	bool GetNextWord(wchar_t** chppBuffer, int* ipLength);

	// 파일 전체 읽기
	void LoadFile(const wchar_t* txtName);

	~TextParser();
};

#endif // !__TEXT_PARSER__