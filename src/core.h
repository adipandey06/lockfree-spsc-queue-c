#include <stdbool.h>

typedef struct {
    const int capacity;
    int tail;
    int head;    
    const void** ptrToRingBuffer;
} lockfreeSpscQueue;


lockfreeSpscQueue* initQueue (int capacity);

bool tryPushToQueue (lockfreeSpscQueue* ptrToQueue, void* ptrToItem);

bool tryPopFromQueue (lockfreeSpscQueue* ptrToQueue, void** ptrToPutPoppedItem);

void destroyQueue (lockfreeSpscQueue* ptrToQueue);