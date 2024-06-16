#pragma once
/*---------------------------------------------------------------

	메모리 풀 클래스 (오브젝트 풀 / 프리리스트)
	특정 데이타(구조체,클래스,변수)를 일정량 할당 후 나눠쓴다.

	- 사용법.

	procademy::CMemoryPool<DATA> MemPool(300, FALSE);
	DATA *pData = MemPool.Alloc();

	pData 사용

	MemPool.Free(pData);

----------------------------------------------------------------*/
#ifndef  __PROCADEMY_MEMORY_POOL__
#define  __PROCADEMY_MEMORY_POOL__

#include <new>
#include <list>
#include <map>
#include <unordered_map>
#include <iostream>
#include <cmath>

#define POOLLIMIT 1000											// 오브젝트 풀에서 생성할 수 있는 최대 블럭 개수
#define CRASH() do{ int* ptr = nullptr; *ptr = 100;} while(0)	// nullptr 접근해서 강제로 crash 발생

namespace ldcity
{
	// 객체 할당에 사용되는 노드
	template <class DATA>
	struct st_MEMORY_BLOCK_NODE
	{
		int size;
		int underflow;
		DATA data;
		int overflow;
		st_MEMORY_BLOCK_NODE<DATA>* next;
		unsigned int type;
	};

	template <class DATA>
	class ObjectFreeListBase
	{
	public:
		virtual ~ObjectFreeListBase() {}

		virtual DATA* Alloc() = 0;
		virtual bool Free(DATA* ptr) = 0;
	};

	template <class DATA>
	class ObjectFreeList : ObjectFreeListBase<DATA>
	{
	private:
		// 총 노드 정보 - bool이 false = 소멸자 호출 안함, true = 소멸자 호출함
		std::map<st_MEMORY_BLOCK_NODE<DATA>*, bool> memoryBlocks;
		bool m_bPlacementNew;
		unsigned int m_type;							// 오브젝트 풀 타입
		long m_iCapacity;								// 오브젝트 풀 내부 전체 개수
		long m_iUseCount;								// 사용중인 블럭 개수.
		long m_iAllocCnt;
		long m_iFreeCnt;
		CRITICAL_SECTION poolLock;
	public:
		// 스택 방식으로 반환된 (미사용) 오브젝트 블럭을 관리.(Top)
		st_MEMORY_BLOCK_NODE<DATA>* _pFreeBlockNode;

		//////////////////////////////////////////////////////////////////////////
		// 생성자, 파괴자.
		//
		// Parameters:	(int) 초기 블럭 개수. - 0이면 FreeList, 값이 있으면 초기는 메모리 풀, 나중엔 프리리스트
		//				(bool) Alloc 시 생성자 / Free 시 파괴자 호출 여부
		// Return:
		//////////////////////////////////////////////////////////////////////////
		ObjectFreeList(int iBlockNum = 0, bool bPlacementNew = false);

		//// MemoryFreeList에서 객체가 아닌 순수 메모리 할당을 위해 사용되는 생성자
		//ObjectFreeList(int size);

		virtual	~ObjectFreeList();


		//////////////////////////////////////////////////////////////////////////
		// 메모리 풀 타입 설정
		//
		// Parameters: 없음.
		// Return: 없음
		//////////////////////////////////////////////////////////////////////////
		void PoolType(void);


		//////////////////////////////////////////////////////////////////////////
		// 블럭 하나를 할당받는다.  
		//
		// Parameters: 없음.
		// Return: (DATA *) 데이타 블럭 포인터.
		//////////////////////////////////////////////////////////////////////////
		DATA* Alloc(void);

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
		int		GetCapacityCount(void) { return m_iCapacity; }

		//////////////////////////////////////////////////////////////////////////
		// 현재 사용중인 블럭 개수를 얻는다.
		//
		// Parameters: 없음.
		// Return: (int) 사용중인 블럭 개수.
		//////////////////////////////////////////////////////////////////////////
		long		GetUseCount(void) { return m_iUseCount; }

		long		GetAllocCount(void) { return m_iAllocCnt; }

		long		GetFreeCount(void) { return m_iFreeCnt; }



		void	SetCapacityCount(long value)
		{
			InterlockedExchange(&m_iCapacity, value);
		}

		void	SetUseCount(long value)
		{
			InterlockedExchange(&m_iUseCount, value);
		}

		void	SetAllocCount(long value)
		{
			InterlockedExchange(&m_iAllocCnt, value);
		}

		void	SetFreeCount(long value)
		{
			InterlockedExchange(&m_iFreeCnt, value);
		}
	};

	template <class DATA>
	ObjectFreeList<DATA>::ObjectFreeList(int iBlockNum, bool bPlacementNew)
		: _pFreeBlockNode(nullptr), m_bPlacementNew(bPlacementNew), m_iCapacity(iBlockNum), m_iUseCount(0)
	{
		InitializeCriticalSection(&poolLock);

		// 풀 타입 설정
		PoolType();

		// 완전 프리리스트면 생성자에서 아무것도 안 함
		if (m_iCapacity == 0) return;

		// 초기에 메모리 풀처럼 쓰게 메모리 덩어리만 할당 
		char* memoryBlock = (char*)malloc(sizeof(st_MEMORY_BLOCK_NODE<DATA>));

		if (memoryBlock == nullptr) throw std::bad_alloc();

		// 안전장치(0xfdfdfdfd)를 앞뒤로 넣고 data는 객체가 아닌 메모리 덩어리 상태인채로 둠
		// -> placement new를 할 것인지 아닌지에 따라 메모리 덩어리로 둘지, 생성자 호출하여 객체로 둘지 결정됨
		_pFreeBlockNode = (st_MEMORY_BLOCK_NODE<DATA>*)(memoryBlock);
		_pFreeBlockNode->underflow = 0xfdfdfdfd;
		_pFreeBlockNode->overflow = 0xfdfdfdfd;
		_pFreeBlockNode->type = m_type;
		_pFreeBlockNode->next = nullptr;

		// placement new 가 false 면 객체 생성(처음에 노드 생성할때만 생성자 호출) - 그 이후에는 안함!
		if (!m_bPlacementNew)
			new(&_pFreeBlockNode->data)DATA;

		// 리스트 삭제를 위해 노드 정보들 모두 저장 - false = 소멸자 호출 안한 주소값
		memoryBlocks.insert({ _pFreeBlockNode, false });

		st_MEMORY_BLOCK_NODE<DATA>* temp = _pFreeBlockNode;

		for (int i = 0; i < m_iCapacity - 1; i++)
		{
			// 새 노드 생성
			char* memoryBlock = (char*)malloc(sizeof(st_MEMORY_BLOCK_NODE<DATA>));
			if (memoryBlock == nullptr) throw std::bad_alloc();

			st_MEMORY_BLOCK_NODE<DATA>* newNode = (st_MEMORY_BLOCK_NODE<DATA>*)(memoryBlock);
			newNode->underflow = 0xfdfdfdfd;
			newNode->overflow = 0xfdfdfdfd;
			newNode->type = m_type;
			newNode->next = _pFreeBlockNode;
			_pFreeBlockNode = newNode;

			// placement new 가 false 면 객체 생성(처음에 노드 생성할때만 생성자 호출) - 그 이후에는 안함!
			if (!m_bPlacementNew)
				new(&newNode->data)DATA;

			// 리스트 삭제를 위해 노드 정보들 모두 저장
			memoryBlocks.insert({ newNode, false });
		}
	}

	template <class DATA>
	ObjectFreeList<DATA>::~ObjectFreeList()
	{
		for (auto iter = memoryBlocks.begin(); iter != memoryBlocks.end(); ++iter)
		{
			st_MEMORY_BLOCK_NODE<DATA>* node = (st_MEMORY_BLOCK_NODE<DATA>*)iter->first;

			// placement new 가 false 면 객체 소멸 (메모리 풀 소멸시 마지막에만 소멸자 호출)
			// placement new 가 true 면 소멸자 호출된 것도 있고 안한 것도 있으므로
			// map으로 소멸자 호출 여부 flag도 저장해서 정리하거나
			// 배열이나 리스트라면 소멸자를 호출했는지 아닌지 확인하면서 호출
			if (!m_bPlacementNew)
			{
				node->data.~DATA();
			}
			else
			{
				// second 값이 false : 소멸자 호출 안됐을 경우
				if (!iter->second)
					node->data.~DATA();
			}

			free(node);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// 메모리 풀 타입 설정
	//
	// Parameters: 없음.
	// Return: 없음
	//////////////////////////////////////////////////////////////////////////
	template <class DATA>
	void ObjectFreeList<DATA>::PoolType()
	{
		// 메모리 풀 주소값을 type으로 설정
		m_type = (unsigned int)this;
	}

	//////////////////////////////////////////////////////////////////////////
	// 블럭 하나를 할당받는다.  
	//
	// Parameters: 없음.
	// Return: (DATA *) 데이타 블럭 포인터.
	//////////////////////////////////////////////////////////////////////////
	template <class DATA>
	DATA* ObjectFreeList<DATA>::Alloc()
	{
		EnterCriticalSection(&poolLock);

		m_iAllocCnt++;

		// 미사용 블럭(free block)이 있을 때
		if (_pFreeBlockNode != nullptr)
		{
			// 메모리 블럭의 데이터 덩어리(or 객체) 가져온 후 _pFreeBlockNode 위치 바꿈
			DATA* object = &_pFreeBlockNode->data;

			_pFreeBlockNode->size = sizeof(DATA);
			_pFreeBlockNode = _pFreeBlockNode->next;

			// 생성자 호출
			if (m_bPlacementNew) new(object) DATA;

			// 사용중인 블럭 개수 증가
			m_iUseCount++;

			LeaveCriticalSection(&poolLock);
			return object;
		}
		// 미사용 블럭(free block)이 없을 때
		// 새로운 블럭 생성
		char* memoryBlock = (char*)malloc(sizeof(st_MEMORY_BLOCK_NODE<DATA>));
		if (memoryBlock == nullptr) throw std::bad_alloc();

		st_MEMORY_BLOCK_NODE<DATA>* newNode = (st_MEMORY_BLOCK_NODE<DATA>*)(memoryBlock);
		newNode->size = sizeof(DATA);
		newNode->underflow = 0xfdfdfdfd;
		newNode->overflow = 0xfdfdfdfd;
		newNode->type = m_type;
		newNode->next = nullptr;

		DATA* object = &newNode->data;

		// 새 블럭 생성이므로 생성자 호출
		new(object)DATA;

		// 전체 블럭 개수 증가
		m_iCapacity++;

		memoryBlocks.insert({ newNode, false });

		// 사용중인 블럭 개수 증가
		m_iUseCount++;

		LeaveCriticalSection(&poolLock);

		return object;
	}

	//////////////////////////////////////////////////////////////////////////
	// 사용중이던 블럭을 해제한다.
	//
	// Parameters: (DATA *) 블럭 포인터.
	// Return: (BOOL) TRUE, FALSE.
	//////////////////////////////////////////////////////////////////////////
	template <class DATA>
	bool ObjectFreeList<DATA>::Free(DATA* pData)
	{
		EnterCriticalSection(&poolLock);

		// 안전장치(0xfdfdfdfd) 부분부터 접근 (pData의 4byte 앞)
		st_MEMORY_BLOCK_NODE<DATA>* temp = (st_MEMORY_BLOCK_NODE<DATA>*)((char*)pData - sizeof(int) * 2);

		// 메모리 풀과 type이 다를 경우
		if (temp->type != m_type)
		{
			wprintf(L"Pool Type Failed\n");
			LeaveCriticalSection(&poolLock);
			return false;
		}

		// 사용 중인 블럭이 없어서 더이상 해제할 블럭이 없을 경우
		if (m_iUseCount == 0)
		{
			wprintf(L"Free Failed.\n");
			LeaveCriticalSection(&poolLock);
			return false;
		}

		// 메모리 접근 위반
		if (temp->underflow != 0xfdfdfdfd || temp->overflow != 0xfdfdfdfd)
			CRASH();

		// 소멸자 호출 - placement new가 true이면 객체 반환할 때 소멸자 호출시켜야 함
		if (m_bPlacementNew)
		{
			auto it = memoryBlocks.find(temp);
			if (it != memoryBlocks.end())
			{
				it->second = true;
				temp->data.~DATA();
			}
			else CRASH();
		}

		m_iFreeCnt++;

		if (_pFreeBlockNode == nullptr)
		{
			// 해당 객체를 _pFreeBlockNode(top)으로 변경
			_pFreeBlockNode = temp;
			_pFreeBlockNode->next = nullptr;

			//// buffer clear
			//memset(&_pFreeBlockNode->data, 0, sizeof(_pFreeBlockNode->data));

			// 사용중인 블럭 개수 감소
			m_iUseCount--;

			LeaveCriticalSection(&poolLock);

			return true;
		}

		//// buffer clear
		//memset(&temp->data, 0, sizeof(temp->data));

		// 해당 객체를 _pFreeBlockNode(top)으로 변경
		temp->next = _pFreeBlockNode;
		_pFreeBlockNode = temp;

		// 사용중인 블럭 개수 감소
		m_iUseCount--;

		LeaveCriticalSection(&poolLock);

		return true;
	}
}




#endif

