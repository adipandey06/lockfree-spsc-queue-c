#include <stdbool.h>
#include <stdatomic.h>
#include <stdalign.h>

#ifndef CACHE_LINE_SIZE
#  define CACHE_LINE_SIZE 64  // 64 bytes is standard for x86_64 and modern ARM, and for my machine. May change this to be a dynamically determined value at runtime later.
#endif


typedef struct {
    // Both live together in Cache Line 0 (read-only after init)
    alignas(CACHE_LINE_SIZE) struct { // anonymous struct
        int capacity;
        void** ptrToRingBuffer;
    };

    // Cache Line 1: Producer writes, Consumer reads
    alignas(CACHE_LINE_SIZE) atomic_int head;

    // Cache Line 2: Consumer writes, Producer reads
    alignas(CACHE_LINE_SIZE) atomic_int tail;

    // Cache Line 3: Producer thread-private
    alignas(CACHE_LINE_SIZE) int cachedTail;

    // Cache Line 4: Consumer thread-private
    alignas(CACHE_LINE_SIZE) int cachedHead;
} lockfreeSpscQueue;

lockfreeSpscQueue* initQueue (int capacity);

bool tryPushToQueue (lockfreeSpscQueue* ptrToQueue, void* ptrToItem);

bool tryPopFromQueue (lockfreeSpscQueue* ptrToQueue, void** ptrToPutPoppedItem);

bool isLockFree (lockfreeSpscQueue* ptrToQueue);

void destroyQueue (lockfreeSpscQueue* ptrToQueue);