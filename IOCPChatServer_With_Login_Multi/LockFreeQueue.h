#pragma once
#ifndef __LOCKFREEQUEUE__HEADER__
#define __LOCKFREEQUEUE__HEADER__

#pragma comment(lib, "winmm.lib")

#include <Windows.h>
#include <vector>

#include "LockFreeFreeList.h"

#define CRASH() do { int* ptr = nullptr; *ptr = 100;} while(0);

template <typename DATA>
class LockFreeQueue
{
private:
	struct Node
	{
		DATA data;
		Node* next;
	};

	alignas(64)Node* _head;
	alignas(64)	Node* _tail;
	
	alignas(64) __int64 _size;

	ObjectLockFreeStackFreeList<Node>* LockFreeQueuePool;

public:
	LockFreeQueue(int size = QUEUEMAX, int bPlacement = false) : _size(0)
	{
		LockFreeQueuePool = new ObjectLockFreeStackFreeList<Node>(size);

		// dummy node
		Node* node = LockFreeQueuePool->Alloc();
		node->next = nullptr;

		_head = node;
		_tail = _head;
	}

	~LockFreeQueue()
	{
		Node* node = Get64Pointer(_head);

		while (node != nullptr)
		{
			Node* temp = node;
			node = node->next;
			LockFreeQueuePool->Free(temp);
		}

		delete LockFreeQueuePool;
	}


	inline __int64 GetSize() { return _size; }

	// unique pointer
	inline Node* GetUniqu64ePointer(Node* pointer, uint64_t iCount)
	{
		return (Node*)((uint64_t)pointer | (iCount << MAX_COUNT_VALUE));
	}

	// real pointer
	inline Node* Get64Pointer(Node* uniquePointer)
	{
		return (Node*)((uint64_t)uniquePointer & PTRMASK);
	}

	void Enqueue(DATA _data)
	{

		uint64_t cnt;

		Node* realNewNode = LockFreeQueuePool->Alloc();
		if (realNewNode == nullptr)
			return;

		realNewNode->data = _data;
		realNewNode->next = nullptr;

		Node* oldTail = nullptr;
		Node* realOldTail = nullptr;
		Node* oldTailNext = nullptr;
		Node* newNode = nullptr;

		Node* oldHead = nullptr;
		Node* realOldHead = nullptr;
		Node* oldHeadNext = nullptr;

		while (true)
		{
			oldTail = _tail;

			realOldTail = Get64Pointer(oldTail);

			// unique value
			cnt = ((uint64_t)oldTail >> MAX_COUNT_VALUE) + 1;

			// 기존 tail의 next 백업
			oldTailNext = realOldTail->next;

			oldHead = _head;
			realOldHead = Get64Pointer(oldHead);
			oldHeadNext = realOldHead->next;

			if (oldTailNext == nullptr)
			{

				// CAS 1 성공
				if (oldTailNext == InterlockedCompareExchangePointer((PVOID*)&realOldTail->next, realNewNode, oldTailNext))
				{
					newNode = GetUniqu64ePointer(realNewNode, cnt);
					InterlockedCompareExchangePointer((PVOID*)&_tail, newNode, oldTail);

					break;
				}
				else
				{
					oldTailNext = realOldTail->next;
					if (oldTailNext != nullptr)
					{
						newNode = GetUniqu64ePointer(oldTailNext, cnt);
						InterlockedCompareExchangePointer((PVOID*)&_tail, newNode, oldTail);
					}
				}

			}
			else
			{
				// oldTail의 next가 null이 아님
				// 이 부분 해결 안되면 무한루프
				// CAS 2 실패 이후 _tail이 마지막 노드로 셋팅이 안되어 발생하는 문제
				// -> _tail을 마지막 노드로 셋팅
				// -> 첫번째 CAS가 성공하면 enqueue된 것으로 봄

				if (_tail == oldTail)
				{
					newNode = GetUniqu64ePointer(oldTailNext, cnt);
					InterlockedCompareExchangePointer((PVOID*)&_tail, newNode, oldTail);
				}
			}
		}

		InterlockedIncrement64(&_size);
	}

	bool Dequeue(DATA& _data)
	{
		if (_size <= 0)
		{
			return false;
		}

		InterlockedDecrement64(&_size);

		uint64_t cnt;

		Node* oldHead = nullptr;
		Node* raalOldHead = nullptr;
		Node* oldHeadNext = nullptr;

		Node* oldTail = nullptr;
		Node* realOldTail = nullptr;
		Node* newTail = nullptr;

		while (true)
		{
			// head, next 백업
			oldHead = _head;
			raalOldHead = Get64Pointer(oldHead);

			cnt = ((uint64_t)oldHead >> MAX_COUNT_VALUE) + 1;

			oldHeadNext = raalOldHead->next;

			// 백업 head의 next가 될 일이 없음 -> 테스트 코드에서 개수에 맞춰 인큐, 디큐를 하기 때문
			// 그럼에도 발생할 수 있는 문제?
			// head를 백업한 후, 컨텍스트 스위칭이 일어나서 다른 스레드가 해당 노드를 디큐, 인큐하여
			// 같은 주소의 노드로 재할당될 때, head의 next가 nullptr로 설정됨
			// 다시 컨텍스트 스위칭되어 아까의 스레드로 돌아오면 백업 head의 next도 nullptr로 변경되어 있는 상태
			// -> 실제로 큐가 비어있어야 진행
			//if (oldHeadNext == nullptr && oldSize <= 0)
			if (oldHeadNext == nullptr)
			{
				if (_size >= 0)
					continue;

				InterlockedIncrement64(&_size);

				return false;
			}
			else
			{
				_data = oldHeadNext->data;
				Node* newHead = GetUniqu64ePointer(oldHeadNext, cnt);

				if (oldHead == InterlockedCompareExchangePointer((PVOID*)&_head, newHead, oldHead))
				{
					oldTail = _tail;
					realOldTail = Get64Pointer(oldTail);

					if (realOldTail->next == oldHeadNext)
					{
						uint64_t rearCnt = ((uint64_t)oldTail >> MAX_COUNT_VALUE) + 1;
						newTail = GetUniqu64ePointer(oldHeadNext, rearCnt);
						InterlockedCompareExchangePointer((PVOID*)&_tail, newTail, oldTail);
					}

					LockFreeQueuePool->Free(raalOldHead);
					break;
				}
			}
		}

		return true;
	}
};

#endif