#include <stdbool.h>
#include "core.h"

lockfreeSpscQueue* initQueue (int capacity) { // returns a pointer to the struct representing the queue

    // allocating the ring buffer
    lockfreeSpscQueue* ptrToQueue = malloc (sizeof(lockfreeSpscQueue));

    // round up capacity to the next power of 2
    int powerOf2Capacity = getNextPowerOf2(1, capacity);

    void** ptrToRingBuffer = malloc (sizeof(void*) * powerOf2Capacity);

    // initializing the struct
    ptrToQueue -> capacity = powerOf2Capacity; // Assigns the stack-allocated address of readOnlyVar
    ptrToQueue -> mask = powerOf2Capacity - 1; // mask for bitwise modulus operation
    
    ptrToQueue -> ptrToRingBuffer = ptrToRingBuffer;
    
    atomic_store_explicit(&(ptrToQueue -> head), 0, memory_order_relaxed);
    atomic_store_explicit(&(ptrToQueue -> tail), 0, memory_order_relaxed);

    ptrToQueue -> cachedHead = 0;
    ptrToQueue -> cachedTail = 0;

    return ptrToQueue;
}

int getNextPowerOf2 (int current, int target) {
    if (current < target) {
        return getNextPowerOf2(current * 2, target);
    } else {
        return current;
    }
}


bool tryPushToQueue (lockfreeSpscQueue* ptrToQueue, void* ptrToItem) {
	
	// Fetching the necessary attributes from lockfreeSpscQueue
	int head = atomic_load_explicit(&(ptrToQueue -> head), memory_order_relaxed);
	int cachedTail = ptrToQueue -> cachedTail;
	int incrementedHead = (head + 1) & (ptrToQueue -> mask); // bitwise modulus operation
	
	if (incrementedHead == cachedTail) { // queue stale-ly full
		
		ptrToQueue -> cachedTail = atomic_load_explicit(&(ptrToQueue -> tail), memory_order_acquire);
		if (incrementedHead == ptrToQueue -> cachedTail) { // queue freshly full
			return 0;
		}
	}
	
	// 1. write item to buffer
	(ptrToQueue -> ptrToRingBuffer)[head] = ptrToItem;
	
	// 2. increment head
	atomic_store_explicit(&(ptrToQueue -> head), incrementedHead, memory_order_release);
	
	return 1;
}


bool tryPopFromQueue (lockfreeSpscQueue* ptrToQueue, void** ptrToPutPoppedItem) {
	
	// Fetching the necessary attributes from lockfreeSpscQueue
	int tail = atomic_load_explicit(&(ptrToQueue -> tail), memory_order_relaxed);
	int cachedHead = ptrToQueue -> cachedHead;
	
	if (cachedHead == tail) { // queue stale-ly empty
		
		ptrToQueue -> cachedHead = atomic_load_explicit(&(ptrToQueue -> head), memory_order_acquire);
		
		if (ptrToQueue -> cachedHead == tail) { // queue freshly empty
			return 0;
		}
		
	}

	// 1. read item from buffer
	 *ptrToPutPoppedItem = (ptrToQueue -> ptrToRingBuffer)[tail];
	
	// 2. increment tail
	atomic_store_explicit(&(ptrToQueue -> tail), ((tail + 1) & (ptrToQueue -> mask)), memory_order_release);
	
	return 1;
}


bool isLockFree (lockfreeSpscQueue* ptrToQueue) {
	return atomic_is_lock_free(&(ptrToQueue -> head)) && atomic_is_lock_free(&(ptrToQueue -> tail));
}
