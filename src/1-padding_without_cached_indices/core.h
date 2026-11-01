#include <stdbool.h>
#include <stdatomic.h>
#include <stdalign.h>

#ifndef CACHE_LINE_SIZE
#  define CACHE_LINE_SIZE 64
#endif

typedef struct {
    alignas(CACHE_LINE_SIZE) struct {
        int capacity;
        void** ptrToRingBuffer;
    };
    alignas(CACHE_LINE_SIZE) atomic_int head;
    alignas(CACHE_LINE_SIZE) atomic_int tail;
} lockfreeSpscQueue;

lockfreeSpscQueue* initQueue (int capacity);
bool tryPushToQueue (lockfreeSpscQueue* ptrToQueue, void* ptrToItem);
bool tryPopFromQueue (lockfreeSpscQueue* ptrToQueue, void** ptrToPutPoppedItem);
bool isLockFree (lockfreeSpscQueue* ptrToQueue);
void destroyQueue (lockfreeSpscQueue* ptrToQueue);
