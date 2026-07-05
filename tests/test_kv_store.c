// Assertion-based correctness tests, separate from the demo in main.c.
// Exits with status 0 and prints "ALL TESTS PASSED" if everything holds,
// or aborts via assert() with a clear failure point otherwise.

#include "../src/kv_store.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

static void test_basic_put_get(void) {
    KVStore *store = kv_store_create(10, 8);
    char out[MAX_VALUE_LEN];

    assert(kv_put(store, "a", "1") == 0);
    assert(kv_get(store, "a", out) == 0);
    assert(strcmp(out, "1") == 0);

    kv_store_destroy(store);
    printf("[PASS] test_basic_put_get\n");
}

static void test_get_missing_key(void) {
    KVStore *store = kv_store_create(10, 8);
    char out[MAX_VALUE_LEN];

    assert(kv_get(store, "nonexistent", out) == -1);

    kv_store_destroy(store);
    printf("[PASS] test_get_missing_key\n");
}

static void test_update_existing_key(void) {
    KVStore *store = kv_store_create(10, 8);
    char out[MAX_VALUE_LEN];

    kv_put(store, "a", "1");
    kv_put(store, "a", "2"); // update, not a new entry
    kv_get(store, "a", out);

    assert(strcmp(out, "2") == 0);
    assert(store->size == 1); // still just one entry, not two

    kv_store_destroy(store);
    printf("[PASS] test_update_existing_key\n");
}

static void test_delete(void) {
    KVStore *store = kv_store_create(10, 8);
    char out[MAX_VALUE_LEN];

    kv_put(store, "a", "1");
    assert(kv_delete(store, "a") == 0);
    assert(kv_get(store, "a", out) == -1);
    assert(kv_delete(store, "a") == -1); // deleting again should fail cleanly

    kv_store_destroy(store);
    printf("[PASS] test_delete\n");
}

static void test_lru_eviction_order(void) {
    KVStore *store = kv_store_create(3, 8);
    char out[MAX_VALUE_LEN];

    kv_put(store, "a", "1");
    kv_put(store, "b", "2");
    kv_put(store, "c", "3");

    kv_get(store, "a", out); // touch "a", making "b" the true LRU

    kv_put(store, "d", "4"); // should evict "b", not "a"

    assert(kv_get(store, "a", out) == 0);  // "a" survived
    assert(kv_get(store, "b", out) == -1); // "b" was evicted
    assert(kv_get(store, "c", out) == 0);  // "c" survived
    assert(kv_get(store, "d", out) == 0);  // "d" was just added

    kv_store_destroy(store);
    printf("[PASS] test_lru_eviction_order\n");
}

static void test_capacity_never_exceeded(void) {
    KVStore *store = kv_store_create(5, 8);

    char key[MAX_KEY_LEN];
    for (int i = 0; i < 50; i++) {
        snprintf(key, sizeof(key), "key-%d", i);
        kv_put(store, key, "value");
        assert(store->size <= store->capacity);
    }

    assert(store->size == store->capacity);

    kv_store_destroy(store);
    printf("[PASS] test_capacity_never_exceeded\n");
}

// --- Concurrency test ---

#define STRESS_THREADS 8
#define STRESS_OPS 20000
#define STRESS_CAPACITY 100

typedef struct {
    KVStore *store;
    int thread_id;
} StressArgs;

static void *stress_worker(void *arg) {
    StressArgs *args = (StressArgs *)arg;
    char key[MAX_KEY_LEN];
    char value[MAX_VALUE_LEN];
    char value_out[MAX_VALUE_LEN];

    for (int i = 0; i < STRESS_OPS; i++) {
        int key_id = rand() % (STRESS_CAPACITY * 2);
        snprintf(key, sizeof(key), "key-%d", key_id);
        snprintf(value, sizeof(value), "val-%d-t%d", i, args->thread_id);

        int op = rand() % 3;
        if (op == 0) kv_put(args->store, key, value);
        else if (op == 1) kv_get(args->store, key, value_out);
        else kv_delete(args->store, key);
    }
    return NULL;
}

static void test_concurrent_stress(void) {
    KVStore *store = kv_store_create(STRESS_CAPACITY, 64);
    pthread_t threads[STRESS_THREADS];
    StressArgs args[STRESS_THREADS];

    for (int i = 0; i < STRESS_THREADS; i++) {
        args[i].store = store;
        args[i].thread_id = i;
        pthread_create(&threads[i], NULL, stress_worker, &args[i]);
    }
    for (int i = 0; i < STRESS_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // The real assertion here is that we survive to this point without
    // crashing, and that the capacity invariant held under contention.
    assert(store->size <= store->capacity);

    kv_store_destroy(store);
    printf("[PASS] test_concurrent_stress (%d threads x %d ops, no crash, capacity held)\n",
           STRESS_THREADS, STRESS_OPS);
}

int main(void) {
    test_basic_put_get();
    test_get_missing_key();
    test_update_existing_key();
    test_delete();
    test_lru_eviction_order();
    test_capacity_never_exceeded();
    test_concurrent_stress();

    printf("\nALL TESTS PASSED\n");
    return 0;
}
