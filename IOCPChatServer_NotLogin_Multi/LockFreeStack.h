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

	// unique pointer (64bit �𵨿��� ���� ������ �ʴ� ���� 17bit�� ����Ͽ� ������ ������ �� ����
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
		// LockFreeFreeList���� ��� ����
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
			// backup�� top node (�� ���̿� �ٸ� ������� ���� top ��ġ�� �ٲ� ���� �ֱ⿡ backup)
			oldTop = pTop;

			// top node�� ���� 17bit�� ���� ���������� ��
			cnt = ((uint64_t)oldTop >> MAX_COUNT_VALUE) + 1;

			// backup�� top node�� ���� �ּ� ��
			realOldTop = Get64Pointer(oldTop);

			// �����Ϸ��� node�� next ��ġ�� backup�� top node�� ����
			newRealNode->next = realOldTop;

			// x64 ȯ�濡�� ���� ���ǰ� ���� �ʴ� ���� 17bit�� ���� ����� �ּ� ���� | �����Ͽ� ���� �� ����
			newNode = GetUniqu64ePointer(newRealNode, cnt);

			// CAS ����
			// backup�� top node�� ���� top node�� ���ٸ� �� ���̿� ���ؽ�Ʈ ����Ī�Ͽ� �ٸ� �����忡�� top ��尡 ������ �ʾҴٴ� �ǹ�
			// -> �����Ϸ��� ���� ����� ���� ������ top ����
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
			// backup�� top node (�� ���̿� �ٸ� ������� ���� top ��ġ�� �ٲ� ���� �ֱ⿡ backup)
			oldTop = pTop;

			// backup�� top node�� ���� �ּ� ��
			realOldTop = Get64Pointer(oldTop);

			if (realOldTop == nullptr)
				return false;

			// top node�� ���� 17bit�� ���� ���������� ��
			cnt = ((uint64_t)oldTop >> MAX_COUNT_VALUE) + 1;

			// x64 ȯ�濡�� ���� ���ǰ� ���� �ʴ� ���� 17bit�� ���� ����� �ּ� ���� | �����Ͽ� ���� �� ����
			// backup�� top ����� next�� ���� �� ���� (top�� �� ���)
			realOldTopNext = realOldTop->next;
			oldTopNext = GetUniqu64ePointer(realOldTopNext, cnt);

			// CAS ����
			// backup�� top node�� ���� top node�� ���ٸ� �� ���̿� ���ؽ�Ʈ ����Ī�Ͽ� �ٸ� �����忡�� top ��尡 ������ �ʾҴٴ� �ǹ�
			// -> top�� pop�� ����� next�� ����
			if ((PVOID)oldTop == InterlockedCompareExchangePointer((PVOID*)&pTop, oldTopNext, oldTop))
			{
				// pop�� ����� �����͸� (���� ����ڰ� �����ϴ� ��) ����
				*_value = realOldTop->data;

				// LockFreeFreeList�� pop�� ��� ��ȯ (delete�� �� �ƴ϶� LockFreeFreeList�� ����)
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
