#include <stdbool.h>
#include <stdlib.h>
#include "core.h"

lockfreeSpscQueue* initQueue (int capacity) {
    lockfreeSpscQueue* ptrToQueue = malloc (sizeof(lockfreeSpscQueue));
    void** ptrToRingBuffer = malloc (sizeof(void*) * capacity);

    ptrToQueue->ptrToRingBuffer = ptrToRingBuffer;
    ptrToQueue->capacity = capacity;
    atomic_store_explicit(&(ptrToQueue -> head), 0, memory_order_relaxed);
    atomic_store_explicit(&(ptrToQueue -> tail), 0, memory_order_relaxed);

    return ptrToQueue;
}

bool tryPushToQueue (lockfreeSpscQueue* ptrToQueue, void* ptrToItem) {
    int head = atomic_load_explicit(&(ptrToQueue -> head), memory_order_relaxed);
    int tail = atomic_load_explicit(&(ptrToQueue -> tail), memory_order_acquire);
    int cap = ptrToQueue -> capacity;
    void** ptrToRingBuffer = ptrToQueue -> ptrToRingBuffer;
    int incrementedHead = ((head + 1) % cap);

    if (incrementedHead == tail) {
        return 0;
    }

    ptrToRingBuffer[head] = ptrToItem;
    atomic_store_explicit(&(ptrToQueue -> head), incrementedHead, memory_order_release);
    return 1;
}

bool tryPopFromQueue (lockfreeSpscQueue* ptrToQueue, void** ptrToPutPoppedItem) {
    int head = atomic_load_explicit(&(ptrToQueue -> head), memory_order_acquire);
    int tail = atomic_load_explicit(&(ptrToQueue -> tail), memory_order_relaxed);
    int cap = ptrToQueue -> capacity;
    void** ptrToRingBuffer = ptrToQueue -> ptrToRingBuffer;
    int incrementedTail = ((tail + 1) % cap);

    if (head == tail) {
        return 0;
    }

    *ptrToPutPoppedItem = ptrToRingBuffer[tail];
    atomic_store_explicit(&(ptrToQueue -> tail), incrementedTail, memory_order_release);
    return 1;
}

bool isLockFree (lockfreeSpscQueue* ptrToQueue) {
    return atomic_is_lock_free(&(ptrToQueue -> head)) &&
           atomic_is_lock_free(&(ptrToQueue -> tail));
}

void destroyQueue (lockfreeSpscQueue* ptrToQueue) {
    free (ptrToQueue->ptrToRingBuffer);
    free (ptrToQueue);
}
