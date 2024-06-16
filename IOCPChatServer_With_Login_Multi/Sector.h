#ifndef __SECTOR__
#define __SECTOR__

#define dfSECTOR_X_MAX		50
#define dfSECTOR_Y_MAX		50

//------------------------------------------------------
// 섹터 하나의 좌표 정보
//------------------------------------------------------
struct st_SECTOR_POS
{
	int x;
	int y;
};

// 섹터 하나의 좌표 정보 (multi thread)
struct Sector
{
	// 섹터에 존재하는 Player 객체들
	std::unordered_set<Player*> playerSet;
	SRWLOCK sectorLock;

	Sector()
	{
		playerSet.clear();
		InitializeSRWLock(&sectorLock);
	}
};

//------------------------------------------------------
// 특정 위치 주변의 9개 섹터 정보
//------------------------------------------------------
struct st_SECTOR_AROUND
{
	int iCount;
	st_SECTOR_POS Around[9];
};


// 특정 섹터 좌표 기준으로 주변 최대 9개의 섹터 좌표를 얻어옴
inline void GetSectorAround(int iSectorX, int iSectorY, st_SECTOR_AROUND* pSectorAround)
{
	// 현재 내 섹터에서 좌측상단부터 섹터 좌표를 얻기 위함
	iSectorX--;
	iSectorY--;

	pSectorAround->iCount = 0;

	for (int iY = 0; iY < 3; iY++)
	{
		// 가장자리에 위치해 있을 경우, 섹터 범위를 벗어나는 것들은 제외
		if (iSectorY + iY < 0 || iSectorY + iY >= dfSECTOR_Y_MAX)
			continue;

		for (int iX = 0; iX < 3; iX++)
		{
			if (iSectorX + iX < 0 || iSectorX + iX >= dfSECTOR_X_MAX)
				continue;

			pSectorAround->Around[pSectorAround->iCount].x = iSectorX + iX;
			pSectorAround->Around[pSectorAround->iCount].y = iSectorY + iY;
			++pSectorAround->iCount;
		}
	}
}


#endif // !__SECTOR__
