/**
 * @file registry.c
 * @brief Tree registry combining a HashMap (by id) and a MinHeap (by urgency).
 */
#include "registry.h"

#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "hashmap.h"
#include "minheap.h"

#define REGISTRY_FIRST_ID 1001u
#define REGISTRY_INITIAL_CAPACITY 64u

struct TreeRegistry {
    pthread_mutex_t lock;
    HashMap        *trees;   /* id -> TreeInfo* (owned by the registry)  */
    MinHeap        *queue;   /* unassigned trees keyed by dry-out time   */
    double          clock;   /* simulated hours since the project began  */
    uint32_t        next_id;
};

/* ---- Species table ---- */

typedef struct {
    const char *name;
    double      decay; /* % water lost per simulated hour */
} SpeciesInfo;

static const SpeciesInfo SPECIES_TABLE[SPECIES_COUNT] = {
    [SPECIES_NEEM]     = { "Neem",     1.2 }, /* drought hardy        */
    [SPECIES_BANYAN]   = { "Banyan",   1.6 },
    [SPECIES_PEEPAL]   = { "Peepal",   1.5 },
    [SPECIES_MANGO]    = { "Mango",    2.2 },
    [SPECIES_TEAK]     = { "Teak",     1.8 },
    [SPECIES_BAMBOO]   = { "Bamboo",   2.8 }, /* shallow, thirsty     */
    [SPECIES_GULMOHAR] = { "Gulmohar", 2.0 },
    [SPECIES_ARJUNA]   = { "Arjuna",   1.7 },
};

static const char *const STATUS_NAMES[TREE_STATUS_COUNT] = {
    [TREE_HEALTHY]  = "HEALTHY",
    [TREE_THIRSTY]  = "THIRSTY",
    [TREE_CRITICAL] = "CRITICAL",
    [TREE_WILTED]   = "WILTED",
    [TREE_DISEASED] = "DISEASED",
};

const char *species_name(Species species)
{
    return ((unsigned)species < SPECIES_COUNT) ? SPECIES_TABLE[species].name : "Unknown";
}

TkStatus species_parse(const char *text, Species *out)
{
    if (text == NULL || out == NULL) {
        return TK_ERR_INVALID;
    }
    for (int i = 0; i < SPECIES_COUNT; i++) {
        if (strcasecmp(text, SPECIES_TABLE[i].name) == 0) {
            *out = (Species)i;
            return TK_OK;
        }
    }
    return TK_ERR_NOT_FOUND;
}

double species_base_decay(Species species)
{
    return ((unsigned)species < SPECIES_COUNT) ? SPECIES_TABLE[species].decay : 0.0;
}

const char *tree_status_name(TreeStatus status)
{
    return ((unsigned)status < TREE_STATUS_COUNT) ? STATUS_NAMES[status] : "UNKNOWN";
}

/* ---- Internal helpers (caller holds the lock) ---- */

#define LOCK(reg)   pthread_mutex_lock(&(reg)->lock)
#define UNLOCK(reg) pthread_mutex_unlock(&(reg)->lock)

static double decay_rate_for(Species species, double age_hours)
{
    double rate = SPECIES_TABLE[species].decay;
    return (age_hours < TREE_YOUNG_HOURS) ? rate * TREE_YOUNG_FACTOR : rate;
}

static TreeStatus derive_status(const TreeInfo *tree)
{
    if (tree->diseased) {
        return TREE_DISEASED;
    }
    if (tree->water_level <= 0.0) {
        return TREE_WILTED;
    }
    if (tree->water_level < TREE_CRITICAL_BELOW) {
        return TREE_CRITICAL;
    }
    if (tree->water_level < TREE_THIRSTY_BELOW) {
        return TREE_THIRSTY;
    }
    return TREE_HEALTHY;
}

static void refresh_derived(TreeInfo *tree)
{
    tree->status = derive_status(tree);
    tree->hours_to_dry = tree->water_level / tree->decay_rate;
}

/* Heap key: the simulated hour at which this tree's tank runs dry. */
static double dry_out_time(const TreeRegistry *reg, const TreeInfo *tree)
{
    return reg->clock + tree->water_level / tree->decay_rate;
}

static TreeInfo *find_tree(const TreeRegistry *reg, uint32_t id)
{
    return hashmap_get(reg->trees, id);
}

/* Stores a new record in both indexes, all-or-nothing. */
static TkStatus insert_tree(TreeRegistry *reg, TreeInfo *tree)
{
    TkStatus st = hashmap_put(reg->trees, tree->id, tree);
    if (st != TK_OK) {
        return st;
    }
    if (tree->assigned_volunteer < 0) {
        st = minheap_push(reg->queue, tree->id, dry_out_time(reg, tree));
        if (st != TK_OK) {
            (void)hashmap_remove(reg->trees, tree->id, NULL);
        }
    }
    return st;
}

/* ---- Lifecycle ---- */

TreeRegistry *registry_create(void)
{
    TreeRegistry *reg = calloc(1, sizeof *reg);
    if (reg == NULL) {
        return NULL;
    }
    if (pthread_mutex_init(&reg->lock, NULL) != 0) {
        free(reg);
        return NULL;
    }
    reg->trees = hashmap_create(REGISTRY_INITIAL_CAPACITY);
    reg->queue = minheap_create(REGISTRY_INITIAL_CAPACITY);
    if (reg->trees == NULL || reg->queue == NULL) {
        registry_destroy(reg);
        return NULL;
    }
    reg->clock = 0.0;
    reg->next_id = REGISTRY_FIRST_ID;
    return reg;
}

void registry_destroy(TreeRegistry *reg)
{
    if (reg == NULL) {
        return;
    }
    hashmap_clear(reg->trees, free);
    hashmap_destroy(reg->trees);
    minheap_destroy(reg->queue);
    pthread_mutex_destroy(&reg->lock);
    free(reg);
}

/* ---- Mutations ---- */

TkStatus registry_plant(TreeRegistry *reg, Species species, int zone_id,
                        double water, uint32_t *out_id)
{
    if (reg == NULL || (unsigned)species >= SPECIES_COUNT || zone_id < 0 ||
        !isfinite(water) || water < 0.0 || water > TREE_WATER_FULL) {
        return TK_ERR_INVALID;
    }

    TreeInfo *tree = calloc(1, sizeof *tree);
    if (tree == NULL) {
        return TK_ERR_NOMEM;
    }

    LOCK(reg);
    if (reg->next_id == UINT32_MAX) {
        UNLOCK(reg);
        free(tree);
        return TK_ERR_NOMEM; /* id space exhausted */
    }
    tree->id = reg->next_id;
    tree->species = species;
    tree->zone_id = zone_id;
    tree->water_level = water;
    tree->planted_at = reg->clock;
    tree->last_watered = reg->clock;
    tree->decay_rate = decay_rate_for(species, 0.0);
    tree->diseased = false;
    tree->assigned_volunteer = -1;
    refresh_derived(tree);

    TkStatus st = insert_tree(reg, tree);
    if (st == TK_OK) {
        reg->next_id++;
        if (out_id != NULL) {
            *out_id = tree->id;
        }
    }
    UNLOCK(reg);

    if (st != TK_OK) {
        free(tree);
    }
    return st;
}

TkStatus registry_restore(TreeRegistry *reg, const TreeInfo *info)
{
    if (reg == NULL || info == NULL || info->id == 0 || info->id == UINT32_MAX ||
        (unsigned)info->species >= SPECIES_COUNT || info->zone_id < 0 ||
        !isfinite(info->water_level) || info->water_level < 0.0 ||
        info->water_level > TREE_WATER_FULL ||
        !isfinite(info->decay_rate) || info->decay_rate <= 0.0 ||
        !isfinite(info->planted_at) || !isfinite(info->last_watered)) {
        return TK_ERR_INVALID;
    }

    TreeInfo *tree = malloc(sizeof *tree);
    if (tree == NULL) {
        return TK_ERR_NOMEM;
    }
    *tree = *info;
    if (tree->assigned_volunteer < -1) {
        tree->assigned_volunteer = -1;
    }
    refresh_derived(tree);

    LOCK(reg);
    TkStatus st = hashmap_contains(reg->trees, tree->id) ? TK_ERR_EXISTS : insert_tree(reg, tree);
    if (st == TK_OK && tree->id >= reg->next_id) {
        reg->next_id = tree->id + 1;
    }
    UNLOCK(reg);

    if (st != TK_OK) {
        free(tree);
    }
    return st;
}

TkStatus registry_water(TreeRegistry *reg, uint32_t id, int *previous_volunteer)
{
    if (reg == NULL) {
        return TK_ERR_INVALID;
    }

    LOCK(reg);
    TreeInfo *tree = find_tree(reg, id);
    if (tree == NULL) {
        UNLOCK(reg);
        return TK_ERR_NOT_FOUND;
    }

    TreeInfo before = *tree;
    tree->water_level = TREE_WATER_FULL;
    tree->last_watered = reg->clock;
    /* Young saplings keep the faster decay rate until they mature. */
    tree->decay_rate = decay_rate_for(tree->species, reg->clock - tree->planted_at);
    tree->assigned_volunteer = -1;

    double key = dry_out_time(reg, tree);
    TkStatus st = minheap_contains(reg->queue, id)
                      ? minheap_update(reg->queue, id, key)
                      : minheap_push(reg->queue, id, key);
    if (st != TK_OK) {
        *tree = before; /* roll back: the record must match the heap */
    } else {
        refresh_derived(tree);
        if (previous_volunteer != NULL) {
            *previous_volunteer = before.assigned_volunteer;
        }
    }
    UNLOCK(reg);
    return st;
}

TkStatus registry_set_diseased(TreeRegistry *reg, uint32_t id, bool diseased)
{
    if (reg == NULL) {
        return TK_ERR_INVALID;
    }
    LOCK(reg);
    TreeInfo *tree = find_tree(reg, id);
    if (tree != NULL) {
        tree->diseased = diseased;
        refresh_derived(tree);
    }
    UNLOCK(reg);
    return (tree != NULL) ? TK_OK : TK_ERR_NOT_FOUND;
}

TkStatus registry_remove(TreeRegistry *reg, uint32_t id)
{
    if (reg == NULL) {
        return TK_ERR_INVALID;
    }
    LOCK(reg);
    void *old = NULL;
    TkStatus st = hashmap_remove(reg->trees, id, &old);
    if (st == TK_OK) {
        (void)minheap_remove(reg->queue, id); /* absent if it was assigned */
    }
    UNLOCK(reg);
    free(old);
    return st;
}

TkStatus registry_assign_next(TreeRegistry *reg, int volunteer, TreeInfo *out)
{
    if (reg == NULL || volunteer < 0) {
        return TK_ERR_INVALID;
    }
    LOCK(reg);
    uint32_t id = 0;
    TkStatus st = minheap_pop(reg->queue, &id, NULL);
    if (st == TK_OK) {
        TreeInfo *tree = find_tree(reg, id);
        tree->assigned_volunteer = volunteer;
        if (out != NULL) {
            *out = *tree;
        }
    }
    UNLOCK(reg);
    return st;
}

TkStatus registry_release(TreeRegistry *reg, uint32_t id)
{
    if (reg == NULL) {
        return TK_ERR_INVALID;
    }
    LOCK(reg);
    TreeInfo *tree = find_tree(reg, id);
    TkStatus st = TK_ERR_NOT_FOUND;
    if (tree != NULL) {
        if (tree->assigned_volunteer < 0) {
            st = TK_ERR_INVALID; /* not assigned: nothing to release */
        } else {
            st = minheap_push(reg->queue, id, dry_out_time(reg, tree));
            if (st == TK_OK) {
                tree->assigned_volunteer = -1;
            }
        }
    }
    UNLOCK(reg);
    return st;
}

typedef struct {
    double hours;
} AdvanceCtx;

static bool advance_visitor(uint32_t key, void *value, void *ctx)
{
    (void)key;
    TreeInfo *tree = value;
    const AdvanceCtx *adv = ctx;
    tree->water_level -= tree->decay_rate * adv->hours;
    if (tree->water_level < 0.0) {
        tree->water_level = 0.0;
    }
    refresh_derived(tree);
    return true;
}

void registry_advance(TreeRegistry *reg, double hours)
{
    if (reg == NULL || !isfinite(hours) || hours <= 0.0) {
        return;
    }
    LOCK(reg);
    reg->clock += hours;
    AdvanceCtx ctx = { .hours = hours };
    /* O(n) water update; heap keys (dry-out times) are unaffected. */
    hashmap_foreach(reg->trees, advance_visitor, &ctx);
    UNLOCK(reg);
}

/* ---- Queries ---- */

TkStatus registry_get(TreeRegistry *reg, uint32_t id, TreeInfo *out)
{
    if (reg == NULL || out == NULL) {
        return TK_ERR_INVALID;
    }
    LOCK(reg);
    const TreeInfo *tree = find_tree(reg, id);
    if (tree != NULL) {
        *out = *tree;
    }
    UNLOCK(reg);
    return (tree != NULL) ? TK_OK : TK_ERR_NOT_FOUND;
}

size_t registry_count(TreeRegistry *reg)
{
    if (reg == NULL) {
        return 0;
    }
    LOCK(reg);
    size_t n = hashmap_size(reg->trees);
    UNLOCK(reg);
    return n;
}

static bool stats_visitor(uint32_t key, void *value, void *ctx)
{
    (void)key;
    const TreeInfo *tree = value;
    RegistryStats *stats = ctx;
    stats->by_status[tree->status]++;
    if (tree->assigned_volunteer >= 0) {
        stats->assigned++;
    }
    return true;
}

void registry_stats(TreeRegistry *reg, RegistryStats *out)
{
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof *out);
    if (reg == NULL) {
        return;
    }
    LOCK(reg);
    out->total = hashmap_size(reg->trees);
    out->queued = minheap_size(reg->queue);
    hashmap_foreach(reg->trees, stats_visitor, out);
    UNLOCK(reg);
}

TkStatus registry_most_urgent(TreeRegistry *reg, size_t k, TreeInfo *out, size_t *written)
{
    if (reg == NULL || written == NULL || (k > 0 && out == NULL)) {
        return TK_ERR_INVALID;
    }
    *written = 0;
    if (k == 0) {
        return TK_OK;
    }
    uint32_t *ids = tk_reallocarray(NULL, k, sizeof *ids);
    if (ids == NULL) {
        return TK_ERR_NOMEM;
    }

    LOCK(reg);
    size_t n = 0;
    TkStatus st = minheap_peek_k(reg->queue, k, ids, NULL, &n);
    for (size_t i = 0; st == TK_OK && i < n; i++) {
        out[i] = *find_tree(reg, ids[i]);
    }
    UNLOCK(reg);

    free(ids);
    if (st == TK_OK) {
        *written = n;
    }
    return st;
}

typedef struct {
    TreeInfo *items;
    size_t    count;
} SnapshotCtx;

static bool snapshot_visitor(uint32_t key, void *value, void *ctx)
{
    (void)key;
    SnapshotCtx *snap = ctx;
    snap->items[snap->count++] = *(const TreeInfo *)value;
    return true;
}

static int compare_by_id(const void *a, const void *b)
{
    uint32_t ia = ((const TreeInfo *)a)->id;
    uint32_t ib = ((const TreeInfo *)b)->id;
    return (ia > ib) - (ia < ib);
}

TkStatus registry_snapshot(TreeRegistry *reg, TreeInfo **out, size_t *count)
{
    if (reg == NULL || out == NULL || count == NULL) {
        return TK_ERR_INVALID;
    }
    *out = NULL;
    *count = 0;

    LOCK(reg);
    size_t n = hashmap_size(reg->trees);
    SnapshotCtx snap = { .items = NULL, .count = 0 };
    if (n > 0) {
        snap.items = tk_reallocarray(NULL, n, sizeof *snap.items);
        if (snap.items == NULL) {
            UNLOCK(reg);
            return TK_ERR_NOMEM;
        }
        hashmap_foreach(reg->trees, snapshot_visitor, &snap);
    }
    UNLOCK(reg);

    /* Sorting happens outside the lock: the copy is private to us. */
    if (snap.count > 1) {
        qsort(snap.items, snap.count, sizeof *snap.items, compare_by_id);
    }
    *out = snap.items;
    *count = snap.count;
    return TK_OK;
}

/* ---- Clock / id bookkeeping ---- */

double registry_clock(TreeRegistry *reg)
{
    if (reg == NULL) {
        return 0.0;
    }
    LOCK(reg);
    double t = reg->clock;
    UNLOCK(reg);
    return t;
}

void registry_set_clock(TreeRegistry *reg, double hours)
{
    if (reg == NULL || !isfinite(hours) || hours < 0.0) {
        return;
    }
    LOCK(reg);
    reg->clock = hours;
    UNLOCK(reg);
}

uint32_t registry_next_id(TreeRegistry *reg)
{
    if (reg == NULL) {
        return 0;
    }
    LOCK(reg);
    uint32_t id = reg->next_id;
    UNLOCK(reg);
    return id;
}

void registry_reserve_id(TreeRegistry *reg, uint32_t next_id)
{
    if (reg == NULL) {
        return;
    }
    LOCK(reg);
    if (next_id > reg->next_id) {
        reg->next_id = next_id;
    }
    UNLOCK(reg);
}
