#ifndef LRU_LIST_H
#define LRU_LIST_H

#include "kv_store.h"

// All functions here are UNSYNCHRONIZED — the caller must already hold
// store->lock before calling any of these. They only manipulate the
// lru_prev / lru_next pointers and store->lru_head / store->lru_tail.

// Insert node at the front (most-recently-used position).
// Assumes node is not currently in the list.
void lru_push_front(KVStore *store, Node *node);

// Unlink node from wherever it currently sits in the list.
// Safe no-op guard not included — caller must ensure node is in the list.
void lru_remove(KVStore *store, Node *node);

// Move an already-linked node to the front (used on cache hit).
void lru_move_to_front(KVStore *store, Node *node);

// Remove and return the least-recently-used node (the tail).
// Returns NULL if the list is empty.
Node *lru_pop_back(KVStore *store);

#endif