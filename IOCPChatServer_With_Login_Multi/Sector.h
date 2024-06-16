#ifndef __SECTOR__
#define __SECTOR__

#define dfSECTOR_X_MAX		50
#define dfSECTOR_Y_MAX		50

//------------------------------------------------------
// ���� �ϳ��� ��ǥ ����
//------------------------------------------------------
struct st_SECTOR_POS
{
	int x;
	int y;
};

// ���� �ϳ��� ��ǥ ���� (multi thread)
struct Sector
{
	// ���Ϳ� �����ϴ� Player ��ü��
	std::unordered_set<Player*> playerSet;
	SRWLOCK sectorLock;

	Sector()
	{
		playerSet.clear();
		InitializeSRWLock(&sectorLock);
	}
};

//------------------------------------------------------
// Ư�� ��ġ �ֺ��� 9�� ���� ����
//------------------------------------------------------
struct st_SECTOR_AROUND
{
	int iCount;
	st_SECTOR_POS Around[9];
};


// Ư�� ���� ��ǥ �������� �ֺ� �ִ� 9���� ���� ��ǥ�� ����
inline void GetSectorAround(int iSectorX, int iSectorY, st_SECTOR_AROUND* pSectorAround)
{
	// ���� �� ���Ϳ��� ������ܺ��� ���� ��ǥ�� ��� ����
	iSectorX--;
	iSectorY--;

	pSectorAround->iCount = 0;

	for (int iY = 0; iY < 3; iY++)
	{
		// �����ڸ��� ��ġ�� ���� ���, ���� ������ ����� �͵��� ����
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
