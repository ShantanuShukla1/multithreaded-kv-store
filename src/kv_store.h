#ifndef KV_STORE_H
#define KV_STORE_H

#include <pthread.h>
#include <stddef.h>

#define MAX_KEY_LEN 64
#define MAX_VALUE_LEN 256

// A single cache entry. Lives in two structures simultaneously:
//   1. A hash bucket chain (via `hash_next`) for O(1) key lookup
//   2. The LRU doubly linked list (via `lru_prev` / `lru_next`) for recency tracking
typedef struct Node {
    char key[MAX_KEY_LEN];
    char value[MAX_VALUE_LEN];

    struct Node *hash_next;   // next node in this bucket's chain

    struct Node *lru_prev;    // toward most-recently-used end
    struct Node *lru_next;    // toward least-recently-used end
} Node;

typedef struct {
    Node **buckets;           // array of bucket head pointers
    size_t num_buckets;

    Node *lru_head;           // most recently used
    Node *lru_tail;           // least recently used (eviction target)

    size_t size;              // current number of entries
    size_t capacity;          // max entries before eviction kicks in

    pthread_mutex_t lock;     // global lock guarding all mutations
} KVStore;

// Lifecycle
KVStore *kv_store_create(size_t capacity, size_t num_buckets);
void kv_store_destroy(KVStore *store);

// Core operations
// Returns 0 on success, -1 if key not found (for get/delete)
int kv_put(KVStore *store, const char *key, const char *value);
int kv_get(KVStore *store, const char *key, char *value_out);
int kv_delete(KVStore *store, const char *key);

// Debug helper (not thread-safe, use only when no other threads active)
void kv_debug_print(KVStore *store);

#endif