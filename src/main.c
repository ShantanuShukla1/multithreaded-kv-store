#include "kv_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define NUM_THREADS 8
#define OPS_PER_THREAD 20000
#define STORE_CAPACITY 100
#define NUM_BUCKETS 64

typedef struct {
    KVStore *store;
    int thread_id;
} WorkerArgs;

// Each thread hammers the store with a mix of put/get/delete on a
// shared, overlapping key range so we actually exercise contention
// and eviction, not just disjoint independent work.
void *worker(void *arg) {
    WorkerArgs *args = (WorkerArgs *)arg;
    KVStore *store = args->store;

    char key[MAX_KEY_LEN];
    char value[MAX_VALUE_LEN];
    char value_out[MAX_VALUE_LEN];

    for (int i = 0; i < OPS_PER_THREAD; i++) {
        int key_id = rand() % (STORE_CAPACITY * 2); // overlap range forces eviction
        snprintf(key, sizeof(key), "key-%d", key_id);
        snprintf(value, sizeof(value), "val-%d-from-t%d", i, args->thread_id);

        int op = rand() % 3;
        if (op == 0) {
            kv_put(store, key, value);
        } else if (op == 1) {
            kv_get(store, key, value_out);
        } else {
            kv_delete(store, key);
        }
    }

    return NULL;
}

static void run_basic_demo(void) {
    printf("=== Basic single-threaded demo ===\n");
    KVStore *store = kv_store_create(3, 8); // tiny capacity to show eviction clearly

    kv_put(store, "a", "1");
    kv_put(store, "b", "2");
    kv_put(store, "c", "3");
    kv_debug_print(store);

    char out[MAX_VALUE_LEN];
    kv_get(store, "a", out); // touch "a" so it becomes most-recently-used
    printf("get(a) = %s\n", out);
    kv_debug_print(store);

    kv_put(store, "d", "4"); // should evict "b" (least recently used after touching "a")
    printf("after put(d) [should evict 'b']:\n");
    kv_debug_print(store);

    if (kv_get(store, "b", out) == -1) {
        printf("get(b) correctly returned not-found (evicted)\n");
    }

    kv_store_destroy(store);
    printf("\n");
}

static void run_concurrent_stress_test(void) {
    printf("=== Multithreaded stress test ===\n");
    printf("%d threads x %d ops each, capacity=%d, buckets=%d\n",
           NUM_THREADS, OPS_PER_THREAD, STORE_CAPACITY, NUM_BUCKETS);

    KVStore *store = kv_store_create(STORE_CAPACITY, NUM_BUCKETS);

    pthread_t threads[NUM_THREADS];
    WorkerArgs args[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++) {
        args[i].store = store;
        args[i].thread_id = i;
        pthread_create(&threads[i], NULL, worker, &args[i]);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("All threads completed without crashing.\n");
    printf("Final store size: %zu (capacity: %zu)\n", store->size, store->capacity);

    if (store->size > store->capacity) {
        printf("BUG: size exceeded capacity!\n");
    } else {
        printf("Capacity invariant held.\n");
    }

    kv_store_destroy(store);
}

int main(void) {
    run_basic_demo();
    run_concurrent_stress_test();
    return 0;
}