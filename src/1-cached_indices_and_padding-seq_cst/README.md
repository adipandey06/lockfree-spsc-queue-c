# The Concept

$CONTEXT$ We share (in the sense of conflicting data accesses) exactly two variables, `head` and `tail`. `Producer` writes to `head` and `Consumer` reads `head`, and `Consumer` writes to `tail`, and `Producer` reads `tail`.

$INTUITION$ Imagine you and a friend are working on some joint project. Each of you keeps a running note of progress: yours says how far you've gotten, and theirs says how far they've gotten. Each of you needs to check the other's note, but only to know if you can proceed.

$ANALOGY$ "***how far you've gotten***", "***how far they've gotten***" `head`/`tail`; "***but only to know if you can proceed***" emptiness check for pop / fullness check for push.

- Suppose we have both your notes written on the same sheet of paper. Only one person can hold the sheet to write on it. Every time you update ***your*** note, you have to snatch the sheet, which takes it away from your friend even though they only wanted to update ***their*** note. You're not conflicted about the information, just about where it is written.
	$ANALOGY$ "***same sheet of paper***" same cache line; ***"snatch the sheet"*** invalidate the other's copy of the cache line; "***information***" value of the other's index.

	This is what we call false sharing of memory. Although both your notes are logically independent variables, they reside on the same sheet of paper i.e. cache line, and memory is operated on at the granularity of cache lines. So we have two threads (you and your friend) contending over the same cache line even though the specific memory locations are different.

- Now suppose each of you gets your own sheet to note progress. Updating your own progress never disturbs the other.
	$ANALOGY$ "***each of you gets your own sheet to note progress***" `head` and `tail` reside on different cache lines.

	This removes the contention when both of you update your own progress, but true sharing remains for the emptiness/fullness check. You still walk over and read their sheet every time you're about to do something, and that interrupts them: your read downgrades their line from ***Modified*** to ***Shared***, so their next write needs a `BusUpgr`.

- Now suppose that, on top of separate sheets, you keep a sticky note with a copy of their last-known progress. You only walk over and re-read their sheet when your sticky note says you've run out of room to proceed, so you barge in on your friend rarely instead of every time.
	$ANALOGY$ "***sticky note***" a thread-local `cachedTail` and `cachedHead` for `Producer` and `Consumer` respectively; "***re-read their sheet when your sticky note says you've run out of room to proceed***" reload the shared `head`/`tail` from memory, pulling the line from the other core when the thread-local value indicates a boundary condition (empty / full).

	Without separate sheets, the sticky note saves little: your own updates would still snatch the shared sheet. Without the sticky note, separate sheets only remove the extra snatching, while the walking over remains. Each fix is only fully visible once the other is in place.

With both, your own sheet is almost never disturbed (the cache line holding `Producer`'s `head`, and the cache line holding `Consumer`'s `tail`, are never invalidated by the other side's writes, and only occasionally downgraded from ***Modified*** to ***Shared*** by the other side's reads), and you only consult the other when the sticky note doesn't suffice.

- [ ] What exactly is going on at the hardware level?

---

# The Implementation

$PROBLEM$ Data Races and Memory ordering.

A data race is two conflicting non-atomic accesses with no happens-before chain between them. Our obligation as the programmer is to exhibit a chain for every conflicting pair of slot accesses.

First, let us define the order of operations we need to preserve correctness.
### Push

1. Atomic load of `head`, load of `cachedTail`, in arbitrary order.
2. If `cachedTail` indicates the queue to be full, atomic load of `tail`. Otherwise proceed.
	If `tail` indicates queue to be full, the queue is full. Push operation cannot happen. Otherwise, proceed.
3. Write to `ptrToRingBuffer[head]`.
4. Atomic store of `incrementedHead` to `head`.
### Pop

1. Atomic load of `tail`, load of `cachedHead`, in arbitrary order.
2. If `cachedHead` indicates the queue to be empty, atomic load of `head`. Otherwise proceed.
	If `head` indicates queue to be empty, the queue is empty. Pop operation cannot happen. Otherwise, proceed.
3. Read `ptrToRingBuffer[tail]`.
4. Write to pointer given in arguments.
5. Atomic store of `incrementedTail` to `tail`.



$REMARK$ We want the atomic write to owned-indices to be a release-store, and atomic-load of counter-part indices to be an acquire-load. 

$INTUITION$ An acquire-load of a counter-part index that reads the value written by a release store sees every write that preceded that release-store. 

- [ ] If every store to `head` is a release, doesn't every acquire-load of `head` synchronize with one? Vice-versa for tail?
	The load can return any release-store to `head`, not only the most recent. Say the producer does:
	```
	write slot 0;  store head=1 (release)
	write slot 1;  store head=2 (release)
	```
	The consumer's acquire-load of `head` may return 0, 1 or 2:
	- Returns 2: It synchronizes with the second store, meaning it is guaranteed to see both slot writes.
	- Returns 1: It synchronizes with the first store, and it is guaranteed to see slot 0. It is not guaranteed to see slot 1, because that write came after the store it read from.
	- Returns 0: it read the initial value, which is not a release-store at all, so it synchronizes with nothing.
	
	- [ ] What does this mean practically?

- [ ] How would we prove correctness of our program according to C11 formalisms?