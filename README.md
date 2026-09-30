# lockfree-spsc-queue-c
a lock-free single-producer-single-consumer queue in c


### book-keeping policies

we begin with both the head and tail initialized to 0. the head tracks the latest item published to the queue, and the tail tracks the next item to be consumed from the queue.

we think of the head as dominant such that if the head is at position `n`, the tail must wait until the head advances to `n+1` before it reads the element at `n` i.e. the tail may only read if `head != tail`. there's probably a "better" way to overlapping head/tail indices, but this is what makes the most intuitive sense to me at this time (15:49, 30/09/2026).

let `cap` be the length of the array with which we represent this queue. we get the following operations:

- push
  - checks whether the array is full
    if `(head+1) mod cap == tail` i.e. if the next available index for the head is the tail, it means the queue is full. if it is full, return execution status as false (0).
  - if the array is not full, we 
    1. write to the array slot indicated by `head` from memory given by caller,
    2. update the head s.t. `head = (head+1) mod cap`, thereby freeing up the written item for the tail, and occupying the next slot to write to, 
    3. return execution status as true (1).

- pop
  - checks whether the array is empty
    if `head == tail`, all items from the array have been consumed, and the tail is waiting on the head to write something and move (kind of like snake). if it is empty, return execution status as false (0).
  - if the array is not empty, we
    1. write the item to memory given by caller,
    2. update the tail such that `tail = (tail + 1) mod cap`,
    3. return the execution status as true (1).
