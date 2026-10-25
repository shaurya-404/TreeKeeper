/**
 * @file graph.h
 * @brief Weighted, undirected campus map stored as an adjacency list.
 *
 * Vertices are plantation "zones" (Library, Hostel_Zone, ...), edges are
 * walkable paths weighted in metres. Zone ids are dense integers 0..n-1
 * assigned in creation order, so per-vertex arrays (dist[], prev[]) can be
 * indexed directly by Dijkstra's algorithm.
 *
 * Complexity (V zones, E paths):
 *   add_zone           O(V) (duplicate-name check)
 *   add/remove path    O(deg)
 *   shortest_path      O((V + E) log V)   Dijkstra with an indexed min-heap
 *   plan_route         O(k (V + E) log V) for k stops
 *
 * Thread safety: none. Only the main (UI) thread touches the graph.
 */
#ifndef GRAPH_H
#define GRAPH_H

#include <stdbool.h>
#include <stddef.h>

#include "tk_common.h"

typedef struct Graph Graph;
typedef struct Path Path;

/** Called once per undirected edge by graph_foreach_edge() (from < to). */
typedef void (*GraphEdgeVisitor)(int from, int to, double meters, void *ctx);

/** Creates an empty graph. Returns NULL on allocation failure. */
Graph *graph_create(void);

/** Frees the graph and all of its edges. Safe to call with NULL. */
void graph_destroy(Graph *graph);

/**
 * Adds a zone. Names must pass tk_validate_name() and be unique
 * (case-insensitive). The new id is written to @p out_id if non-NULL.
 */
TkStatus graph_add_zone(Graph *graph, const char *name, int *out_id);

/** Returns the id of the zone called @p name (case-insensitive) or -1. */
int graph_find_zone(const Graph *graph, const char *name);

/** Returns the zone's name, or NULL if @p id is invalid. */
const char *graph_zone_name(const Graph *graph, int id);

/** Returns true if @p id refers to an existing zone. */
bool graph_zone_valid(const Graph *graph, int id);

/** Number of zones (vertices). */
size_t graph_zone_count(const Graph *graph);

/** Number of undirected paths (edges). */
size_t graph_path_count(const Graph *graph);

/** Number of paths touching zone @p id (0 for invalid ids). */
size_t graph_degree(const Graph *graph, int id);

/**
 * Adds an undirected path of @p meters (> 0) between two distinct zones.
 * If the path already exists its length is updated instead.
 */
TkStatus graph_add_path(Graph *graph, int a, int b, double meters);

/** Removes the path between @p a and @p b. */
TkStatus graph_remove_path(Graph *graph, int a, int b);

/** Visits every undirected edge exactly once. */
void graph_foreach_edge(const Graph *graph, GraphEdgeVisitor visit, void *ctx);

/** Visits the neighbours of zone @p id (each edge reported as id -> to). */
void graph_foreach_neighbor(const Graph *graph, int id, GraphEdgeVisitor visit, void *ctx);

/**
 * Dijkstra's algorithm: computes the shortest walk from @p src to @p dst.
 * On success *out receives a new Path (free with path_destroy()).
 * Returns TK_ERR_UNREACHABLE if the zones are in disconnected components.
 */
TkStatus graph_shortest_path(const Graph *graph, int src, int dst, Path **out);

/**
 * Plans a walk that starts at @p start and visits every zone in
 * @p stops[0..count-1]. Visiting order is chosen greedily (always walk to
 * the nearest unvisited stop, measured by true shortest-path distance),
 * a standard heuristic for this NP-hard travelling-salesman variant.
 * Stops that are visited are flagged (see path_is_stop()).
 */
TkStatus graph_plan_route(const Graph *graph, int start,
                          const int *stops, size_t count, Path **out);

/* ---- Path accessors ---- */

/** Number of zones on the path (including start and end). */
size_t path_length(const Path *path);

/** Zone id at position @p index, or -1 if out of bounds. */
int path_zone_at(const Path *path, size_t index);

/** True if position @p index is one of the requested stops. */
bool path_is_stop(const Path *path, size_t index);

/** Total walking distance in metres. */
double path_distance(const Path *path);

/** Frees a path. Safe to call with NULL. */
void path_destroy(Path *path);

#endif /* GRAPH_H */
