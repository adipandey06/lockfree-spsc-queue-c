#include <stdbool.h>
#include <stdlib.h>
#include "core.h"

lockfreeSpscQueue* initQueue (int capacity) { // returns a pointer to the struct representing the queue

    // allocating the ring buffer
    lockfreeSpscQueue* ptrToQueue = malloc (sizeof(lockfreeSpscQueue));
    void** ptrToRingBuffer = malloc (sizeof(void*) * capacity);

    // initializing the struct
    ptrToQueue -> capacity = capacity; // Assigns the stack-allocated address of readOnlyVar
    ptrToQueue -> ptrToRingBuffer = ptrToRingBuffer;
    
    atomic_store_explicit(&(ptrToQueue -> head), 0, memory_order_seq_cst);
    atomic_store_explicit(&(ptrToQueue -> tail), 0, memory_order_seq_cst);

    ptrToQueue -> cachedHead = 0;
    ptrToQueue -> cachedTail = 0;

    return ptrToQueue;
}

bool tryPushToQueue (lockfreeSpscQueue* ptrToQueue, void* ptrToItem) {
	
	// Fetching the necessary attributes from lockfreeSpscQueue
	int head = atomic_load_explicit(&(ptrToQueue -> head), memory_order_seq_cst);
	int cachedTail = ptrToQueue -> cachedTail;
	int incrementedHead = (head + 1) % (ptrToQueue -> capacity);
	
	if (incrementedHead == cachedTail) { // queue stale-ly full
		
		ptrToQueue -> cachedTail = atomic_load_explicit(&(ptrToQueue -> tail), memory_order_seq_cst);
		if (incrementedHead == ptrToQueue -> cachedTail) { // queue freshly full
			return 0;
		}
	}
	
	// 1. write item to buffer
	(ptrToQueue -> ptrToRingBuffer)[head] = ptrToItem;
	
	// 2. increment head
	atomic_store_explicit(&(ptrToQueue -> head), incrementedHead, memory_order_seq_cst);
	
	return 1;
}


bool tryPopFromQueue (lockfreeSpscQueue* ptrToQueue, void** ptrToPutPoppedItem) {
	
	// Fetching the necessary attributes from lockfreeSpscQueue
	int tail = atomic_load_explicit(&(ptrToQueue -> tail), memory_order_seq_cst);
	int cachedHead = ptrToQueue -> cachedHead;
	
	if (cachedHead == tail) { // queue stale-ly empty
		
		ptrToQueue -> cachedHead = atomic_load_explicit(&(ptrToQueue -> head), memory_order_seq_cst);
		
		if (ptrToQueue -> cachedHead == tail) { // queue freshly empty
			return 0;
		}
		
	}

	// 1. read item from buffer
	 *ptrToPutPoppedItem = (ptrToQueue -> ptrToRingBuffer)[tail];
	
	// 2. increment tail
	atomic_store_explicit(&(ptrToQueue -> tail), ((tail + 1) % (ptrToQueue -> capacity)), memory_order_seq_cst);
	
	return 1;
}


bool isLockFree (lockfreeSpscQueue* ptrToQueue) {
	return atomic_is_lock_free(&(ptrToQueue -> head)) && atomic_is_lock_free(&(ptrToQueue -> tail));
}
