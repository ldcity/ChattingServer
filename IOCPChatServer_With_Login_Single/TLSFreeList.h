#ifndef __TLS_FREE_LIST_VER2__
#define __TLS_FREE_LIST_VER2__

/*
* TLSFreeList Class
- 생성자
rootPool, bucketPool(빈 버킷) 동적할당
TLSAlloc() -> TLS_OUT_OF_INDEXES 예외 확인

- 소멸자
TLSIndex에 버킷 포인터 확인
버킷 포인터 존재할 경우, 해당 버킷 내에 남아있는 노드 확인 (remain)
빈 버킷일 경우, bucketPool에 반환


- Alloc


*/

#include "LockFreeStack.h"

#define BUCKETMAX				20			// 버킷 개수 (pTop)
#define NODEMAX					10000		// 버킷 당 들어있는 노드 개수

struct Node;
class Bucket;

template <typename DATA>
class TLSObjectPool
{
	struct Node
	{
		DATA object;
		Node* next = nullptr;
	};

	class Bucket
	{
	public:
		Node* pTop;								// 버킷 내 top Node
		long size;								// 버킷 내 존재하는 노드 개수

	private:
		long maxCount;							// 버킷 내 존재할 수 있는 최대 노드 개수
		long allocCount;						// 버킷 내 노드 할당 개수
		long freeCount;							// 버킷 내 노드 반환 개수
		bool m_bPlacementNew;
		LockFreeStack<Node*>* _rootPool;

	public:

		// 해당 개수만큼의 버킷 내 노드를 풀에서 갖고옴
		Bucket(int iNodeNum, bool _placementNew, LockFreeStack<Node*>* rootPool) :
			maxCount(iNodeNum), size(iNodeNum), allocCount(0), freeCount(0), pTop(nullptr),
			m_bPlacementNew(_placementNew), _rootPool(rootPool)
		{
			if (size != 0)
			{
				_rootPool->Pop(&pTop);
			}
		}

		inline DATA* Alloc()
		{
			// top 노드를 빼옴
			Node* node = pTop;
			pTop = pTop->next;

			//allocCount++;
			size--;

			return &node->object;
		}

		inline bool Free(DATA* pData)
		{
			size++;
			//freeCount++;

			Node* node = (Node*)pData;

			// 버킷에 반환할 노드 집어넣음
			node->next = pTop;
			pTop = node;

			return true;
		}

		// 빈 버킷이 됐는데 rootPool에 노드가 없어 새로 할당을 하려 할 경우
		inline void newAlloc()
		{
			size = maxCount;
			//allocCount = 0;
			//freeCount = 0;

			// true면 실제로 사용자가 Alloc함수 호출할 때마다 생성자 호출해줘야 하므로 지금은 X
		/*	if (m_bPlacementNew)
			{*/
			for (int i = 0; i < maxCount; i++)
			{
				Node* node = (Node*)_aligned_malloc(sizeof(Node), alignof(Node));

				node->next = pTop;
				pTop = node;
			}
			/*}
			else
			{
				for (int i = 0; i < maxCount; i++)
				{
					Node* node = new Node;

					node->next = pTop;
					pTop = node;
				}
			}*/
		}

		//inline void Setting(Node* node, long iNodeCount)
		//{
		//	pTop = node;
		//	size = iNodeCount;
		//}

		//inline long GetSize() { return size; }

		inline void Clear()
		{
			pTop = nullptr;
			size = 0;
			//allocCount = 0;
			//freeCount = 0;
		}

		~Bucket()
		{
			if (pTop != nullptr)
			{
				Node* node;
				while (pTop)
				{
					node = pTop;
					pTop = pTop->next;

					// 소멸자 호출해야하나 확인해야함
					if (m_bPlacementNew)
						//free(node);
						_aligned_free(node);
					else
						delete node;
				}
			}
		}

		inline Node* GetTop() { return pTop; }
	};

	// TLS pointer (할당 & 해제를 빈번하게 하면서 발생하는 오버헤드 감소)
	struct TLS_Bucket
	{
		Bucket* bucket1;			// 첫번째 버킷
		Bucket* bucket2;			// 두번째 버킷
	};

private:
	alignas(64) __int64 objectAllocCount;				// 사용자에게 할당해준 노드 개수
	alignas(64) __int64 objectFreeCount;				// 사용자가 반환한 노드 개수
	alignas(64) __int64 objectUseCount;					// 현재 사용하고 있는 노드 개수
	alignas(64) __int64 m_iCapacity;					// TLS 풀 내부 전체 용량
	alignas(64) DWORD tlsIndex;							// tls Index

	int defaultNodeCount;								// 처음 지정한 노드 개수
	int defaultBucketCount;								// 처음 지정한 버킷 개수
	bool m_bPlacementNew;								// placement new 작동 여부

	LockFreeStack<Node*>* rootPool;						// root Pool
public:
	TLSObjectPool(int iNodeNum = NODEMAX, bool bPlacementNew = false, int iBucketNum = BUCKETMAX)
		: m_bPlacementNew(bPlacementNew), m_iCapacity(0),
		objectAllocCount(0), objectFreeCount(0), defaultNodeCount(iNodeNum), defaultBucketCount(iBucketNum)
	{
		// 버킷 개수만큼 스택프리리스트 노드 생성
		rootPool = new LockFreeStack<Node*>(defaultBucketCount, bPlacementNew);
		Init();

		tlsIndex = TlsAlloc();

		// Exception
		if (tlsIndex == TLS_OUT_OF_INDEXES)
			CRASH();
	}

	~TLSObjectPool()
	{
		// 해당 스레드만 종료되어 해당 TLS 버킷에 남아있는 리소스 정리
		// placement new flag에 따라 소멸자 호출
		// tls index 해제
		// 생성한 메모리 풀 해제

		TLS_Bucket* buckets = (TLS_Bucket*)TlsGetValue(tlsIndex);

		if (buckets != nullptr)
		{
			delete buckets->bucket1;
			delete buckets->bucket2;
		}

		Node* top;
		while (rootPool->Pop(&top))
		{
			Node* cur;
			while (top)
			{
				cur = top;
				top = cur->next;
				if (!m_bPlacementNew)
					cur->object.~DATA();

				free(cur);
			}

			/*if (m_bPlacementNew)
			{

			}
			else
			{
				while (top)
				{
					cur = top;
					top = cur->next;
					delete cur;
				}
			}*/
		}

		TlsFree(tlsIndex);

		delete rootPool;
	}


	inline void Init()
	{
		if (m_bPlacementNew)
		{
			for (int i = 0; i < defaultBucketCount; i++)
			{
				Node* pTop = nullptr;
				// rootPool에 pTop 포인터 stack으로 연결 (각 pTop 포인터는 버킷 단위 stack)
				for (int j = 0; j < defaultNodeCount; j++)
				{
					Node* node = (Node*)_aligned_malloc(sizeof(Node), alignof(Node));

					node->next = pTop;
					pTop = node;
				}

				rootPool->Push(pTop);
			}
		}
		else
		{
			for (int i = 0; i < defaultBucketCount; i++)
			{
				Node* pTop = nullptr;

				// rootPool에 pTop 포인터 stack으로 연결 (각 pTop 포인터는 버킷 단위 stack)
				for (int j = 0; j < defaultNodeCount; j++)
				{
					Node* node = new Node;

					node->next = pTop;
					pTop = node;
				}

				rootPool->Push(pTop);
			}
		}

		m_iCapacity = defaultBucketCount * defaultNodeCount;
	}

	__int64 GetCapacity() { return m_iCapacity; }
	__int64 GetObjectAllocCount() { return objectAllocCount; }
	__int64 GetObjectFreeCount() { return objectFreeCount; }
	__int64 GetObjectUseCount() { return objectUseCount; }

	DATA* Alloc()
	{
		TLS_Bucket* buckets = (TLS_Bucket*)TlsGetValue(tlsIndex);

		// tlsIndex가 비어있다면 노드 버킷을 할당하지 못한 상태
		// 공용 메모리 풀에서 노드 버킷 가져옴
		if (buckets == nullptr)
		{
			buckets = new TLS_Bucket;

			buckets->bucket1 = new Bucket(defaultNodeCount, m_bPlacementNew, rootPool);
			buckets->bucket2 = new Bucket(0, m_bPlacementNew, rootPool);

			TlsSetValue(tlsIndex, buckets);

			//// 한 스레드 당 버킷 2개 갖고 있으므로 할당 용량도 2배
			//InterlockedAdd64(&m_iCapacity, defaultNodeCount * 2);
		}

		// 두번째 버킷 먼저 확인
		Bucket* bucket = buckets->bucket2;

		// 1. 두번째 버킷이 비어있을 경우
		if (bucket->size == 0)
		{
			// 첫번째 버킷 확인
			bucket = buckets->bucket1;

			// 2. 첫번째 버킷도 비어있을 경우
			if (bucket->size == 0)
			{
				Node* pTop = nullptr;

				// rootPool에서 노드 가져옴
				if (rootPool->Pop(&pTop))
				{
					// rootPool에서 가져온 노드를 버킷에 할당
					//bucket->Setting(pTop, defaultNodeCount);
					bucket->pTop = pTop;
					bucket->size = defaultNodeCount;
					//size = iNodeCount;
				}
				// rootPool에도 없을 경우
				else
				{
					// 버킷에 노드 새로 할당 (버킷에 노드를 모두 채움)
					bucket->newAlloc();
				}
			}
		}

		DATA* object = bucket->Alloc();

		// 할당할 때마다 생성자 호출
		if (m_bPlacementNew)
			new(object)DATA;

		InterlockedIncrement64(&objectAllocCount);
		InterlockedIncrement64(&objectUseCount);

		return object;
	}

	bool Free(DATA* pData)
	{
		TLS_Bucket* buckets = (TLS_Bucket*)TlsGetValue(tlsIndex);

		// bucket이 없을 경우 (처음 free)
		if (buckets == nullptr)
		{
			buckets = new TLS_Bucket;

			buckets->bucket1 = new Bucket(defaultNodeCount, m_bPlacementNew, rootPool);
			buckets->bucket2 = new Bucket(0, m_bPlacementNew, rootPool);

			TlsSetValue(tlsIndex, buckets);

			//// 한 스레드 당 버킷 2개 갖고 있으므로 할당 용량도 2배
			//InterlockedAdd64(&m_iCapacity, defaultNodeCount * 2);

		}

		//첫번째 버킷 먼저 확인
		Bucket* bucket = buckets->bucket1;

		// 첫번째 버킷이 가득 찼을 경우
		if (bucket->size == defaultNodeCount)
		{
			bucket = buckets->bucket2;

			// 두번째 버킷이 가득 찼을 경우
			if (bucket->size == defaultNodeCount)
			{
				// 두번째 버킷의 노드를 모두 root pool로 반환 -> top 포인터 넘김
				rootPool->Push(bucket->GetTop());

				// 두번째 버킷 비우기
				bucket->Clear();
			}
		}

		bucket->Free(pData);

		// 반환할 때마다 소멸자 호출
		if (m_bPlacementNew)
			pData->~DATA();

		InterlockedIncrement64(&objectFreeCount);
		InterlockedDecrement64(&objectUseCount);

		return true;
	}
};


#endif
