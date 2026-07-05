#include "lru_list.h"
#include <stddef.h>

void lru_push_front(KVStore *store, Node *node) {
    node->lru_prev = NULL;
    node->lru_next = store->lru_head;

    if (store->lru_head != NULL) {
        store->lru_head->lru_prev = node;
    }
    store->lru_head = node;

    if (store->lru_tail == NULL) {
        // list was empty, node is both head and tail
        store->lru_tail = node;
    }
}

void lru_remove(KVStore *store, Node *node) {
    if (node->lru_prev != NULL) {
        node->lru_prev->lru_next = node->lru_next;
    } else {
        // node was the head
        store->lru_head = node->lru_next;
    }

    if (node->lru_next != NULL) {
        node->lru_next->lru_prev = node->lru_prev;
    } else {
        // node was the tail
        store->lru_tail = node->lru_prev;
    }

    node->lru_prev = NULL;
    node->lru_next = NULL;
}

void lru_move_to_front(KVStore *store, Node *node) {
    if (store->lru_head == node) {
        // already at front, nothing to do
        return;
    }
    lru_remove(store, node);
    lru_push_front(store, node);
}

Node *lru_pop_back(KVStore *store) {
    Node *tail = store->lru_tail;
    if (tail == NULL) {
        return NULL;
    }
    lru_remove(store, tail);
    return tail;
}