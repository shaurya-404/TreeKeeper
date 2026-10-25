/**
 * @file storage.h
 * @brief CSV persistence for the map, tree registry and volunteer roster.
 *
 * Layout inside the data directory (human-readable, opens in Excel):
 *
 *   meta.csv        key,value                (format version, clock, ids, speed)
 *   zones.csv       id,name
 *   paths.csv       from,to,meters           (each undirected path once)
 *   volunteers.csv  id,name,zone,tasks_completed
 *   trees.csv       id,species,zone,water,decay_rate,planted_at,
 *                   last_watered,diseased,assigned_volunteer
 *
 * Saving is crash-safe per file: data is written to "<file>.tmp" and then
 * atomically rename()d over the old file, so a crash mid-save never
 * leaves a half-written CSV behind.
 *
 * Loading is defensive: malformed rows, unknown zone references and
 * duplicate ids are skipped and counted instead of aborting the load.
 */
#ifndef STORAGE_H
#define STORAGE_H

#include <stdbool.h>
#include <stddef.h>

#include "graph.h"
#include "registry.h"
#include "tk_common.h"
#include "volunteer.h"

#define STORAGE_FORMAT_VERSION 1

/** Counts reported back after a load. */
typedef struct {
    size_t zones;
    size_t paths;
    size_t volunteers;
    size_t trees;
    size_t skipped_rows; /* malformed or inconsistent rows ignored */
    double speed;        /* saved simulator speed (0 if absent)    */
} StorageReport;

/** True if @p dir contains a saved TreeKeeper state. */
bool storage_exists(const char *dir);

/**
 * Writes the full state into @p dir (created if missing).
 * @p speed is the simulator speed to remember for next start-up.
 */
TkStatus storage_save(const char *dir, const Graph *graph, TreeRegistry *reg,
                      const Roster *roster, double speed);

/**
 * Loads state from @p dir into the given EMPTY containers.
 * @p report may be NULL.
 */
TkStatus storage_load(const char *dir, Graph *graph, TreeRegistry *reg,
                      Roster *roster, StorageReport *report);

#endif /* STORAGE_H */
