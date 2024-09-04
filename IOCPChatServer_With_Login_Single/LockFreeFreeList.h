/*---------------------------------------------------------------

procademy MemoryPool.

메모리 풀 클래스 (오브젝트 풀 / 프리리스트)
특정 데이타(구조체,클래스,변수)를 일정량 할당 후 나눠쓴다.

- 사용법.

procademy::CMemoryPool<DATA> MemPool(300, FALSE);
DATA *pData = MemPool.Alloc();

pData 사용

MemPool.Free(pData);


----------------------------------------------------------------*/
#ifndef  __LOCKFREESTACK_MEMORY_POOL__
#define  __LOCKFREESTACK_MEMORY_POOL__

#include <Windows.h>
#include <vector>
#include <iostream>

#define CRASH() do{ int* ptr = nullptr; *ptr = 100;} while(0)		// nullptr 접근해서 강제로 crash 발생


#define MEMORYPOOLCNT 10000											// 메모리 풀 블록 개수
#define QUEUEMAX 100
#define STACKMAX 200

#define MAXBITS 17
#define MAX_COUNT_VALUE 47
#define PTRMASK 0x00007FFFFFFFFFFF
#define MAXIMUMADDR 0x00007FFFFFFEFFFF

template <class DATA>
class ObjectFreeListBase
{
public:
	virtual ~ObjectFreeListBase() {}
	virtual DATA* Alloc() = 0;
	virtual bool Free(DATA* ptr) = 0;
};

template <class DATA>
class ObjectLockFreeStackFreeList : ObjectFreeListBase<DATA>
{
private:
	alignas(64) uint64_t _iUseCount;								// 사용중인 블럭 개수.
	alignas(64) uint64_t _iAllocCnt;
	alignas(64) uint64_t _iFreeCnt;
	alignas(64) uint64_t _iCapacity;								// 오브젝트 풀 내부 전체 개수

	// 객체 할당에 사용되는 노드
	template <class DATA>
	struct st_MEMORY_BLOCK_NODE
	{
		DATA data;
		st_MEMORY_BLOCK_NODE<DATA>* next;
	};

	// 스택 방식으로 반환된 (미사용) 오브젝트 블럭을 관리.(Top)
	st_MEMORY_BLOCK_NODE<DATA>* _pFreeBlockNode;

	bool _bPlacementNew;

public:
	//////////////////////////////////////////////////////////////////////////
	// 생성자, 파괴자.
	//
	// Parameters:	(int) 초기 블럭 개수. - 0이면 FreeList, 값이 있으면 초기는 메모리 풀, 나중엔 프리리스트
	//				(bool) Alloc 시 생성자 / Free 시 파괴자 호출 여부
	// Return:
	//////////////////////////////////////////////////////////////////////////
	ObjectLockFreeStackFreeList(int iBlockNum = 0, bool bPlacementNew = false);

	virtual	~ObjectLockFreeStackFreeList();

	//////////////////////////////////////////////////////////////////////////
	// 블럭 하나를 할당받는다.  
	//
	// Parameters: 없음.
	// Return: (DATA *) 데이타 블럭 포인터.
	//////////////////////////////////////////////////////////////////////////
	DATA* Alloc();

	//////////////////////////////////////////////////////////////////////////
	// 사용중이던 블럭을 해제한다.
	//
	// Parameters: (DATA *) 블럭 포인터.
	// Return: (BOOL) TRUE, FALSE.
	//////////////////////////////////////////////////////////////////////////
	bool	Free(DATA* pData);

	//////////////////////////////////////////////////////////////////////////
	// 현재 확보 된 블럭 개수를 얻는다. (메모리풀 내부의 전체 개수)
	//
	// Parameters: 없음.
	// Return: (int) 메모리 풀 내부 전체 개수
	//////////////////////////////////////////////////////////////////////////
	uint64_t		GetCapacityCount(void) { return _iCapacity; }

	//////////////////////////////////////////////////////////////////////////
	// 현재 사용중인 블럭 개수를 얻는다.
	//
	// Parameters: 없음.
	// Return: (int) 사용중인 블럭 개수.
	//////////////////////////////////////////////////////////////////////////
	uint64_t		GetUseCount(void) { return _iUseCount; }

	uint64_t		GetAllocCount(void) { return _iAllocCnt; }

	uint64_t		GetFreeCount(void) { return _iFreeCnt; }

	// unique pointer
	inline st_MEMORY_BLOCK_NODE<DATA>* GetUniqu64ePointer(st_MEMORY_BLOCK_NODE<DATA>* pointer, uint64_t iCount)
	{
		return (st_MEMORY_BLOCK_NODE<DATA>*)((uint64_t)pointer | ((uint64_t)iCount << MAX_COUNT_VALUE));
	}

	// real pointer
	inline st_MEMORY_BLOCK_NODE<DATA>* Get64Pointer(st_MEMORY_BLOCK_NODE<DATA>* uniquePointer)
	{
		return (st_MEMORY_BLOCK_NODE<DATA>*)((uint64_t)uniquePointer & PTRMASK);
	}
};

template <class DATA>
ObjectLockFreeStackFreeList<DATA>::ObjectLockFreeStackFreeList(int iBlockNum, bool bPlacementNew)
	: _pFreeBlockNode(nullptr), _bPlacementNew(bPlacementNew), _iCapacity(iBlockNum), _iUseCount(0), _iAllocCnt(0), _iFreeCnt(0)
{
	// x64 모델에서 사용할 수 있는 메모리 대역이 변경됐는지 확인
	SYSTEM_INFO si;
	GetSystemInfo(&si);

	if ((__int64)si.lpMaximumApplicationAddress != MAXIMUMADDR)
	{
		wprintf(L"X64 Maximum Address Used...\n");
		CRASH();
	}

	// 완전 프리리스트면 생성자에서 아무것도 안 함
	if (_iCapacity == 0) return;

	st_MEMORY_BLOCK_NODE<DATA>* pFreeBlockNode = nullptr;

	for (int i = 0; i < _iCapacity; i++)
	{
		// 새 노드 생성
		st_MEMORY_BLOCK_NODE<DATA>* memoryBlock = (st_MEMORY_BLOCK_NODE<DATA>*)_aligned_malloc(sizeof(st_MEMORY_BLOCK_NODE<DATA>), alignof(st_MEMORY_BLOCK_NODE<DATA>));
		if (memoryBlock == nullptr)
			CRASH();
		//memoryBlock->next = _pFreeBlockNode;
		//_pFreeBlockNode = newNode;
		memoryBlock->next = pFreeBlockNode;
		pFreeBlockNode = memoryBlock;

		// placement new 가 false 면 객체 생성(처음에 노드 생성할때만 생성자 호출) - 그 이후에는 안함!
		if (!_bPlacementNew)
			new(&memoryBlock->data)DATA;
	}

	_pFreeBlockNode = pFreeBlockNode;
}

template <class DATA>
ObjectLockFreeStackFreeList<DATA>::~ObjectLockFreeStackFreeList()
{
	while (_pFreeBlockNode != nullptr)
	{
		st_MEMORY_BLOCK_NODE<DATA>* curNode = Get64Pointer(_pFreeBlockNode);

		if (!_bPlacementNew)
		{
			curNode->data.~DATA();
		}

		_pFreeBlockNode = curNode->next;

		_aligned_free(curNode);

	}

}


//////////////////////////////////////////////////////////////////////////
// 블럭 하나를 할당받는다.  - 프리노드를 pop
//
// Parameters: 없음.
// Return: (DATA *) 데이타 블럭 포인터.
//////////////////////////////////////////////////////////////////////////
template <class DATA>
DATA* ObjectLockFreeStackFreeList<DATA>::Alloc()
{
	st_MEMORY_BLOCK_NODE<DATA>* pOldTopNode = nullptr;
	st_MEMORY_BLOCK_NODE<DATA>* pOldRealNode = nullptr;
	st_MEMORY_BLOCK_NODE<DATA>* pNewTopNode = nullptr;

	uint64_t cnt;

	while (true)
	{
		pOldTopNode = _pFreeBlockNode;

		pOldRealNode = Get64Pointer(pOldTopNode);

		// 미사용 블럭(free block)이 없을 때 
		if (pOldRealNode == nullptr)
		{
			pOldRealNode = (st_MEMORY_BLOCK_NODE<DATA>*)_aligned_malloc(sizeof(st_MEMORY_BLOCK_NODE<DATA>), alignof(st_MEMORY_BLOCK_NODE<DATA>));
			if (pOldRealNode == nullptr)
				CRASH();

			pOldRealNode->next = nullptr;

			// 전체 블럭 개수 증가
			InterlockedIncrement64((LONG64*)&_iCapacity);

			break;
		}

		cnt = ((uint64_t)pOldTopNode >> MAX_COUNT_VALUE) + 1;

		//pNewTopNode = pOldRealNode->next;
		pNewTopNode = GetUniqu64ePointer(pOldRealNode->next, cnt);

		if (pOldTopNode == InterlockedCompareExchangePointer((PVOID*)&_pFreeBlockNode, pNewTopNode, pOldTopNode))
		{
			break;
		}
	}

	DATA* object = &pOldRealNode->data;

	// 새 블럭 생성이므로 생성자 호출
	if (_bPlacementNew)
	{
		new(object) DATA;
	}

	InterlockedIncrement64((LONG64*)&_iAllocCnt);

	// 사용중인 블럭 개수 증가
	InterlockedIncrement64((LONG64*)&_iUseCount);

	return object;
}

//////////////////////////////////////////////////////////////////////////
// 사용중이던 블럭을 해제한다. - 프리노드를 push
//
// Parameters: (DATA *) 블럭 포인터.
// Return: (BOOL) TRUE, FALSE.
//////////////////////////////////////////////////////////////////////////
template <class DATA>
bool ObjectLockFreeStackFreeList<DATA>::Free(DATA* pData)
{
	st_MEMORY_BLOCK_NODE<DATA>* realTemp = (st_MEMORY_BLOCK_NODE<DATA>*)((char*)pData);

	// 사용 중인 블럭이 없어서 더이상 해제할 블럭이 없을 경우
	if (_iUseCount == 0)
	{
		wprintf(L"Free Failed.\n");
		CRASH();
		return false;
	}


	// backup할 기존 free top 노드
	st_MEMORY_BLOCK_NODE<DATA>* oldTopNode = nullptr;
	st_MEMORY_BLOCK_NODE<DATA>* temp = nullptr;
	uint64_t cnt;

	while (true)
	{
		// free node top을 backup
		oldTopNode = _pFreeBlockNode;

		cnt = ((uint64_t)oldTopNode >> MAX_COUNT_VALUE) + 1;

		// 해제하려는 node를 프리리스트의 top으로 임시 셋팅
		//realTemp->next = oldTopNode;
		//temp = GetUniqu64ePointer(realTemp, cnt);
		realTemp->next = Get64Pointer(oldTopNode);

		temp = GetUniqu64ePointer(realTemp, cnt);

		// CAS
		if (oldTopNode == InterlockedCompareExchangePointer((PVOID*)&_pFreeBlockNode, temp, oldTopNode))
		{
			break;
		}
	}

	// 소멸자 호출 - placement new가 true이면 객체 반환할 때 소멸자 호출시켜야 함
	// CAS 작업이 성공하여 free node를 push 완료하고나서 소멸자 호출해야함 (oldNode의 소멸자 호출)
	if (_bPlacementNew)
	{
		realTemp->data.~DATA();
	}

	InterlockedIncrement64((LONG64*)&_iFreeCnt);
	InterlockedDecrement64((LONG64*)&_iUseCount);

	return true;
}


#endif

