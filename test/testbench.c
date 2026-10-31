#include <unistd.h>

// DEFINE SAMPLE_INTERVAL
// DEFINE MIN_BUFFER_SIZE
// DEFINE NUM_ELEMENTS_TO_PUSH
// Make these runtime arguments

typedef struct {
    // Pointer to Queue;

    // Pointer to Array (Elements to Push);

    // Num of Elements;

    // Interval to take timestamp;

    // Pointer to Array (PushTimestamp);

    // Place to put start timestamp;
} ProducerArgs;


typedef struct {
    // Pointer to Queue;

    // Pointer to Array (PutPoppedElements);
    
    // Num of Elements;

    // Interval to take timestamp;

    // Pointer to Array (PopTimestamp);

    // Place to put end timestamp;
} ConsumerArgs;

int main () {
    // 1. Initialize Queue (Make it aligned to cache line so false sharing is always distinguishable); Pointer to the struct to be passed to worker threads

    // 2. Initialize Array; Initialize Array with Pointers to elements of previous array; Pointer to this array to be passed to the producer thread

    // 3. Initialize Empty Array for Popped Elements; Pointer to the array to be passed to the consumer thread

    // 4. Initialize PushTimestamp Array

    // 5. Initialize PopTimestamp Array

    // 6. Pre-Fault RingBuffer, EmptyArray, PushTimestamp and PopTimestamp



    // 7. Synchronize Start for producerThread and ConsumerThread; Pin each to their own (distinct) physical CPU core.

    // 8. Wait for both to finish

    // 9. Perform correctness check: compare elements that were to be pushed against popped elements.

    // 10. Compute Latencies and Throughput and write to a file. 

    // 11. Free Queue and Arrays
    
    return 0;
}

void* producerThread (void* pArgs) { // Pushes items from a given array to the ring buffer
    // 1. Dereference and Cast pArgs to ProducerArgs

    // 2. Take before timestamp for throughput.
    // 3. For-Loop pushing elements 
        // a. Take and store PushTimestamp at specified intervals.
        // b. While-Loop until push is successful.

}

void* consumerThread (void* cArgs) { // Pops items from the ring buffer into a given array
    // 1. Dereference and Cast cArgs to ConsumerArgs

    // 2. For-Loop popping elements
        // a. While-Loop until pop is successful
        // b. Take and store PopTimestamp at specified intervals.
    // Take after timestamp for throughput.
}


void preFault (int capacity, void** pointerToMemory) { // PreFaults Memory
    // Validating arguments
    if (capacity <= 0 || pointerToMemory == NULL) {
        return;
    }

    // 1. Get the page size
    long pageSize = sysconf(_SC_PAGESIZE);

    // malloc, by default, aligns our memory to multiples of 16. What exactly does this mean?
    // pointerToMemory is some memory address that is a multiple of 16. Our page frames begin at memory addresses that are multiples of the page size.
    // If pointerToMemory is coincidentally some multiple of the pageSize, it'd be sufficient to touch every pageSize-th byte starting from pointerToMemory (relative 0).
    // But this is not necessarily the case. 
    // Suppose we had pointerToMemory somewhere in the middle of a page frame, and numBytes was smaller than pageSize, but enough to overflow into the next page frame. By the former logic, we'd only touch pointerToMemory (relative 0),
    // but its possible that the next page frame remained uninitialized, and cause a page fault on our hot-path - a problem. The solution? We simply touch the last byte of numBytes to ensure all our page frames are loaded.

    // 2. PreFault at intervals
    for (int i = 0; i < capacity; i += (pageSize / sizeof(void*))) { // We want to increment `i` such that it incrementSize is exactly the number of void* that occupy pageSize bytes. 
        (((volatile void**)pointerToMemory)[i]) = NULL; // pointerToMemory[i] is the same as (pointerToMemory + i * (sizeof(void*)));
        // We cast pointerToMemory as a volatile pointer so the compiler doesn't optimize away the loop.
        // A volatile variable tells the compiler that this variable may change at anytime from outside the current code path, meaning the compiler must never optimize away reads or writes to it.
    }


    // 3. Unconditionally PreFault Final Chunk
    (((volatile void**)pointerToMemory)[capacity - 1]) = NULL; // capacity-1 is the index of the final element in the array

    return;
}