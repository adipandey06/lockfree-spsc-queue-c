#define _GNU_SOURCE // Enables pthread_barrier_t and pthread_setaffinity_np
#include "core.h" // using compiler include path for each queue implementation
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>

#define DEFAULT_SAMPLE_INTERVAL 4096     // 8388608 / 4096 = 2048 samples
#define DEFAULT_NUM_ELEMENTS (1 << 23)   // 8388608 elements
#define DEFAULT_PRODUCER_CORE 0
#define DEFAULT_CONSUMER_CORE 1

// Upper bound on warm-up pushes; guards against a queue that never reports full.
#define WARMUP_CAP (1 << 22)

// Usage: ./harness [sampleInterval] [numElements] [producerCore] [consumerCore]
// Producer/consumer cores must be in the process's allowed CPU set and should be
// on different physical cores (not hyperthread siblings): check `lscpu -e`.

typedef struct {
    lockfreeSpscQueue* ptrToQueue;
    pthread_barrier_t* startBarrier;
    void** ptrToElementsToPush;
    int numElementsToPush;
    int sampleInterval;
    int coreId;
    struct timespec* ptrToPushTimestamps;
    struct timespec* ptrToStartTimestamp;
} ProducerArgs;

typedef struct {
    lockfreeSpscQueue* ptrToQueue;
    pthread_barrier_t* startBarrier;
    void** ptrToPutPoppedElements;
    int numElementsToPop;
    int sampleInterval;
    int coreId;
    struct timespec* ptrToPopTimestamps;
    struct timespec* ptrToEndTimestamp;
} ConsumerArgs;

void preFault(size_t totalBytes, void* pointerToMemory);
void warmUpQueue(lockfreeSpscQueue* queue, void* dummy, int numElements);
void* producerThread(void* pArgs);
void* consumerThread(void* cArgs);
int pinThreadToCore(pthread_t thread, int coreId);
int parseLong(const char* s, long minVal, long maxVal, long* out);

int main(int argc, char* argv[]) {
    long sampleIntervalL = DEFAULT_SAMPLE_INTERVAL;
    long numElementsL    = DEFAULT_NUM_ELEMENTS;
    long producerCoreL   = DEFAULT_PRODUCER_CORE;
    long consumerCoreL   = DEFAULT_CONSUMER_CORE;

    if (argc > 1 && !parseLong(argv[1], 1, INT_MAX, &sampleIntervalL)) {
        fprintf(stderr, "Error: invalid sampleInterval '%s'\n", argv[1]);
        return 1;
    }
    if (argc > 2 && !parseLong(argv[2], 1, INT_MAX, &numElementsL)) {
        fprintf(stderr, "Error: invalid numElements '%s'\n", argv[2]);
        return 1;
    }
    if (argc > 3 && !parseLong(argv[3], 0, CPU_SETSIZE - 1, &producerCoreL)) {
        fprintf(stderr, "Error: invalid producer core '%s'\n", argv[3]);
        return 1;
    }
    if (argc > 4 && !parseLong(argv[4], 0, CPU_SETSIZE - 1, &consumerCoreL)) {
        fprintf(stderr, "Error: invalid consumer core '%s'\n", argv[4]);
        return 1;
    }

    int sampleInterval = (int)sampleIntervalL;
    int numElements    = (int)numElementsL;
    int producerCore   = (int)producerCoreL;
    int consumerCore   = (int)consumerCoreL;

    // The sampling check uses (i & (sampleInterval - 1)), which needs a power of two.
    if ((sampleInterval & (sampleInterval - 1)) != 0) {
        fprintf(stderr, "Error: sampleInterval must be a power of two (got %d)\n", sampleInterval);
        return 1;
    }
    if (producerCore == consumerCore) {
        fprintf(stderr, "Error: producer and consumer must use different cores\n");
        return 1;
    }

    int numSamples = (numElements + sampleInterval - 1) / sampleInterval;
    
    // Test every power of two from 2 through the smallest power of two that
    // can hold all elements, and the next two powers of two (if they fit).
    if (numElementsL > (1L << 29)) {
        fprintf(stderr, "Error: numElements is too large for an int-sized buffer\n");
        return 1;
    }
    const int maxBufferSize = (int)(1L << 30);
    int bufferSizes[sizeof(int) * CHAR_BIT - 2];
    int numBufferSizes = 0;
    long long greatestBufferSize = 2;
    while (greatestBufferSize < numElementsL) {
        greatestBufferSize <<= 1;
    }
    for (int i = 0; i < 2 && greatestBufferSize < maxBufferSize; i++) {
        greatestBufferSize <<= 1;
    }
    for (long long bufferSize = 2;
         bufferSize <= greatestBufferSize && bufferSize <= maxBufferSize;
         bufferSize <<= 1) {
        bufferSizes[numBufferSizes++] = (int)bufferSize;
    }

    // Allocate everything that doesn't depend on buffer size ONCE and reuse it.
    // Elements are slots in one int array (the queue only moves pointers).
    int*   values                 = malloc(sizeof(int) * (size_t)numElements);
    void** ptrToElementsToPush    = malloc(sizeof(void*) * (size_t)numElements);
    void** ptrToPutPoppedElements = malloc(sizeof(void*) * (size_t)numElements);
    struct timespec* ptrToPushTimestamps = malloc(sizeof(struct timespec) * (size_t)numSamples);
    struct timespec* ptrToPopTimestamps  = malloc(sizeof(struct timespec) * (size_t)numSamples);

    if (!values || !ptrToElementsToPush || !ptrToPutPoppedElements ||
        !ptrToPushTimestamps || !ptrToPopTimestamps) {
        fprintf(stderr, "Error: allocation failed\n");
        return 1;
    }

    for (int i = 0; i < numElements; i++) {
        values[i] = i;
        ptrToElementsToPush[i] = &values[i];
    }

    int failed = 0;

    for (int bufferIndex = 0; bufferIndex < numBufferSizes && !failed; bufferIndex++) {
        int minBufferSize = bufferSizes[bufferIndex];

        // 1. Initialize queue and touch its backing memory before timing
        lockfreeSpscQueue* ptrToQueue = initQueue(minBufferSize);
        if (ptrToQueue == NULL) {
            fprintf(stderr, "Error: initQueue(%d) failed\n", minBufferSize);
            failed = 1;
            break;
        }
        warmUpQueue(ptrToQueue, &values[0], numElements);

        // 2. Reset and pre-fault output arrays. Zeroing popped[] also stops stale
        //    matches from a previous run passing the correctness check.
        memset(ptrToPutPoppedElements, 0, sizeof(void*) * (size_t)numElements);
        memset(ptrToPushTimestamps, 0, sizeof(struct timespec) * (size_t)numSamples);
        memset(ptrToPopTimestamps, 0, sizeof(struct timespec) * (size_t)numSamples);
        preFault(sizeof(void*) * (size_t)numElements, ptrToPutPoppedElements);
        preFault(sizeof(struct timespec) * (size_t)numSamples, ptrToPushTimestamps);
        preFault(sizeof(struct timespec) * (size_t)numSamples, ptrToPopTimestamps);

        struct timespec startTimestamp = {0, 0};
        struct timespec endTimestamp   = {0, 0};

        // 3. Barrier + thread args
        pthread_barrier_t startBarrier;
        pthread_barrier_init(&startBarrier, NULL, 2);

        ProducerArgs producerArgs = {
            .ptrToQueue = ptrToQueue,
            .startBarrier = &startBarrier,
            .ptrToElementsToPush = ptrToElementsToPush,
            .numElementsToPush = numElements,
            .sampleInterval = sampleInterval,
            .coreId = producerCore,
            .ptrToPushTimestamps = ptrToPushTimestamps,
            .ptrToStartTimestamp = &startTimestamp
        };

        ConsumerArgs consumerArgs = {
            .ptrToQueue = ptrToQueue,
            .startBarrier = &startBarrier,
            .ptrToPutPoppedElements = ptrToPutPoppedElements,
            .numElementsToPop = numElements,
            .sampleInterval = sampleInterval,
            .coreId = consumerCore,
            .ptrToPopTimestamps = ptrToPopTimestamps,
            .ptrToEndTimestamp = &endTimestamp
        };

        // 4. Each thread pins ITSELF before the barrier, so no measured work runs unpinned
        pthread_t producerThreadId, consumerThreadId;
        if (pthread_create(&producerThreadId, NULL, producerThread, &producerArgs) != 0 ||
            pthread_create(&consumerThreadId, NULL, consumerThread, &consumerArgs) != 0) {
            fprintf(stderr, "Error: pthread_create failed\n");
            return 1; // a half-started pair would deadlock on the barrier
        }

        pthread_join(producerThreadId, NULL);
        pthread_join(consumerThreadId, NULL);
        pthread_barrier_destroy(&startBarrier);

        // 5. Per-run summary so each buffer size reports itself
        double elapsed = (double)(endTimestamp.tv_sec - startTimestamp.tv_sec) +
                         (double)(endTimestamp.tv_nsec - startTimestamp.tv_nsec) * 1e-9;
        fprintf(stderr, "buffer %6d: %8.3f s, %8.2f Mops/s\n",
                minBufferSize, elapsed, elapsed > 0 ? (double)numElements / elapsed / 1e6 : 0.0);

        // 6. Write CSV BEFORE verifying, so a failed run still leaves data for debugging
        char outputFilename[64];
        snprintf(outputFilename, sizeof(outputFilename),
                 "latency_throughput_buffer_%d.csv", minBufferSize);
        FILE* fp = fopen(outputFilename, "w");
        if (fp == NULL) {
            fprintf(stderr, "Error: Could not open %s for writing\n", outputFilename);
            failed = 1;
        } else {
            // Start/end are per-run constants, so write them once as a comment line
            // (pandas: read_csv(..., comment='#')).
            fprintf(fp, "# StartTimestamp=%ld.%09ld EndTimestamp=%ld.%09ld NumElements=%d SampleInterval=%d\n",
        startTimestamp.tv_sec, startTimestamp.tv_nsec,
        endTimestamp.tv_sec, endTimestamp.tv_nsec,
        numElements, sampleInterval);
            fprintf(fp, "PushTimestamp,PopTimestamp\n");
            for (int s = 0; s < numSamples; s++) {
                fprintf(fp, "%ld.%09ld,%ld.%09ld\n",
                        ptrToPushTimestamps[s].tv_sec, ptrToPushTimestamps[s].tv_nsec,
                        ptrToPopTimestamps[s].tv_sec, ptrToPopTimestamps[s].tv_nsec);
            }
            fclose(fp);
        }

        // 7. Correctness check
        for (int i = 0; i < numElements; i++) {
            if (ptrToElementsToPush[i] != ptrToPutPoppedElements[i]) {
                fprintf(stderr, "Error: buffer %d: element at index %d does not match\n",
                        minBufferSize, i);
                failed = 1;
                break;
            }
        }

        destroyQueue(ptrToQueue);
    }

    free(values);
    free(ptrToElementsToPush);
    free(ptrToPutPoppedElements);
    free(ptrToPushTimestamps);
    free(ptrToPopTimestamps);

    return failed ? 1 : 0;
}

int parseLong(const char* s, long minVal, long maxVal, long* out) {
    char* end = NULL;
    errno = 0;
    long v = strtol(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0' || v < minVal || v > maxVal) {
        return 0;
    }
    *out = v;
    return 1;
}

int pinThreadToCore(pthread_t thread, int coreId) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(coreId, &cpuset);
    int rc = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    if (rc != 0) {
        fprintf(stderr, "Warning: failed to pin thread to core %d: %s "
                        "(is it in the allowed CPU set?)\n", coreId, strerror(rc));
    }
    return rc;
}

void* producerThread(void* pArgs) {
    ProducerArgs* args = (ProducerArgs*)pArgs;

    pinThreadToCore(pthread_self(), args->coreId);
    pthread_barrier_wait(args->startBarrier);
    clock_gettime(CLOCK_MONOTONIC, args->ptrToStartTimestamp);

    int timestampIndex = 0;

    for (int i = 0; i < args->numElementsToPush; i++) {
        if ((i & (args->sampleInterval - 1)) == 0) {
            clock_gettime(CLOCK_MONOTONIC, &args->ptrToPushTimestamps[timestampIndex++]);
        }

        while (!tryPushToQueue(args->ptrToQueue, args->ptrToElementsToPush[i])) {
            // busy wait
        }
        
    }

    /*
    REMARK: Placing the pushTimestamp before the push operations begins measures the total system latency (time waiting for a free slot,
    + time sitting in the ring buffer + pop time). Placing it after the push operation measures transit latency i.e.
    how long an item spends inside the queue once admitted.
    */
    return NULL;
}

void* consumerThread(void* cArgs) {
    ConsumerArgs* args = (ConsumerArgs*)cArgs;

    pinThreadToCore(pthread_self(), args->coreId);
    pthread_barrier_wait(args->startBarrier);

    int timestampIndex = 0;

    for (int i = 0; i < args->numElementsToPop; i++) {
        while (!tryPopFromQueue(args->ptrToQueue, &(args->ptrToPutPoppedElements[i]))) {
            // busy wait
        }
        if ((i & (args->sampleInterval - 1)) == 0) {
            clock_gettime(CLOCK_MONOTONIC, &args->ptrToPopTimestamps[timestampIndex++]);
        }
    }

    clock_gettime(CLOCK_MONOTONIC, args->ptrToEndTimestamp);
    return NULL;
}

// Single-threaded fill-then-drain so every page of the queue's backing buffer is touched
// before timing starts. Pushes until the queue reports full (or WARMUP_CAP), then pops
// everything back out. The queue ends up empty.
void warmUpQueue(lockfreeSpscQueue* queue, void* dummy, int numElements) {
    long pushed = 0;
    while (pushed < numElements && tryPushToQueue(queue, dummy)) {
        pushed++;
    }
    void* sink = NULL;
    for (long i = 0; i < pushed; i++) {
        tryPopFromQueue(queue, &sink);
    }
}

void preFault(size_t totalBytes, void* pointerToMemory) {
    if (totalBytes == 0 || pointerToMemory == NULL) {
        return;
    }

    long pageSize = sysconf(_SC_PAGESIZE);
    volatile char* bytePtr = (volatile char*)pointerToMemory;

    for (size_t i = 0; i < totalBytes; i += (size_t)pageSize) {
        bytePtr[i] = 0;
    }
    bytePtr[totalBytes - 1] = 0;
}