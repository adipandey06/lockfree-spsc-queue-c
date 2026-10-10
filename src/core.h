#include <stdbool.h>
#include <stdatomic.h>

typedef struct {
    int capacity;
    atomic_int tail;
    atomic_int head;    
    void** ptrToRingBuffer;
} lockfreeSpscQueue;


lockfreeSpscQueue* initQueue (int capacity);

bool tryPushToQueue (lockfreeSpscQueue* ptrToQueue, void* ptrToItem);

bool tryPopFromQueue (lockfreeSpscQueue* ptrToQueue, void** ptrToPutPoppedItem);

void destroyQueue (lockfreeSpscQueue* ptrToQueue);