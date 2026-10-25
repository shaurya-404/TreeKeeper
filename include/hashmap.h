/**
 * @file hashmap.h
 * @brief Open-addressing hash map from uint32_t keys to void* values.
 *
 * Design:
 *   - Linear probing over a power-of-two table (index = hash & (cap - 1)).
 *   - Deleted slots become "tombstones" so probe chains stay intact.
 *   - The table doubles when (live + tombstones) exceeds 70% load, which
 *     keeps probe sequences short: expected O(1) get / put / remove.
 *
 * The map never owns its values; callers free them (see hashmap_clear()).
 */
#ifndef HASHMAP_H
#define HASHMAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "tk_common.h"

typedef struct HashMap HashMap;

/**
 * Visitor used by hashmap_foreach(). Return true to keep iterating or
 * false to stop early. The visitor must not insert into or remove from
 * the map it is visiting.
 */
typedef bool (*HashMapVisitor)(uint32_t key, void *value, void *ctx);

/** Creates a map able to hold about @p expected entries before resizing. */
HashMap *hashmap_create(size_t expected);

/** Frees the table (not the values). Safe to call with NULL. */
void hashmap_destroy(HashMap *map);

/**
 * Inserts @p key or replaces its value. Replacing an existing key never
 * allocates and therefore cannot fail (the min-heap relies on this).
 */
TkStatus hashmap_put(HashMap *map, uint32_t key, void *value);

/** Looks up @p key. Returns true and stores the value in @p out if found. */
bool hashmap_lookup(const HashMap *map, uint32_t key, void **out);

/** Convenience wrapper: returns the value or NULL when absent. */
void *hashmap_get(const HashMap *map, uint32_t key);

/** Returns true when @p key is present. */
bool hashmap_contains(const HashMap *map, uint32_t key);

/** Removes @p key. The old value is returned through @p old (may be NULL). */
TkStatus hashmap_remove(HashMap *map, uint32_t key, void **old);

/** Number of live entries. */
size_t hashmap_size(const HashMap *map);

/** Calls @p visit for every entry in unspecified order. */
void hashmap_foreach(const HashMap *map, HashMapVisitor visit, void *ctx);

/** Removes every entry, calling @p free_value on each value if non-NULL. */
void hashmap_clear(HashMap *map, void (*free_value)(void *));

#endif /* HASHMAP_H */
