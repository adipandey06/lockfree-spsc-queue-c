#include <stdbool.h>
#include <stdlib.h>
#include "core.h"

lockfreeSpscQueue* initQueue (int capacity) { // returns a pointer to the struct representing the queue

    // allocating the ring buffer 
    lockfreeSpscQueue* ptrToQueue = malloc (sizeof(lockfreeSpscQueue));
    void** ptrToRingBuffer = malloc (sizeof(void*) * capacity);


    // initializing the struct
    ptrToQueue->ptrToRingBuffer = ptrToRingBuffer; // equivalent to (*ptrToQueue).ringBuffer = ptrToRingBuffer;
    ptrToQueue->capacity = capacity;
    atomic_store_explicit(&(ptrToQueue -> head), 0, memory_order_seq_cst);
    atomic_store_explicit(&(ptrToQueue -> tail), 0, memory_order_seq_cst);

    return ptrToQueue;
}



bool tryPushToQueue (lockfreeSpscQueue* ptrToQueue, void* ptrToItem) {
    int head = atomic_load_explicit(&(ptrToQueue -> head), memory_order_seq_cst);
    int tail = atomic_load_explicit(&(ptrToQueue -> tail), memory_order_seq_cst);
    int cap = ptrToQueue -> capacity;
    void** ptrToRingBuffer = ptrToQueue -> ptrToRingBuffer;

    
    int incrementedHead = ((head + 1) % cap);


    if ( incrementedHead == tail) { // ringBuffer full
        return 0;
    } else {
        ptrToRingBuffer[head] = ptrToItem; // equivalent to *(ptrToRingBuffer + head)
        atomic_store_explicit(&(ptrToQueue -> head), incrementedHead, memory_order_seq_cst);
        return 1;
    }
};



bool tryPopFromQueue (lockfreeSpscQueue* ptrToQueue, void** ptrToPutPoppedItem) {
    int head = atomic_load_explicit(&(ptrToQueue -> head), memory_order_seq_cst);
    int tail = atomic_load_explicit(&(ptrToQueue -> tail), memory_order_seq_cst);
    int cap = ptrToQueue -> capacity;
    void** ptrToRingBuffer = ptrToQueue -> ptrToRingBuffer;


    int incrementedTail = ((tail + 1) % cap);

    if (head == tail) { // ringBuffer empty
        return 0;
    } else {
        *ptrToPutPoppedItem = ptrToRingBuffer[tail];
        atomic_store_explicit(&(ptrToQueue -> tail), incrementedTail, memory_order_seq_cst);
        return 1;
    }
};


bool isLockFree (lockfreeSpscQueue* ptrToQueue) {
    return atomic_is_lock_free(&(ptrToQueue -> head)) && atomic_is_lock_free(&(ptrToQueue -> tail));
}




void destroyQueue (lockfreeSpscQueue* ptrToQueue) {
    
    // freeing the ring buffer
    free (ptrToQueue->ptrToRingBuffer);
    
    // freeing the struct
    free (ptrToQueue);
    return;
}