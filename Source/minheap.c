/**
 * @file minheap.c
 * @brief Array-backed binary min-heap with a hash-map position index.
 *
 * Array layout: for the node at index i,
 *   parent = (i - 1) / 2,  left child = 2i + 1,  right child = 2i + 2.
 */
#include "minheap.h"

#include <math.h>
#include <stdlib.h>

#include "hashmap.h"

#define MINHEAP_MIN_CAPACITY 8u

typedef struct {
    uint32_t id;
    double   key;
} HeapEntry;

struct MinHeap {
    HeapEntry *data;
    size_t     size;
    size_t     capacity;
    HashMap   *position; /* id -> index into data[], stored as uintptr_t */
};

/*
 * Array indices are stored in the generic void* map by round-tripping them
 * through uintptr_t, which is guaranteed to be wide enough for a pointer.
 */
static void *index_to_value(size_t index)
{
    return (void *)(uintptr_t)index;
}

static size_t value_to_index(void *value)
{
    return (size_t)(uintptr_t)value;
}

/* Strict ordering: smaller key first, then smaller id. */
static bool entry_less(const HeapEntry *a, const HeapEntry *b)
{
    if (a->key != b->key) {
        return a->key < b->key;
    }
    return a->id < b->id;
}

/*
 * Writes @p entry at index @p i and records the new position.
 * The id is already in the position map, so hashmap_put() only replaces
 * a value and cannot fail.
 */
static void place(MinHeap *heap, size_t i, HeapEntry entry)
{
    heap->data[i] = entry;
    (void)hashmap_put(heap->position, entry.id, index_to_value(i));
}

/* Moves the entry at @p i towards the root while it beats its parent. */
static void sift_up(MinHeap *heap, size_t i)
{
    HeapEntry moving = heap->data[i];
    while (i > 0) {
        size_t parent = (i - 1) / 2;
        if (!entry_less(&moving, &heap->data[parent])) {
            break;
        }
        place(heap, i, heap->data[parent]);
        i = parent;
    }
    place(heap, i, moving);
}

/* Moves the entry at @p i towards the leaves while a child beats it. */
static void sift_down(MinHeap *heap, size_t i)
{
    HeapEntry moving = heap->data[i];
    for (;;) {
        size_t left = 2 * i + 1;
        if (left >= heap->size) {
            break;
        }
        size_t child = left;
        size_t right = left + 1;
        if (right < heap->size && entry_less(&heap->data[right], &heap->data[left])) {
            child = right;
        }
        if (!entry_less(&heap->data[child], &moving)) {
            break;
        }
        place(heap, i, heap->data[child]);
        i = child;
    }
    place(heap, i, moving);
}

/* Restores the heap property after the entry at @p i changed arbitrarily. */
static void sift(MinHeap *heap, size_t i)
{
    if (i > 0 && entry_less(&heap->data[i], &heap->data[(i - 1) / 2])) {
        sift_up(heap, i);
    } else {
        sift_down(heap, i);
    }
}

/* Removes the entry at index @p i (used by pop and remove). */
static void remove_at(MinHeap *heap, size_t i)
{
    (void)hashmap_remove(heap->position, heap->data[i].id, NULL);
    heap->size--;
    if (i == heap->size) {
        return; /* removed the last element: nothing to fix */
    }
    /* Fill the hole with the last element, then let it settle. */
    place(heap, i, heap->data[heap->size]);
    sift(heap, i);
}

static bool lookup_index(const MinHeap *heap, uint32_t id, size_t *index)
{
    void *value = NULL;
    if (!hashmap_lookup(heap->position, id, &value)) {
        return false;
    }
    *index = value_to_index(value);
    return true;
}

MinHeap *minheap_create(size_t initial_capacity)
{
    if (initial_capacity < MINHEAP_MIN_CAPACITY) {
        initial_capacity = MINHEAP_MIN_CAPACITY;
    }
    MinHeap *heap = malloc(sizeof *heap);
    if (heap == NULL) {
        return NULL;
    }
    heap->data = tk_reallocarray(NULL, initial_capacity, sizeof *heap->data);
    heap->position = hashmap_create(initial_capacity);
    if (heap->data == NULL || heap->position == NULL) {
        free(heap->data);
        hashmap_destroy(heap->position);
        free(heap);
        return NULL;
    }
    heap->size = 0;
    heap->capacity = initial_capacity;
    return heap;
}

void minheap_destroy(MinHeap *heap)
{
    if (heap == NULL) {
        return;
    }
    free(heap->data);
    hashmap_destroy(heap->position);
    free(heap);
}

TkStatus minheap_push(MinHeap *heap, uint32_t id, double key)
{
    if (heap == NULL || isnan(key)) {
        return TK_ERR_INVALID;
    }
    if (hashmap_contains(heap->position, id)) {
        return TK_ERR_EXISTS;
    }

    /* Grow the array geometrically so push stays amortised O(log n). */
    if (heap->size == heap->capacity) {
        if (heap->capacity > SIZE_MAX / 2) {
            return TK_ERR_NOMEM;
        }
        size_t new_capacity = heap->capacity * 2;
        HeapEntry *grown = tk_reallocarray(heap->data, new_capacity, sizeof *grown);
        if (grown == NULL) {
            return TK_ERR_NOMEM;
        }
        heap->data = grown;
        heap->capacity = new_capacity;
    }

    /* Register the id first: this is the only step that can fail. */
    TkStatus st = hashmap_put(heap->position, id, index_to_value(heap->size));
    if (st != TK_OK) {
        return st;
    }
    heap->data[heap->size] = (HeapEntry){ .id = id, .key = key };
    heap->size++;
    sift_up(heap, heap->size - 1);
    return TK_OK;
}

TkStatus minheap_peek(const MinHeap *heap, uint32_t *id, double *key)
{
    if (heap == NULL) {
        return TK_ERR_INVALID;
    }
    if (heap->size == 0) {
        return TK_ERR_EMPTY;
    }
    if (id != NULL) {
        *id = heap->data[0].id;
    }
    if (key != NULL) {
        *key = heap->data[0].key;
    }
    return TK_OK;
}

TkStatus minheap_pop(MinHeap *heap, uint32_t *id, double *key)
{
    TkStatus st = minheap_peek(heap, id, key);
    if (st == TK_OK) {
        remove_at(heap, 0);
    }
    return st;
}

TkStatus minheap_update(MinHeap *heap, uint32_t id, double new_key)
{
    if (heap == NULL || isnan(new_key)) {
        return TK_ERR_INVALID;
    }
    size_t i = 0;
    if (!lookup_index(heap, id, &i)) {
        return TK_ERR_NOT_FOUND;
    }
    heap->data[i].key = new_key;
    sift(heap, i);
    return TK_OK;
}

TkStatus minheap_remove(MinHeap *heap, uint32_t id)
{
    if (heap == NULL) {
        return TK_ERR_INVALID;
    }
    size_t i = 0;
    if (!lookup_index(heap, id, &i)) {
        return TK_ERR_NOT_FOUND;
    }
    remove_at(heap, i);
    return TK_OK;
}

bool minheap_contains(const MinHeap *heap, uint32_t id)
{
    return heap != NULL && hashmap_contains(heap->position, id);
}

TkStatus minheap_get_key(const MinHeap *heap, uint32_t id, double *key)
{
    if (heap == NULL || key == NULL) {
        return TK_ERR_INVALID;
    }
    size_t i = 0;
    if (!lookup_index(heap, id, &i)) {
        return TK_ERR_NOT_FOUND;
    }
    *key = heap->data[i].key;
    return TK_OK;
}

size_t minheap_size(const MinHeap *heap)
{
    return (heap == NULL) ? 0 : heap->size;
}

/* ---- peek_k: a tiny auxiliary heap of *indices* into heap->data ---- */

static bool index_less(const MinHeap *heap, size_t a, size_t b)
{
    return entry_less(&heap->data[a], &heap->data[b]);
}

static void candidates_push(const MinHeap *heap, size_t *cand, size_t *n, size_t index)
{
    size_t i = (*n)++;
    cand[i] = index;
    while (i > 0 && index_less(heap, cand[i], cand[(i - 1) / 2])) {
        size_t parent = (i - 1) / 2;
        size_t tmp = cand[i];
        cand[i] = cand[parent];
        cand[parent] = tmp;
        i = parent;
    }
}

static size_t candidates_pop(const MinHeap *heap, size_t *cand, size_t *n)
{
    size_t top = cand[0];
    cand[0] = cand[--(*n)];
    size_t i = 0;
    for (;;) {
        size_t smallest = i;
        size_t left = 2 * i + 1;
        size_t right = left + 1;
        if (left < *n && index_less(heap, cand[left], cand[smallest])) {
            smallest = left;
        }
        if (right < *n && index_less(heap, cand[right], cand[smallest])) {
            smallest = right;
        }
        if (smallest == i) {
            break;
        }
        size_t tmp = cand[i];
        cand[i] = cand[smallest];
        cand[smallest] = tmp;
        i = smallest;
    }
    return top;
}

/*
 * The k smallest elements of a min-heap form a connected subtree at the
 * root. We explore it best-first: start with the root as the only
 * candidate; repeatedly emit the smallest candidate and add its two
 * children. Each step adds at most one net candidate, so the auxiliary
 * heap never exceeds 2k + 1 entries, giving O(k log k) total.
 */
TkStatus minheap_peek_k(const MinHeap *heap, size_t k,
                        uint32_t *ids, double *keys, size_t *written)
{
    if (heap == NULL || written == NULL) {
        return TK_ERR_INVALID;
    }
    *written = 0;
    if (k == 0 || heap->size == 0) {
        return TK_OK;
    }
    if (k > heap->size) {
        k = heap->size;
    }

    if (k > (SIZE_MAX - 1) / 2) {
        return TK_ERR_NOMEM;
    }
    size_t *cand = tk_reallocarray(NULL, 2 * k + 1, sizeof *cand);
    if (cand == NULL) {
        return TK_ERR_NOMEM;
    }

    size_t n = 0;
    candidates_push(heap, cand, &n, 0);
    while (*written < k && n > 0) {
        size_t top = candidates_pop(heap, cand, &n);
        if (ids != NULL) {
            ids[*written] = heap->data[top].id;
        }
        if (keys != NULL) {
            keys[*written] = heap->data[top].key;
        }
        (*written)++;

        size_t left = 2 * top + 1;
        if (left < heap->size) {
            candidates_push(heap, cand, &n, left);
        }
        if (left + 1 < heap->size) {
            candidates_push(heap, cand, &n, left + 1);
        }
    }
    free(cand);
    return TK_OK;
}
