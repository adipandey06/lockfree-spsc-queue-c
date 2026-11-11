what does this readme contain?

1. [a description of what this repo contains;](#1-what-does-this-repo-contain)
2. [a description of the algorithm of the push/pop operations.](#2-how-do-the-operations-work)

### 1. what does this repo contain? 

**two things**:

1. five implementations of a single-producer, single-consumer lock-free queue. a naive implementation going off of intuition, an implementation the head and tail indices on different cache lines to minimize false sharing, an implementation with cached indices to (ideally) minimize true sharing, and a version with both, and finally extending this version with a bitwise operation to replace the costly modulus operation. each of these five implementations have two variants: one with sequentially consistent memory ordering, and one with acquire release semantics.

2. a benchmark for each of these implementations. 
	- **_what does the benchmark do_**? 
		for every variant of every implementation, it queues `<num_of_elements>`  pointers using `tryPushToQueue`, and dequeues them using `tryPopFromQueue`. it takes timestamps using the monotonic clock at a resolution of nanoseconds once before the first push, once after the final pop, and once every `<sample_interval>` elements, and writes them to a csv, respectively. it runs `<num_of_runs>` iterations of this exactly, with the order of variants being selected at random. it then runs the jupyter notebook.
	
	- **_what does the jupyter notebook do_**?
		plots the (averaged) throughput of each implementation against buffer sizes. saves the plot, `output.png`, and the executed jupyter notebook, `analysis.ip` to `OUTPUT_DIR`.
	  
	- **_how do you run the benchmark_**? 
		  in the directory where you've cloned the repo, run `CPU_SET = <> ./run_containerized_harness <num_of_runs> <output_directory> <sample_interval> <num_of_elements>`
			- `CPU_SET` holds the thread numbers on which the producer, consumer and harness processes run respectively. by default: `0,2,4`;
			- `<output_directory>` is the relative path of where you want the csv's to be saved. by default: `/results`;
			- `<num_of_runs>` self explanatory. by default: `1`.
			- `<sample_interval>` self explanatory. by default: `4096`.
			- `<num_of_elements>` self explanatory. by default: `8388608`.



### 2. how do the operations work?

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
