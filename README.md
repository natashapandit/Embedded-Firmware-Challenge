
# Embedded Firmware Coding Challenge

This repository contains two C implementations: a single-producer/single-consumer (SPSC) ring buffer and a mutex with priority inheritance.

## Build and Run

The programs were built and tested using Ubuntu through WSL.

To build:

```bash
cmake -S . -B build
cmake --build build
```

To run the tests:

```bash
./build/test_ringbuf
sudo ./build/test_pi_mutex
```

The second test requires `sudo` to allow the use of real-time thread priorities.

## Part 1: SPSC Ring Buffer

A circular buffer that allows one producer to add bytes and one consumer to remove them.

The buffer uses two indices:
- `head`: position of the next byte to be read
- `tail`: position where the next byte will be written

Both indices wrap around when they reach the end of the buffer. C11 atomic variables are used to allow the producer and consumer to operate concurrently without a mutex.

One slot is left unused to distinguish between a full and empty buffer. For example, a buffer with a capacity of 5 can store 4 bytes.

### Testing

The tests check:
- Empty and full buffer detection
- FIFO ordering
- Index wraparound
- Capacity-one edge case
- Concurrent producer and consumer operation

The concurrent test transfers 100,000 bytes between two threads.

**Result:** All tests passed.

## Part 2: Priority Inheritance Mutex

### Priority Inversion

Priority inversion occurs when a high-priority thread is blocked by a low-priority thread holding a mutex. If a medium-priority thread is also running, it can prevent the low-priority thread from finishing its work and releasing the mutex. This delays the high-priority thread even though it has a higher priority.

Priority inheritance prevents this by temporarily raising the priority of the mutex owner to match the highest-priority waiting thread. Once the mutex is released, the owner's original priority is restored.

### Implementation

The mutex uses POSIX threads, with a regular pthread mutex to protect internal variables and condition variables to block waiting threads.

When a thread attempts to lock an occupied mutex, its priority is compared with the current owner's priority. If the waiting thread has a higher priority, the owner's priority is raised using `pthread_setschedparam()`.

When the mutex is unlocked, the owner's original priority is restored, and ownership is passed to the highest-priority waiting thread.

The implementation does not use the built-in `PTHREAD_PRIO_INHERIT` option.

### Testing

Three threads are used to demonstrate priority inheritance:

| Thread | Priority | Role |
|---|---|---|
| LOW | 10 | Acquires and holds the mutex |
| MEDIUM | 20 | Performs CPU work independently |
| HIGH | 30 | Attempts to acquire the mutex while LOW holds it |

LOW acquires the mutex first. HIGH then attempts to acquire it and blocks. MEDIUM runs a separate CPU workload.

The test checks LOW's scheduling priority before HIGH blocks, while HIGH is waiting, and after LOW releases the mutex.

**Observed priority changes:**

- Initially: 10
- While HIGH is waiting: 30
- After unlocking: 10

HIGH successfully acquires the mutex after LOW releases it.

**Result:** Test passed using `sudo` in WSL. Running without elevated permissions results in a scheduling permission error (`EPERM`).

## Test Results

### Part 1

```text
PASS: initially empty
PASS: full detection and capacity
PASS: FIFO order, wraparound, empty detection
PASS: capacity-one edge case
PASS: 100,000 concurrent SPSC transfers
All ring buffer tests PASS
```

### Part 2

```text
LOW: acquired mutex
LOW initially: policy=SCHED_FIFO priority=10
HIGH: trying to acquire mutex (should block)
MEDIUM: doing CPU work without using the mutex
MEDIUM: finished CPU work
LOW after HIGH blocks: policy=SCHED_FIFO priority=30
LOW: releasing mutex
LOW after unlock: policy=SCHED_FIFO priority=10
HIGH: acquired mutex after LOW released it
PASS: HIGH acquired mutex after LOW released it
```

## Limitations

- The ring buffer supports one producer and one consumer.
- The priority-inheritance mutex is a simplified implementation for a single mutex and does not handle nested priority inheritance.
- Real-time scheduling permissions are required to demonstrate priority inheritance.
