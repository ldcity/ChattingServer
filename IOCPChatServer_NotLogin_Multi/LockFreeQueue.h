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
		
		// LockFreeFreeList���� ��带 �Ҵ�޾ƿ�
		Node* realNewNode = LockFreeQueuePool->Alloc();
		if (realNewNode == nullptr)
			return;

		realNewNode->data = _data;
		realNewNode->next = nullptr;

		Node* oldTail = nullptr;			// backup tail
		Node* realOldTail = nullptr;		// backup tail ���� �ּ� ��
		Node* oldTailNext = nullptr;		// bacup tail�� next ���
		Node* newNode = nullptr;			// enqueue�� �� �� ��� (tail�� �� ���)

		Node* oldHead = nullptr;			// backup head
		Node* realOldHead = nullptr;		// backup head ���� �ּ� ��
		Node* oldHeadNext = nullptr;		// backup head�� next ���

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

				// CAS 1 ����
				if (oldTailNext == InterlockedCompareExchangePointer((PVOID*)&realOldTail->next, realNewNode, oldTailNext))
				{
					newNode = GetUniqu64ePointer(realNewNode, cnt);

					// CAS 2 ����
					InterlockedCompareExchangePointer((PVOID*)&_tail, newNode, oldTail);

					break;
				}
				else
				{
					// backup tail ����� next�� nullptr�� �� Ȯ���ϰ� �����ߴµ�, �� ���̿� ���ڱ� nullptr�� �ƴϰ� �� ���?
					// - ���� �߻� ���
					// 1. tail ���� tail ����� next ���
					// 2. ���ؽ�Ʈ ����Ī�Ǿ� Enqueue�� Dequeue �۾��� ������ �����ϴٺ���
					// backup �ߴ� tail ���� ���� head ����� ��ġ�� ����
					// 3. CAS 1�� ���������� CAS 2 �۾��� ���� 
					// 4. CAS 2 ���� ���� _tail�� ������ ���� ������ �ȵǾ� �־� ���ѷ���
					// -> ù��° CAS�� �����ϸ� enqueue�� ������ ��
					// _tail�� ������ ���� �����ϴ� CAS �۾��� ���� �ذ�
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
				// backup �ߴ� tail ����� next�� null�� �ƴϱ� ������ ���ѷ��� �߻�
				// ���� ���� _tail�� ������ ���� �����ϴ� CAS �۾��� ���� �ذ�
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
			// head, next ���
			oldHead = _head;
			raalOldHead = Get64Pointer(oldHead);

			cnt = ((uint64_t)oldHead >> MAX_COUNT_VALUE) + 1;
			oldHeadNext = raalOldHead->next;

			// �߻��� �� �ִ� ����?
			// head�� ����� ��, ���ؽ�Ʈ ����Ī�� �Ͼ�� �ٸ� �����尡 �ش� ��带 ��ť, ��ť�Ͽ�
			// ���� �ּ��� ���� ���Ҵ�� ��, head�� next�� nullptr�� ������ (����� ������ ���ʰ� ������ ������)
			// �ٽ� ���ؽ�Ʈ ����Ī�Ǿ� �Ʊ��� ������� ���ƿ��� ��� head�� next�� nullptr�� ����Ǿ� �ִ� ����
			// -> �� ��, size�� 0�� �ƴϱ� �빮�� �׷� ��쿡�� continue�� �� ��Ȳ skip �Ͽ� ���� loop�� �ѱ�
			// size�� 0�̸� ������ ��尡 1������ �̸� ��ť�Ϸ��� ��Ȳ�̾��� ����
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

					// Enqueue ���� �߻��ϴ� ������ ���� �����̹Ƿ�
					// �߰����� CAS �۾��� ���� ���� tail ��� ��ġ�� �ٽ� �����Ͽ� �ذ�
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