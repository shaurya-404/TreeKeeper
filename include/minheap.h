/**
 * @file minheap.h
 * @brief Indexed binary min-heap (priority queue) of (id, key) pairs.
 *
 * A plain binary heap cannot change the priority of an arbitrary element
 * without an O(n) search. This heap keeps a HashMap from id -> array index
 * alongside the array, so it can locate any element in O(1) and then
 * restore the heap property in O(log n):
 *
 *   push / pop / update / remove : O(log n)
 *   peek / contains / get_key     : O(1)
 *   peek_k (k smallest, no pop)   : O(k log k)
 *
 * Ties on equal keys are broken by the smaller id, so ordering is fully
 * deterministic (useful for tests and reproducible demos).
 */
#ifndef MINHEAP_H
#define MINHEAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "tk_common.h"

typedef struct MinHeap MinHeap;

/** Creates an empty heap with room for @p initial_capacity entries. */
MinHeap *minheap_create(size_t initial_capacity);

/** Frees the heap. Safe to call with NULL. */
void minheap_destroy(MinHeap *heap);

/** Inserts a new id. Returns TK_ERR_EXISTS if @p id is already queued. */
TkStatus minheap_push(MinHeap *heap, uint32_t id, double key);

/** Reads the minimum without removing it. TK_ERR_EMPTY if the heap is empty. */
TkStatus minheap_peek(const MinHeap *heap, uint32_t *id, double *key);

/** Removes and returns the minimum. TK_ERR_EMPTY if the heap is empty. */
TkStatus minheap_pop(MinHeap *heap, uint32_t *id, double *key);

/** Changes the key of a queued id (decrease-key or increase-key). */
TkStatus minheap_update(MinHeap *heap, uint32_t id, double new_key);

/** Removes an arbitrary queued id. */
TkStatus minheap_remove(MinHeap *heap, uint32_t id);

/** Returns true if @p id is currently queued. */
bool minheap_contains(const MinHeap *heap, uint32_t id);

/** Reads the key of a queued id. */
TkStatus minheap_get_key(const MinHeap *heap, uint32_t id, double *key);

/** Number of queued entries. */
size_t minheap_size(const MinHeap *heap);

/**
 * Copies the @p k smallest entries in ascending order WITHOUT modifying
 * the heap. Either output array may be NULL. The number of entries
 * written (<= k) is stored in @p written.
 */
TkStatus minheap_peek_k(const MinHeap *heap, size_t k,
                        uint32_t *ids, double *keys, size_t *written);

#endif /* MINHEAP_H */
