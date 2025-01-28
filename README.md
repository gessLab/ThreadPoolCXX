# ThreadPoolCXX
Thread pool with one barrier for resetting threads and toggleable SPIN or SLEEP states, all post dispatch.

# Overview
This is a singleton class, for global access regardless of location in your source code.

The thread pool system offers two dispatch functions (pending a third) which act best for *Task* vs *Data* parallelism. The difference is the _data parallel disptach_ invokes a function that takes in two **size_t** variables -- one used for the beginning index of a shared data pool, and the ending index of the shared data pool. This distinction is for performance purposes, but will be described by first outlining how the system dispatches work.

## Dispatch Calls
The dispatch call arguments take in the follwing (general) information:
- Count of work
- Function pointer for the work to perform
- An post operation _spin_ or _sleep_ state

Turning to a sleep state ensures system load will not remain at a potential **100%** while not utilizing the thread pool, but spinning will be useful when immediate responsiveness is desired between parallel dispatch invocations.

The count of work is evenly distributed to the available count of worker threads (including the _main thread_). This means a range of work can be precomputed to distribute for the number of function pointer invocations to make. The differentiating concern for data and task parallelism is as follows:

### Task Parallel Invocation
The task parallel invocation has the Thread Pool manage iterating through a thread's fraction of the total work items -- invoking the function pointer for each execution. The function pointer expects a shared mutex (_for those moments where a critical section is going to be slowed with software locking :(_) and a size_t for the work item being processed. This index is useful for conveying which unique task (but could be data parallel work) should be worked on from the work function invocation (using indirection to execute another unique function if needed).

The data parallel work flow changes this work function pointer signature slightly to promote performance.

### Data Parallel Invocation
The data parallel invocation adds a second size_t argument to the work function pointer. This is because the data parallel execution does not iterate over a thread's work item ranges, opting instead to invoke the work function and pass the beginning and end of the range to the function pointer.

This means the work function supplied should implement its own for-loop or similar iterations solution from the beginning index to the ending index. Offloading the iteration over work items was found to be consistently more performant between both Intel and AMD x64 chips (thoroughly charts to be supplied in the future as time allows) from a few generations of CPUs. Conceptually, this makes sense as the program does not need to make repeated stack frames for executing an identical looking function all the time, and can instead iterate over a sequential data space to further promote cache coherency.

## General Pool Behavior
After spawning threads, the dispatch system needs to ensure the threads are in a _ready state_ to be successfully dispatched "simultaneously" from a waiting state (either system sleeping or busy waiting in a spin lock).

The range of work is distributed for each thread, including to the main thread. The threads are released to perform their tasks (including threads that have no current work items). Once complete, the main thread spins -- polling an atomic counter, waiting for all threads to be accounted for by an atomic increment operation. The spawned threads do two things, they modify a logical clock and they atomically increment the shared atomic counter.

The main thread is released when the atomic counter matches the size of spawned threads. The logical clock is necessary for the predicate condition if the threads are put to sleep. If the system is too fast, problems with waking all of the thread pool can go awry -- notably, waking threads might conventionally be better suited for a consumer-producer model (_waking only the needed threads as tasks become available_). **HOWEVER,** waking all threads from a sleeping state requires an accurate predicate condition, and to ensure that all threads were asleep before the main thread tries to "notify all." Moreover, a _pessimistic view_ of lock acquisition is necessary to ensure not causing a deadlock scenario when using sleep and attempting to release the entire work pool.
