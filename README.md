![build](https://github.com/ShantanuShukla1/multithreaded-kv-store/actions/workflows/ci.yml/badge.svg)
# Multithreaded Key-Value Store

A thread-safe, in-memory key-value store written in C, supporting concurrent access from multiple threads with LRU (Least Recently Used) eviction under a fixed memory capacity.

Built as a systems-programming portfolio project to demonstrate concurrent data structure design, manual memory management, and correctness verification using industry-standard tooling (ThreadSanitizer, AddressSanitizer).

## Features

- **O(1) average-case get/put/delete** via a hash table with separate chaining
- **O(1) LRU eviction** via a doubly linked list threaded through the same nodes as the hash table
- **Thread-safe** — all operations are guarded by a mutex, verified race-free under ThreadSanitizer across 160,000+ concurrent operations
- **No memory leaks** — verified under AddressSanitizer and Valgrind, including on the eviction path where nodes are freed while other threads may be concurrently reading

## Build & Run

Requires `gcc` and POSIX threads (available by default on Linux/WSL).

```bash
make              # standard build
./kvstore

make tsan         # build with ThreadSanitizer (detects data races)
./kvstore_tsan

make asan         # build with AddressSanitizer (detects leaks, use-after-free, overflows)
./kvstore_asan

make clean        # remove all built binaries
```

Optionally, verify with Valgrind:

```bash
valgrind --leak-check=full --show-leak-kinds=all ./kvstore
```

`main.c` runs two demos automatically:
1. A small single-threaded walkthrough showing LRU eviction order changing as keys are accessed
2. A stress test spawning 8 threads, each performing 20,000 mixed put/get/delete operations against a shared store with overlapping keys, intentionally forcing lock contention and eviction

## Design

### Data structure

Each cache entry (`Node`) is threaded into two structures simultaneously:

- A **hash bucket chain** (`hash_next` pointer) for O(1) average-case key lookup, using separate chaining to resolve collisions
- The **LRU doubly linked list** (`lru_prev` / `lru_next` pointers), maintaining strict recency order from most-recently-used (head) to least-recently-used (tail)

This is the standard approach for building an LRU cache (the same idea behind the classic "LRU Cache" interview problem), extended here with a hash table for lookup and made thread-safe for concurrent access.

On a cache hit (`get`, or `put` on an existing key), the node is unlinked from its current position in the LRU list and moved to the head — O(1) because we already have direct pointers to the node from the hash lookup, no traversal needed.

On eviction (`put` when the store is at capacity), the tail of the LRU list — the least-recently-used entry — is popped, removed from its hash bucket, and freed.

### Concurrency model

A single global `pthread_mutex_t` guards every operation. Each public function (`kv_put`, `kv_get`, `kv_delete`) locks at entry and unlocks before returning, so the entire store is serialized: only one thread can be inside the store at a time, regardless of whether their target keys collide.

`kv_get` copies the value into a caller-provided buffer *before* releasing the lock, rather than returning a pointer into the store's internal memory. This avoids a subtle class of use-after-free bug: without this, one thread could receive a pointer to a node, and before it finished reading, another thread could evict and free that same node.

**Known limitation:** the global lock means all operations are fully serialized, even ones touching unrelated keys. This is correct but not maximally concurrent. A natural next step (see below) is per-bucket locking, so operations on different hash buckets can proceed in parallel.

### Fixed-size buffers

Keys and values are stored in fixed-size `char` arrays (`MAX_KEY_LEN`, `MAX_VALUE_LEN`) rather than heap-allocated strings. This simplifies ownership — no need to `strdup`/`free` key and value strings independently — at the cost of wasting space on short strings and capping maximum key/value length.

## Verification

This project was validated with:

- **ThreadSanitizer** (`make tsan`) — instruments every shared-memory access and flags data races. Ran clean across multiple runs of the 8-thread, 160,000-operation stress test.
- **AddressSanitizer** (`make asan`) — flags memory leaks, use-after-free, and buffer overflows. Ran clean.
- **Valgrind** — independent leak-detection pass, also clean.

## Possible extensions

- **Sharded locking**: replace the single global mutex with per-bucket locks (or a small fixed pool of locks hashed by bucket), allowing operations on different buckets to run concurrently instead of serializing all access
- **Read-write locks**: if reads dominate writes in the target workload, a `pthread_rwlock_t` would let concurrent readers proceed together
- **Dynamic-length keys/values**: replace fixed-size buffers with heap-allocated strings for arbitrary-length data, at the cost of more careful ownership tracking
- **Resizable hash table**: grow `num_buckets` dynamically as `size` approaches `num_buckets` to keep chain lengths short under heavy load

## Project structure

```
kv-store/
├── src/
│   ├── kv_store.h    # Node, KVStore structs; public API declarations
│   ├── kv_store.c    # hash table, put/get/delete, eviction logic
│   ├── lru_list.h    # LRU doubly-linked-list operation declarations
│   ├── lru_list.c    # push_front, remove, move_to_front, pop_back
│   └── main.c        # demo + multithreaded stress test
├── Makefile
└── README.md
```
