#pragma once
#ifndef __LOCKFREESTACK__HEADER__
#define __LOCKFREESTACK__HEADER__

#pragma comment(lib, "winmm.lib")

#include <Windows.h>
#include <vector>

#include "LockFreeFreeList.h"

#define CRASH() do { int* ptr = nullptr; *ptr = 100;} while(0);

template <typename DATA>
class LockFreeStack
{

private:
	struct Node
	{
		Node() :data(NULL), next(nullptr)
		{
		}

		DATA data;
		Node* next;
	};

	alignas(64) Node* pTop;							// Stack Top
	alignas(64) __int64 _size;

	LockFreeFreeList<Node>* LockFreeStackPool;

public:
	LockFreeStack(int size = STACKMAX, int bPlacement = false) : _size(0), pTop(nullptr)
	{
		LockFreeStackPool = new LockFreeFreeList<Node>(size, bPlacement);
	}

	// unique pointer (64bit 모델에서 현재 사용되지 않는 상위 17bit를 사용하여 고유한 포인터 값 생성
	inline Node* GetUniqu64ePointer(Node* pointer, uint64_t iCount)
	{
		return (Node*)((uint64_t)pointer | (iCount << MAX_COUNT_VALUE));
	}

	// real pointer
	inline Node* Get64Pointer(Node* uniquePointer)
	{
		return (Node*)((uint64_t)uniquePointer & PTRMASK);
	}

	bool Push(DATA data)
	{
		// LockFreeFreeList에서 노드 얻어옴
		Node* newRealNode = LockFreeStackPool->Alloc();
		if (newRealNode == nullptr)
			return false;

		newRealNode->data = data;

		Node* oldTop = nullptr;
		Node* realOldTop = nullptr;
		Node* newNode = nullptr;
		uint64_t cnt;

		while (true)
		{
			// backup용 top node (이 사이에 다른 스레드로 인해 top 위치가 바뀔 수도 있기에 backup)
			oldTop = pTop;

			// top node의 상위 17bit를 얻어와 증가연산을 함
			cnt = ((uint64_t)oldTop >> MAX_COUNT_VALUE) + 1;

			// backup용 top node의 실제 주소 값
			realOldTop = Get64Pointer(oldTop);

			// 삽입하려는 node의 next 위치에 backup한 top node를 셋팅
			newRealNode->next = realOldTop;

			// x64 환경에서 현재 사용되고 있지 않는 상위 17bit와 실제 노드의 주소 값을 | 연산하여 고유 값 생성
			newNode = GetUniqu64ePointer(newRealNode, cnt);

			// CAS 연산
			// backup용 top node와 현재 top node가 같다면 이 사이에 컨텍스트 스위칭하여 다른 스레드에서 top 노드가 변하지 않았다는 의미
			// -> 삽입하려고 들어온 노드의 고유 값으로 top 설정
			if ((PVOID)oldTop == InterlockedCompareExchangePointer((PVOID*)&pTop, newNode, oldTop))
			{
				break;
			}
		}

		InterlockedIncrement64(&_size);
		return true;
	}

	bool Pop(DATA* _value)
	{
		Node* oldTop = nullptr;
		Node* realOldTop = nullptr;

		Node* oldTopNext = nullptr;
		Node* realOldTopNext = nullptr;

		uint64_t cnt;

		while (true)
		{
			// backup용 top node (이 사이에 다른 스레드로 인해 top 위치가 바뀔 수도 있기에 backup)
			oldTop = pTop;

			// backup용 top node의 실제 주소 값
			realOldTop = Get64Pointer(oldTop);

			if (realOldTop == nullptr)
				return false;

			// top node의 상위 17bit를 얻어와 증가연산을 함
			cnt = ((uint64_t)oldTop >> MAX_COUNT_VALUE) + 1;

			// x64 환경에서 현재 사용되고 있지 않는 상위 17bit와 실제 노드의 주소 값을 | 연산하여 고유 값 생성
			// backup한 top 노드의 next의 고유 값 생성 (top이 될 노드)
			realOldTopNext = realOldTop->next;
			oldTopNext = GetUniqu64ePointer(realOldTopNext, cnt);

			// CAS 연산
			// backup용 top node와 현재 top node가 같다면 이 사이에 컨텍스트 스위칭하여 다른 스레드에서 top 노드가 변하지 않았다는 의미
			// -> top을 pop할 노드의 next로 설정
			if ((PVOID)oldTop == InterlockedCompareExchangePointer((PVOID*)&pTop, oldTopNext, oldTop))
			{
				// pop할 노드의 데이터를 (실제 사용자가 오규하는 값) 얻음
				*_value = realOldTop->data;

				// LockFreeFreeList에 pop한 노드 반환 (delete된 게 아니라 LockFreeFreeList에 보관)
				LockFreeStackPool->Free(realOldTop);

				break;
			}
		}

		InterlockedDecrement64(&_size);

		return true;
	}

	__int64 GetSize() { return _size; }

	~LockFreeStack()
	{
		delete LockFreeStackPool;
	}
};

#endif // !1
