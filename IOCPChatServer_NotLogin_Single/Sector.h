#pragma once
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
	// 현재 내 섹터에서 좌측상단부터 섹터 좌표를 얻기 위해 x, y 좌표 둘 다 -1 시킴
	iSectorX--;
	iSectorY--;

	// 섹터 갯수 counting
	pSectorAround->iCount = 0;

	for (int iY = 0; iY < 3; iY++)
	{
		// 모서리에 위치해 있을 경우, 섹터 범위를 벗어나는 것들은 제외시킴
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
