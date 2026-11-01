#include <stdbool.h>
#include <stdatomic.h>

typedef struct {
    int capacity;
    atomic_int tail;
    atomic_int head;
    int cachedTail;
    int cachedHead;
    void** ptrToRingBuffer;
} lockfreeSpscQueue;

lockfreeSpscQueue* initQueue (int capacity);

bool tryPushToQueue (lockfreeSpscQueue* ptrToQueue, void* ptrToItem);

bool tryPopFromQueue (lockfreeSpscQueue* ptrToQueue, void** ptrToPutPoppedItem);

bool isLockFree (lockfreeSpscQueue* ptrToQueue);

void destroyQueue (lockfreeSpscQueue* ptrToQueue);
