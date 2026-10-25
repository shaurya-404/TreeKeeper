/**
 * @file registry.h
 * @brief Thread-safe tree registry: HashMap for lookup, MinHeap for urgency.
 *
 * Two data structures index the same tree records:
 *
 *   HashMap  id -> TreeRecord*       O(1) lookup / update by Tree ID
 *                                    (what a QR-code scan resolves to)
 *   MinHeap  id keyed by "dry-out"   O(log n) "which tree is most urgent?"
 *            time (sim hours)
 *
 * Why the heap key is a dry-out TIME rather than the water level:
 * every tree loses water at its own constant rate r, so its tank empties
 * at  t_dry = t_now + water / r.  As the clock advances, water drops but
 * t_dry stays the same. The heap therefore never has to be re-ordered on
 * each simulation tick; keys change only when a tree is watered, planted
 * or removed. Ordering by water level alone would also be wrong: a Bamboo
 * at 40% may dry out before a Neem at 30%.
 *
 * Trees assigned to a volunteer leave the heap (they are "in progress")
 * and rejoin it when watered or released.
 *
 * All public functions lock an internal mutex, so the simulation thread
 * and the UI thread can call them concurrently. Tree data is only ever
 * handed out as TreeInfo *copies*; no pointer to internal state escapes.
 */
#ifndef REGISTRY_H
#define REGISTRY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "tk_common.h"

/** Water level thresholds (percent) used to derive a tree's status. */
#define TREE_WATER_FULL      100.0
#define TREE_THIRSTY_BELOW    60.0
#define TREE_CRITICAL_BELOW   30.0

/** Saplings younger than this lose water faster (shallow roots). */
#define TREE_YOUNG_HOURS     (24.0 * 30.0)
#define TREE_YOUNG_FACTOR     1.25

typedef enum {
    SPECIES_NEEM = 0,
    SPECIES_BANYAN,
    SPECIES_PEEPAL,
    SPECIES_MANGO,
    SPECIES_TEAK,
    SPECIES_BAMBOO,
    SPECIES_GULMOHAR,
    SPECIES_ARJUNA,
    SPECIES_COUNT
} Species;

typedef enum {
    TREE_HEALTHY = 0, /* water >= 60%                  */
    TREE_THIRSTY,     /* 30% <= water < 60%            */
    TREE_CRITICAL,    /* 0% < water < 30%              */
    TREE_WILTED,      /* water == 0%                   */
    TREE_DISEASED,    /* flagged manually by volunteer */
    TREE_STATUS_COUNT
} TreeStatus;

/** Public, copyable view of one tree. */
typedef struct {
    uint32_t   id;
    Species    species;
    int        zone_id;
    double     water_level;        /* percent, 0..100                      */
    double     decay_rate;         /* percent lost per simulated hour      */
    double     planted_at;         /* sim hours                            */
    double     last_watered;       /* sim hours                            */
    bool       diseased;
    int        assigned_volunteer; /* volunteer id, or -1 if unassigned    */
    /* Derived fields (recomputed by the registry, ignored on restore): */
    TreeStatus status;
    double     hours_to_dry;
} TreeInfo;

/** Aggregate counters for the dashboard. */
typedef struct {
    size_t total;
    size_t queued;   /* waiting in the min-heap        */
    size_t assigned; /* currently given to a volunteer */
    size_t by_status[TREE_STATUS_COUNT];
} RegistryStats;

typedef struct TreeRegistry TreeRegistry;

/* ---- Species / status helpers ---- */

const char *species_name(Species species);
/** Parses a species name (case-insensitive). */
TkStatus species_parse(const char *text, Species *out);
/** Base water loss in percent per simulated hour. */
double species_base_decay(Species species);
const char *tree_status_name(TreeStatus status);

/* ---- Lifecycle ---- */

TreeRegistry *registry_create(void);
/** Frees every tree record. Safe to call with NULL. */
void registry_destroy(TreeRegistry *reg);

/* ---- Mutations ---- */

/**
 * Plants a new tree in @p zone_id with @p water percent in its tank.
 * The registry does not know the map; callers validate the zone first.
 */
TkStatus registry_plant(TreeRegistry *reg, Species species, int zone_id,
                        double water, uint32_t *out_id);

/** Re-inserts a tree loaded from disk (keeps its id). */
TkStatus registry_restore(TreeRegistry *reg, const TreeInfo *info);

/**
 * Records a watering (a QR scan at the tree guard): refills the tank,
 * clears any assignment and requeues the tree. The volunteer that held the
 * task (or -1) is written to @p previous_volunteer if non-NULL.
 */
TkStatus registry_water(TreeRegistry *reg, uint32_t id, int *previous_volunteer);

/** Sets or clears the manual "diseased" flag. */
TkStatus registry_set_diseased(TreeRegistry *reg, uint32_t id, bool diseased);

/** Deletes a tree entirely. */
TkStatus registry_remove(TreeRegistry *reg, uint32_t id);

/**
 * Pops the most urgent queued tree and assigns it to @p volunteer.
 * Returns TK_ERR_EMPTY when no unassigned tree is waiting.
 */
TkStatus registry_assign_next(TreeRegistry *reg, int volunteer, TreeInfo *out);

/** Puts an assigned tree back into the queue without watering it. */
TkStatus registry_release(TreeRegistry *reg, uint32_t id);

/**
 * Advances the simulation clock by @p hours, draining every tree's water.
 * Called from the background simulation thread.
 */
void registry_advance(TreeRegistry *reg, double hours);

/* ---- Queries ---- */

TkStatus registry_get(TreeRegistry *reg, uint32_t id, TreeInfo *out);
size_t registry_count(TreeRegistry *reg);
void registry_stats(TreeRegistry *reg, RegistryStats *out);

/**
 * Copies the @p k most urgent queued trees into @p out (ascending dry-out
 * time) without dequeuing them. Count written goes to @p written.
 */
TkStatus registry_most_urgent(TreeRegistry *reg, size_t k, TreeInfo *out, size_t *written);

/**
 * Returns a malloc'd array with a copy of every tree, sorted by id.
 * The caller must free(*out). *out is NULL when the registry is empty.
 */
TkStatus registry_snapshot(TreeRegistry *reg, TreeInfo **out, size_t *count);

/* ---- Clock / id bookkeeping (used by persistence) ---- */

double registry_clock(TreeRegistry *reg);
void registry_set_clock(TreeRegistry *reg, double hours);
uint32_t registry_next_id(TreeRegistry *reg);
/** Raises the next id to at least @p next_id (never lowers it). */
void registry_reserve_id(TreeRegistry *reg, uint32_t next_id);

#endif /* REGISTRY_H */
