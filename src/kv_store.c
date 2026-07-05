#include "kv_store.h"
#include "lru_list.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// djb2 string hash — simple, fast, decent distribution for short keys
static unsigned long hash_key(const char *key) {
    unsigned long hash = 5381;
    int c;
    while ((c = *key++)) {
        hash = ((hash << 5) + hash) + (unsigned long)c; // hash * 33 + c
    }
    return hash;
}

static size_t bucket_index(KVStore *store, const char *key) {
    return hash_key(key) % store->num_buckets;
}

// Find a node by key within its bucket chain.
// If prev_out is non-NULL, it's set to the previous node in the chain
// (or NULL if the match is the bucket head) — needed for unlinking on delete.
static Node *find_in_bucket(KVStore *store, size_t idx, const char *key, Node **prev_out) {
    Node *prev = NULL;
    Node *curr = store->buckets[idx];

    while (curr != NULL) {
        if (strncmp(curr->key, key, MAX_KEY_LEN) == 0) {
            if (prev_out != NULL) {
                *prev_out = prev;
            }
            return curr;
        }
        prev = curr;
        curr = curr->hash_next;
    }
    return NULL;
}

KVStore *kv_store_create(size_t capacity, size_t num_buckets) {
    if (capacity == 0 || num_buckets == 0) {
        return NULL;
    }

    KVStore *store = malloc(sizeof(KVStore));
    if (store == NULL) {
        return NULL;
    }

    store->buckets = calloc(num_buckets, sizeof(Node *));
    if (store->buckets == NULL) {
        free(store);
        return NULL;
    }

    store->num_buckets = num_buckets;
    store->capacity = capacity;
    store->size = 0;
    store->lru_head = NULL;
    store->lru_tail = NULL;

    if (pthread_mutex_init(&store->lock, NULL) != 0) {
        free(store->buckets);
        free(store);
        return NULL;
    }

    return store;
}

void kv_store_destroy(KVStore *store) {
    if (store == NULL) {
        return;
    }

    pthread_mutex_lock(&store->lock);

    // Walk the LRU list (touches every node exactly once) and free each.
    Node *curr = store->lru_head;
    while (curr != NULL) {
        Node *next = curr->lru_next;
        free(curr);
        curr = next;
    }

    pthread_mutex_unlock(&store->lock);
    pthread_mutex_destroy(&store->lock);

    free(store->buckets);
    free(store);
}

// Unlink a node from its hash bucket chain given the bucket index and
// the previous node found by find_in_bucket. Does NOT free the node
// or touch the LRU list — caller's responsibility.
static void unlink_from_bucket(KVStore *store, size_t idx, Node *node, Node *prev) {
    if (prev == NULL) {
        store->buckets[idx] = node->hash_next;
    } else {
        prev->hash_next = node->hash_next;
    }
    node->hash_next = NULL;
}

int kv_put(KVStore *store, const char *key, const char *value) {
    if (store == NULL || key == NULL || value == NULL) {
        return -1;
    }

    pthread_mutex_lock(&store->lock);

    size_t idx = bucket_index(store, key);
    Node *existing = find_in_bucket(store, idx, key, NULL);

    if (existing != NULL) {
        // Key already present: update value in place, mark as most recently used.
        strncpy(existing->value, value, MAX_VALUE_LEN - 1);
        existing->value[MAX_VALUE_LEN - 1] = '\0';
        lru_move_to_front(store, existing);
        pthread_mutex_unlock(&store->lock);
        return 0;
    }

    // Evict least-recently-used entry if we're at capacity.
    if (store->size >= store->capacity) {
        Node *victim = lru_pop_back(store);
        if (victim != NULL) {
            size_t victim_idx = bucket_index(store, victim->key);
            Node *prev = NULL;
            // Re-find within the bucket to get the correct prev pointer for unlinking.
            find_in_bucket(store, victim_idx, victim->key, &prev);
            unlink_from_bucket(store, victim_idx, victim, prev);
            free(victim);
            store->size--;
        }
    }

    Node *node = malloc(sizeof(Node));
    if (node == NULL) {
        pthread_mutex_unlock(&store->lock);
        return -1;
    }

    strncpy(node->key, key, MAX_KEY_LEN - 1);
    node->key[MAX_KEY_LEN - 1] = '\0';
    strncpy(node->value, value, MAX_VALUE_LEN - 1);
    node->value[MAX_VALUE_LEN - 1] = '\0';
    node->lru_prev = NULL;
    node->lru_next = NULL;

    // Insert at bucket head.
    node->hash_next = store->buckets[idx];
    store->buckets[idx] = node;

    lru_push_front(store, node);
    store->size++;

    pthread_mutex_unlock(&store->lock);
    return 0;
}

int kv_get(KVStore *store, const char *key, char *value_out) {
    if (store == NULL || key == NULL || value_out == NULL) {
        return -1;
    }

    pthread_mutex_lock(&store->lock);

    size_t idx = bucket_index(store, key);
    Node *node = find_in_bucket(store, idx, key, NULL);

    if (node == NULL) {
        pthread_mutex_unlock(&store->lock);
        return -1;
    }

    // Copy out under the lock so the caller can never read a value
    // that's mid-eviction on another thread.
    strncpy(value_out, node->value, MAX_VALUE_LEN);
    lru_move_to_front(store, node);

    pthread_mutex_unlock(&store->lock);
    return 0;
}

int kv_delete(KVStore *store, const char *key) {
    if (store == NULL || key == NULL) {
        return -1;
    }

    pthread_mutex_lock(&store->lock);

    size_t idx = bucket_index(store, key);
    Node *prev = NULL;
    Node *node = find_in_bucket(store, idx, key, &prev);

    if (node == NULL) {
        pthread_mutex_unlock(&store->lock);
        return -1;
    }

    unlink_from_bucket(store, idx, node, prev);
    lru_remove(store, node);
    free(node);
    store->size--;

    pthread_mutex_unlock(&store->lock);
    return 0;
}

void kv_debug_print(KVStore *store) {
    printf("--- KVStore (size=%zu, capacity=%zu) ---\n", store->size, store->capacity);
    printf("LRU order (MRU -> LRU): ");
    Node *curr = store->lru_head;
    while (curr != NULL) {
        printf("[%s=%s] ", curr->key, curr->value);
        curr = curr->lru_next;
    }
    printf("\n");
}