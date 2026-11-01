#define _GNU_SOURCE // Enables pthread_barrier_t and pthread_setaffinity_np
#include "src/core.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>

#define DEFAULT_SAMPLE_INTERVAL 1000
#define DEFAULT_MIN_BUFFER_SIZE 2
#define DEFAULT_NUM_ELEMENTS (1 << 20)
#define PRODUCER_CORE 0
#define CONSUMER_CORE 1

typedef struct {
    lockfreeSpscQueue* ptrToQueue;
    pthread_barrier_t* startBarrier;
    void** ptrToElementsToPush;
    int numElementsToPush;
    int sampleInterval;
    struct timespec* ptrToPushTimestamps;
    struct timespec* ptrToStartTimestamp;
} ProducerArgs;

typedef struct {
    lockfreeSpscQueue* ptrToQueue;
    pthread_barrier_t* startBarrier;
    void** ptrToPutPoppedElements;
    int numElementsToPop;
    int sampleInterval;
    struct timespec* ptrToPopTimestamps;
    struct timespec* ptrToEndTimestamp;
} ConsumerArgs;

void preFault(size_t totalBytes, void* pointerToMemory);
void* producerThread(void* pArgs);
void* consumerThread(void* cArgs);
void pinThreadToCore(pthread_t thread, int coreId);

int main(int argc, char* argv[]) {
    int sampleInterval = (argc > 1) ? atoi(argv[1]) : DEFAULT_SAMPLE_INTERVAL;
    int minBufferSize  = (argc > 2) ? atoi(argv[2]) : DEFAULT_MIN_BUFFER_SIZE;
    int numElements    = (argc > 3) ? atoi(argv[3]) : DEFAULT_NUM_ELEMENTS;
    int numSamples     = (numElements + sampleInterval - 1) / sampleInterval;

    // 1. Initialize Queue
    lockfreeSpscQueue* ptrToQueue = initQueue(minBufferSize);

    // 2. Initialize Elements to Push
    void** ptrToElementsToPush = malloc(sizeof(void*) * numElements);
    for (int i = 0; i < numElements; i++) {
        ptrToElementsToPush[i] = malloc(sizeof(int));
        *((int*)ptrToElementsToPush[i]) = i;
    }

    // 3. Allocate Popped Array & Timestamp Arrays (sized to numSamples)
    void** ptrToPutPoppedElements      = malloc(sizeof(void*) * numElements);
    struct timespec* ptrToPushTimestamps = malloc(sizeof(struct timespec) * numSamples);
    struct timespec* ptrToPopTimestamps  = malloc(sizeof(struct timespec) * numSamples);
    struct timespec* ptrToStartTimestamp = malloc(sizeof(struct timespec));
    struct timespec* ptrToEndTimestamp   = malloc(sizeof(struct timespec));

    // 4. Pre-Fault Arrays using exact byte sizes (do NOT overwrite ptrToQueue struct fields)
    preFault(sizeof(void*) * numElements, ptrToPutPoppedElements);
    preFault(sizeof(struct timespec) * numSamples, ptrToPushTimestamps);
    preFault(sizeof(struct timespec) * numSamples, ptrToPopTimestamps);
    preFault(sizeof(struct timespec), ptrToStartTimestamp);
    preFault(sizeof(struct timespec), ptrToEndTimestamp);

    // 5. Set up synchronization barrier and thread args
    pthread_barrier_t startBarrier;
    pthread_barrier_init(&startBarrier, NULL, 2);

    ProducerArgs producerArgs = {
        .ptrToQueue = ptrToQueue,
        .startBarrier = &startBarrier,
        .ptrToElementsToPush = ptrToElementsToPush,
        .numElementsToPush = numElements,
        .sampleInterval = sampleInterval,
        .ptrToPushTimestamps = ptrToPushTimestamps,
        .ptrToStartTimestamp = ptrToStartTimestamp
    };

    ConsumerArgs consumerArgs = {
        .ptrToQueue = ptrToQueue,
        .startBarrier = &startBarrier,
        .ptrToPutPoppedElements = ptrToPutPoppedElements,
        .numElementsToPop = numElements,
        .sampleInterval = sampleInterval,
        .ptrToPopTimestamps = ptrToPopTimestamps,
        .ptrToEndTimestamp = ptrToEndTimestamp
    };

    // 6. Create and pin threads to distinct cores
    pthread_t producerThreadId, consumerThreadId;
    pthread_create(&producerThreadId, NULL, producerThread, &producerArgs);
    pthread_create(&consumerThreadId, NULL, consumerThread, &consumerArgs);

    pinThreadToCore(producerThreadId, PRODUCER_CORE);
    pinThreadToCore(consumerThreadId, CONSUMER_CORE);

    // 7. Wait for both threads to finish
    pthread_join(producerThreadId, NULL);
    pthread_join(consumerThreadId, NULL);
    pthread_barrier_destroy(&startBarrier);

    // 8. Correctness check
    for (int i = 0; i < numElements; i++) {
        if (ptrToElementsToPush[i] != ptrToPutPoppedElements[i]) {
            fprintf(stderr, "Error: Element at index %d does not match\n", i);
            return 1;
        }
    }

    // 9. Write sampled timestamps to CSV
    FILE* fp = fopen("latency_throughput.csv", "w");
    if (fp == NULL) {
        fprintf(stderr, "Error: Could not open file for writing\n");
        return 1;
    }
    fprintf(fp, "PushTimestamp,PopTimestamp,StartTimestamp,EndTimestamp\n");
    for (int s = 0; s < numSamples; s++) {
        fprintf(fp, "%ld.%09ld,%ld.%09ld,%ld.%09ld,%ld.%09ld\n",
                ptrToPushTimestamps[s].tv_sec, ptrToPushTimestamps[s].tv_nsec,
                ptrToPopTimestamps[s].tv_sec, ptrToPopTimestamps[s].tv_nsec,
                ptrToStartTimestamp->tv_sec, ptrToStartTimestamp->tv_nsec,
                ptrToEndTimestamp->tv_sec, ptrToEndTimestamp->tv_nsec);
    }
    fclose(fp);

    // 10. Free memory cleanly
    for (int i = 0; i < numElements; i++) {
        free(ptrToElementsToPush[i]);
    }
    free(ptrToElementsToPush);
    free(ptrToPutPoppedElements);
    free(ptrToPushTimestamps);
    free(ptrToPopTimestamps);
    free(ptrToStartTimestamp);
    free(ptrToEndTimestamp);
    free(ptrToQueue); // Replace with destroyQueue(ptrToQueue) if internal buffer was malloc'd

    return 0;
}

void pinThreadToCore(pthread_t thread, int coreId) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(coreId, &cpuset);
    if (pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset) != 0) {
        perror("Warning: Failed to pin thread to core");
    }
}

void* producerThread(void* pArgs) {
    ProducerArgs* args = (ProducerArgs*)pArgs;
    pthread_barrier_wait(args->startBarrier);

    clock_gettime(CLOCK_MONOTONIC, args->ptrToStartTimestamp);

    for (int i = 0; i < args->numElementsToPush; i++) {
        if (i % args->sampleInterval == 0) {
            clock_gettime(CLOCK_MONOTONIC, &args->ptrToPushTimestamps[i / args->sampleInterval]);
        }
        while (!tryPushToQueue(args->ptrToQueue, args->ptrToElementsToPush[i])) {
            // busy wait
        }
    }
    return NULL;
}

void* consumerThread(void* cArgs) {
    ConsumerArgs* args = (ConsumerArgs*)cArgs;
    pthread_barrier_wait(args->startBarrier);

    for (int i = 0; i < args->numElementsToPop; i++) {
        while (!tryPopFromQueue(args->ptrToQueue, &(args->ptrToPutPoppedElements[i]))) {
            // busy wait
        }
        if (i % args->sampleInterval == 0) {
            clock_gettime(CLOCK_MONOTONIC, &args->ptrToPopTimestamps[i / args->sampleInterval]);
        }
    }

    clock_gettime(CLOCK_MONOTONIC, args->ptrToEndTimestamp);
    return NULL;
}

void preFault(size_t totalBytes, void* pointerToMemory) {
    if (totalBytes == 0 || pointerToMemory == NULL) {
        return;
    }

    long pageSize = sysconf(_SC_PAGESIZE);
    volatile char* bytePtr = (volatile char*)pointerToMemory;

    for (size_t i = 0; i < totalBytes; i += pageSize) {
        bytePtr[i] = 0;
    }
    bytePtr[totalBytes - 1] = 0;
}