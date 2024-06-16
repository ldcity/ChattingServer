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

	LockFreeFreeList<Node>* LockFreeQueuePool;

public:
	LockFreeQueue(int size = QUEUEMAX, int bPlacement = false) : _size(0)
	{
		LockFreeQueuePool = new LockFreeFreeList<Node>(size);

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
		
		// LockFreeFreeList에서 노드를 할당받아옴
		Node* realNewNode = LockFreeQueuePool->Alloc();
		if (realNewNode == nullptr)
			return;

		realNewNode->data = _data;
		realNewNode->next = nullptr;

		Node* oldTail = nullptr;			// backup tail
		Node* realOldTail = nullptr;		// backup tail 실제 주소 값
		Node* oldTailNext = nullptr;		// bacup tail의 next 노드
		Node* newNode = nullptr;			// enqueue를 할 새 노드 (tail이 될 노드)

		Node* oldHead = nullptr;			// backup head
		Node* realOldHead = nullptr;		// backup head 실제 주소 값
		Node* oldHeadNext = nullptr;		// backup head의 next 노드

		while (true)
		{
			oldTail = _tail;

			realOldTail = Get64Pointer(oldTail);

			// unique value
			cnt = ((uint64_t)oldTail >> MAX_COUNT_VALUE) + 1;

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

					// CAS 2 성공
					InterlockedCompareExchangePointer((PVOID*)&_tail, newNode, oldTail);

					break;
				}
				else
				{
					// backup tail 노드의 next가 nullptr인 걸 확인하고 진입했는데, 이 사이에 갑자기 nullptr가 아니게 될 경우?
					// - 문제 발생 경우
					// 1. tail 노드와 tail 노드의 next 백업
					// 2. 컨텍스트 스위칭되어 Enqueue와 Dequeue 작업을 여러번 수행하다보면
					// backup 했던 tail 노드와 현재 head 노드의 위치가 역전
					// 3. CAS 1은 성공하지만 CAS 2 작업은 실패 
					// 4. CAS 2 실패 이후 _tail이 마지막 노드로 셋팅이 안되어 있어 무한루프
					// -> 첫번째 CAS가 성공하면 enqueue된 것으로 봄
					// _tail을 마지막 노드로 셋팅하는 CAS 작업을 통해 해결
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
				// backup 했던 tail 노드의 next가 null이 아니기 때문에 무한루프 발생
				// 위와 같이 _tail을 마지막 노드로 셋팅하는 CAS 작업을 통해 해결
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

			// 발생할 수 있는 문제?
			// head를 백업한 후, 컨텍스트 스위칭이 일어나서 다른 스레드가 해당 노드를 디큐, 인큐하여
			// 같은 주소의 노드로 재할당될 때, head의 next가 nullptr로 설정됨 (연결된 노드들의 앞쪽과 뒷쪽이 단절됨)
			// 다시 컨텍스트 스위칭되어 아까의 스레드로 돌아오면 백업 head의 next도 nullptr로 변경되어 있는 상태
			// -> 이 때, size는 0이 아니기 대문에 그럴 경우에는 continue로 이 상황 skip 하여 다음 loop로 넘김
			// size도 0이면 정말로 노드가 1개였고 이를 디큐하려는 상황이었던 것임
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

				// CAS 1
				if (oldHead == InterlockedCompareExchangePointer((PVOID*)&_head, newHead, oldHead))
				{
					oldTail = _tail;
					realOldTail = Get64Pointer(oldTail);

					// Enqueue 에서 발생하는 문제와 같은 유형이므로
					// 추가적인 CAS 작업을 통해 실제 tail 노드 위치를 다시 셋팅하여 해결
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