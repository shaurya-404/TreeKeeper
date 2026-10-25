/**
 * @file hashmap.c
 * @brief Linear-probing hash map with tombstones and dynamic resizing.
 */
#include "hashmap.h"

#include <stdlib.h>
#include <string.h>

#define HASHMAP_MIN_CAPACITY 8u
/* Resize when (count + tombstones) / capacity would exceed 7/10. */
#define HASHMAP_LOAD_NUM 7u
#define HASHMAP_LOAD_DEN 10u

typedef enum {
    SLOT_EMPTY = 0, /* never used: terminates a probe sequence          */
    SLOT_OCCUPIED,  /* holds a live key/value pair                      */
    SLOT_TOMBSTONE  /* was deleted: probing must continue past it       */
} SlotState;

typedef struct {
    uint32_t  key;
    SlotState state;
    void     *value;
} Slot;

struct HashMap {
    Slot  *slots;
    size_t capacity;   /* always a power of two */
    size_t count;      /* live entries          */
    size_t tombstones; /* deleted entries       */
};

/*
 * MurmurHash3 32-bit finaliser. Sequential tree IDs (1001, 1002, ...)
 * would cluster badly with "key % cap"; this mixes every input bit into
 * every output bit so the table stays evenly filled.
 */
static uint32_t hash_key(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x85ebca6bu;
    x ^= x >> 13;
    x *= 0xc2b2ae35u;
    x ^= x >> 16;
    return x;
}

/* Smallest power of two >= requested (and >= the minimum), or 0 on overflow. */
static size_t capacity_for(size_t requested)
{
    size_t cap = HASHMAP_MIN_CAPACITY;
    while (cap < requested) {
        if (cap > (SIZE_MAX / sizeof(Slot)) / 2) {
            return 0;
        }
        cap <<= 1;
    }
    return cap;
}

/* Returns the index of the slot holding @p key, or SIZE_MAX if absent. */
static size_t find_existing(const HashMap *map, uint32_t key)
{
    size_t mask = map->capacity - 1;
    size_t i = hash_key(key) & mask;

    for (size_t probes = 0; probes < map->capacity; probes++) {
        const Slot *slot = &map->slots[i];
        if (slot->state == SLOT_EMPTY) {
            return SIZE_MAX;
        }
        if (slot->state == SLOT_OCCUPIED && slot->key == key) {
            return i;
        }
        i = (i + 1) & mask;
    }
    return SIZE_MAX;
}

/*
 * Returns the first non-occupied slot on @p key's probe sequence.
 * Precondition: the key is not present and the table has a free slot.
 */
static size_t find_free(const Slot *slots, size_t capacity, uint32_t key)
{
    size_t mask = capacity - 1;
    size_t i = hash_key(key) & mask;
    while (slots[i].state == SLOT_OCCUPIED) {
        i = (i + 1) & mask;
    }
    return i;
}

/* Moves every live entry into a fresh table, discarding tombstones. */
static TkStatus rehash(HashMap *map, size_t new_capacity)
{
    Slot *fresh = calloc(new_capacity, sizeof *fresh);
    if (fresh == NULL) {
        return TK_ERR_NOMEM;
    }
    for (size_t i = 0; i < map->capacity; i++) {
        if (map->slots[i].state == SLOT_OCCUPIED) {
            size_t j = find_free(fresh, new_capacity, map->slots[i].key);
            fresh[j] = map->slots[i];
        }
    }
    free(map->slots);
    map->slots = fresh;
    map->capacity = new_capacity;
    map->tombstones = 0;
    return TK_OK;
}

HashMap *hashmap_create(size_t expected)
{
    /* Size the table so @p expected entries stay under the load limit. */
    size_t wanted = (expected / HASHMAP_LOAD_NUM + 1) * HASHMAP_LOAD_DEN;
    size_t capacity = capacity_for(wanted);
    if (capacity == 0) {
        return NULL;
    }

    HashMap *map = malloc(sizeof *map);
    if (map == NULL) {
        return NULL;
    }
    map->slots = calloc(capacity, sizeof *map->slots);
    if (map->slots == NULL) {
        free(map);
        return NULL;
    }
    map->capacity = capacity;
    map->count = 0;
    map->tombstones = 0;
    return map;
}

void hashmap_destroy(HashMap *map)
{
    if (map == NULL) {
        return;
    }
    free(map->slots);
    free(map);
}

TkStatus hashmap_put(HashMap *map, uint32_t key, void *value)
{
    if (map == NULL) {
        return TK_ERR_INVALID;
    }

    /* Replacing an existing key: no allocation, cannot fail. */
    size_t idx = find_existing(map, key);
    if (idx != SIZE_MAX) {
        map->slots[idx].value = value;
        return TK_OK;
    }

    /* New key: grow (or just purge tombstones) before crossing 70% load. */
    size_t used_after = map->count + map->tombstones + 1;
    if (used_after * HASHMAP_LOAD_DEN > map->capacity * HASHMAP_LOAD_NUM) {
        size_t new_capacity = map->capacity;
        /* Double only if live entries alone would be over half the limit. */
        if ((map->count + 1) * HASHMAP_LOAD_DEN * 2 > map->capacity * HASHMAP_LOAD_NUM) {
            new_capacity = capacity_for(map->capacity * 2);
            if (new_capacity == 0) {
                return TK_ERR_NOMEM;
            }
        }
        TkStatus st = rehash(map, new_capacity);
        if (st != TK_OK) {
            return st;
        }
    }

    idx = find_free(map->slots, map->capacity, key);
    if (map->slots[idx].state == SLOT_TOMBSTONE) {
        map->tombstones--;
    }
    map->slots[idx].key = key;
    map->slots[idx].value = value;
    map->slots[idx].state = SLOT_OCCUPIED;
    map->count++;
    return TK_OK;
}

bool hashmap_lookup(const HashMap *map, uint32_t key, void **out)
{
    if (map == NULL) {
        return false;
    }
    size_t idx = find_existing(map, key);
    if (idx == SIZE_MAX) {
        return false;
    }
    if (out != NULL) {
        *out = map->slots[idx].value;
    }
    return true;
}

void *hashmap_get(const HashMap *map, uint32_t key)
{
    void *value = NULL;
    return hashmap_lookup(map, key, &value) ? value : NULL;
}

bool hashmap_contains(const HashMap *map, uint32_t key)
{
    return hashmap_lookup(map, key, NULL);
}

TkStatus hashmap_remove(HashMap *map, uint32_t key, void **old)
{
    if (map == NULL) {
        return TK_ERR_INVALID;
    }
    size_t idx = find_existing(map, key);
    if (idx == SIZE_MAX) {
        return TK_ERR_NOT_FOUND;
    }
    if (old != NULL) {
        *old = map->slots[idx].value;
    }
    map->slots[idx].state = SLOT_TOMBSTONE;
    map->slots[idx].value = NULL;
    map->count--;
    map->tombstones++;
    return TK_OK;
}

size_t hashmap_size(const HashMap *map)
{
    return (map == NULL) ? 0 : map->count;
}

void hashmap_foreach(const HashMap *map, HashMapVisitor visit, void *ctx)
{
    if (map == NULL || visit == NULL) {
        return;
    }
    for (size_t i = 0; i < map->capacity; i++) {
        const Slot *slot = &map->slots[i];
        if (slot->state == SLOT_OCCUPIED && !visit(slot->key, slot->value, ctx)) {
            return;
        }
    }
}

void hashmap_clear(HashMap *map, void (*free_value)(void *))
{
    if (map == NULL) {
        return;
    }
    if (free_value != NULL) {
        for (size_t i = 0; i < map->capacity; i++) {
            if (map->slots[i].state == SLOT_OCCUPIED) {
                free_value(map->slots[i].value);
            }
        }
    }
    memset(map->slots, 0, map->capacity * sizeof *map->slots);
    map->count = 0;
    map->tombstones = 0;
}
