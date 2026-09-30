#include <stdbool.h>
#include "core.h"

lockfreeSpscQueue* initQueue (int capacity) { // returns a pointer to the struct representing the queue

    // allocating the ring buffer 
    lockfreeSpscQueue* ptrToQueue = malloc (sizeof(lockfreeSpscQueue));
    void** ptrToRingBuffer = malloc (sizeof(void*) * capacity);



    // initializing the struct
    ptrToQueue->ptrToRingBuffer = ptrToRingBuffer; // equivalent to (*ptrToQueue).ringBuffer = ptrToRingBuffer;
    ptrToQueue->capacity = capacity;
    ptrToQueue->head = 0;
    ptrToQueue->tail = 0;

    return ptrToQueue;
}



bool tryPushToQueue (lockfreeSpscQueue* ptrToQueue, void* ptrToItem) {
    int head = ptrToQueue -> head;
    int tail = ptrToQueue -> tail;
    int cap = ptrToQueue -> capacity;
    void** ptrToRingBuffer = ptrToQueue -> ptrToRingBuffer;

    
    int incrementedHead = ((head + 1) % cap);


    if ( incrementedHead == tail) { // ringBuffer full
        return 0;
    } else {
        ptrToRingBuffer[head] = ptrToItem; // equivalent to *(ptrToRingBuffer + head)
        ptrToQueue -> head = incrementedHead;
        return 1;
    }
};



bool tryPopFromQueue (lockfreeSpscQueue* ptrToQueue, void** ptrToPutPoppedItem) {
    int head = ptrToQueue -> head;
    int tail = ptrToQueue -> tail;
    int cap = ptrToQueue -> capacity;
    void** ptrToRingBuffer = ptrToQueue -> ptrToRingBuffer;


    int incrementedTail = ((tail + 1) % cap);

    if (head == tail) { // ringBuffer empty
        return 0;
    } else {
        *ptrToPutPoppedItem = ptrToRingBuffer[head];
        ptrToQueue -> tail = incrementedTail;
        return 1;
    }
};




void destroyQueue (lockfreeSpscQueue* ptrToQueue) {
    
    // freeing the ring buffer
    free (ptrToQueue->ptrToRingBuffer);
    
    // freeing the struct
    free (ptrToQueue);
    return;
}

