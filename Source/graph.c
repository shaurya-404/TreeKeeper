/**
 * @file graph.c
 * @brief Adjacency-list graph, Dijkstra's algorithm and route planning.
 */
#include "graph.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "minheap.h"

#define GRAPH_MIN_CAPACITY 8u
#define PATH_MIN_CAPACITY  8u

/* One directed half of an undirected path, kept in a singly linked list. */
typedef struct Edge {
    int          to;
    double       meters;
    struct Edge *next;
} Edge;

typedef struct {
    char  name[TK_NAME_MAX];
    Edge *head;   /* adjacency list */
    size_t degree;
} Zone;

struct Graph {
    Zone  *zones;      /* dynamic array indexed by zone id */
    size_t count;
    size_t capacity;
    size_t path_count; /* undirected edges */
};

struct Path {
    int           *zones;
    unsigned char *stop;  /* stop[i] != 0 if zones[i] was a requested stop */
    size_t         len;
    size_t         capacity;
    double         meters;
};

/* ======================================================================
 *  Graph construction
 * ==================================================================== */

Graph *graph_create(void)
{
    Graph *graph = malloc(sizeof *graph);
    if (graph == NULL) {
        return NULL;
    }
    graph->zones = tk_reallocarray(NULL, GRAPH_MIN_CAPACITY, sizeof *graph->zones);
    if (graph->zones == NULL) {
        free(graph);
        return NULL;
    }
    graph->count = 0;
    graph->capacity = GRAPH_MIN_CAPACITY;
    graph->path_count = 0;
    return graph;
}

static void free_edge_list(Edge *edge)
{
    while (edge != NULL) {
        Edge *next = edge->next;
        free(edge);
        edge = next;
    }
}

void graph_destroy(Graph *graph)
{
    if (graph == NULL) {
        return;
    }
    for (size_t i = 0; i < graph->count; i++) {
        free_edge_list(graph->zones[i].head);
    }
    free(graph->zones);
    free(graph);
}

bool graph_zone_valid(const Graph *graph, int id)
{
    return graph != NULL && id >= 0 && (size_t)id < graph->count;
}

int graph_find_zone(const Graph *graph, const char *name)
{
    if (graph == NULL || name == NULL) {
        return -1;
    }
    for (size_t i = 0; i < graph->count; i++) {
        if (strcasecmp(graph->zones[i].name, name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

TkStatus graph_add_zone(Graph *graph, const char *name, int *out_id)
{
    if (graph == NULL || tk_validate_name(name) != TK_OK) {
        return TK_ERR_INVALID;
    }
    if (graph_find_zone(graph, name) >= 0) {
        return TK_ERR_EXISTS;
    }
    if (graph->count >= (size_t)INT32_MAX) {
        return TK_ERR_NOMEM; /* ids must fit in an int */
    }

    if (graph->count == graph->capacity) {
        size_t new_capacity = graph->capacity * 2;
        Zone *grown = tk_reallocarray(graph->zones, new_capacity, sizeof *grown);
        if (grown == NULL) {
            return TK_ERR_NOMEM;
        }
        graph->zones = grown;
        graph->capacity = new_capacity;
    }

    Zone *zone = &graph->zones[graph->count];
    memset(zone, 0, sizeof *zone);
    /* Length already validated (< TK_NAME_MAX), so this always fits. */
    memcpy(zone->name, name, strlen(name) + 1);

    if (out_id != NULL) {
        *out_id = (int)graph->count;
    }
    graph->count++;
    return TK_OK;
}

const char *graph_zone_name(const Graph *graph, int id)
{
    return graph_zone_valid(graph, id) ? graph->zones[id].name : NULL;
}

size_t graph_zone_count(const Graph *graph)
{
    return (graph == NULL) ? 0 : graph->count;
}

size_t graph_path_count(const Graph *graph)
{
    return (graph == NULL) ? 0 : graph->path_count;
}

size_t graph_degree(const Graph *graph, int id)
{
    return graph_zone_valid(graph, id) ? graph->zones[id].degree : 0;
}

static Edge *find_edge(const Graph *graph, int from, int to)
{
    for (Edge *e = graph->zones[from].head; e != NULL; e = e->next) {
        if (e->to == to) {
            return e;
        }
    }
    return NULL;
}

TkStatus graph_add_path(Graph *graph, int a, int b, double meters)
{
    if (!graph_zone_valid(graph, a) || !graph_zone_valid(graph, b) || a == b) {
        return TK_ERR_INVALID;
    }
    /* Dijkstra requires strictly positive, finite weights. */
    if (!isfinite(meters) || meters <= 0.0) {
        return TK_ERR_INVALID;
    }

    Edge *ab = find_edge(graph, a, b);
    if (ab != NULL) {
        /* Both halves always exist together, so just update them. */
        ab->meters = meters;
        find_edge(graph, b, a)->meters = meters;
        return TK_OK;
    }

    /* Allocate both halves before linking either: all-or-nothing. */
    Edge *forward = malloc(sizeof *forward);
    Edge *backward = malloc(sizeof *backward);
    if (forward == NULL || backward == NULL) {
        free(forward);
        free(backward);
        return TK_ERR_NOMEM;
    }

    *forward = (Edge){ .to = b, .meters = meters, .next = graph->zones[a].head };
    *backward = (Edge){ .to = a, .meters = meters, .next = graph->zones[b].head };
    graph->zones[a].head = forward;
    graph->zones[b].head = backward;
    graph->zones[a].degree++;
    graph->zones[b].degree++;
    graph->path_count++;
    return TK_OK;
}

/* Unlinks and frees the edge from -> to. Returns true if one was found. */
static bool unlink_edge(Graph *graph, int from, int to)
{
    Edge **link = &graph->zones[from].head;
    while (*link != NULL) {
        if ((*link)->to == to) {
            Edge *dead = *link;
            *link = dead->next;
            free(dead);
            graph->zones[from].degree--;
            return true;
        }
        link = &(*link)->next;
    }
    return false;
}

TkStatus graph_remove_path(Graph *graph, int a, int b)
{
    if (!graph_zone_valid(graph, a) || !graph_zone_valid(graph, b)) {
        return TK_ERR_INVALID;
    }
    if (!unlink_edge(graph, a, b)) {
        return TK_ERR_NOT_FOUND;
    }
    (void)unlink_edge(graph, b, a);
    graph->path_count--;
    return TK_OK;
}

void graph_foreach_edge(const Graph *graph, GraphEdgeVisitor visit, void *ctx)
{
    if (graph == NULL || visit == NULL) {
        return;
    }
    for (size_t i = 0; i < graph->count; i++) {
        for (const Edge *e = graph->zones[i].head; e != NULL; e = e->next) {
            if ((int)i < e->to) { /* report each undirected edge once */
                visit((int)i, e->to, e->meters, ctx);
            }
        }
    }
}

void graph_foreach_neighbor(const Graph *graph, int id, GraphEdgeVisitor visit, void *ctx)
{
    if (!graph_zone_valid(graph, id) || visit == NULL) {
        return;
    }
    for (const Edge *e = graph->zones[id].head; e != NULL; e = e->next) {
        visit(id, e->to, e->meters, ctx);
    }
}

/* ======================================================================
 *  Path (growable list of zone ids)
 * ==================================================================== */

static Path *path_create(void)
{
    Path *path = calloc(1, sizeof *path);
    if (path == NULL) {
        return NULL;
    }
    path->zones = tk_reallocarray(NULL, PATH_MIN_CAPACITY, sizeof *path->zones);
    path->stop = calloc(PATH_MIN_CAPACITY, sizeof *path->stop);
    if (path->zones == NULL || path->stop == NULL) {
        path_destroy(path);
        return NULL;
    }
    path->capacity = PATH_MIN_CAPACITY;
    return path;
}

void path_destroy(Path *path)
{
    if (path == NULL) {
        return;
    }
    free(path->zones);
    free(path->stop);
    free(path);
}

static TkStatus path_append(Path *path, int zone, bool is_stop)
{
    if (path->len == path->capacity) {
        if (path->capacity > SIZE_MAX / 2) {
            return TK_ERR_NOMEM;
        }
        size_t new_capacity = path->capacity * 2;
        int *zones = tk_reallocarray(path->zones, new_capacity, sizeof *zones);
        if (zones == NULL) {
            return TK_ERR_NOMEM;
        }
        path->zones = zones;
        unsigned char *stop = tk_reallocarray(path->stop, new_capacity, sizeof *stop);
        if (stop == NULL) {
            return TK_ERR_NOMEM;
        }
        path->stop = stop;
        path->capacity = new_capacity;
    }
    path->zones[path->len] = zone;
    path->stop[path->len] = is_stop ? 1 : 0;
    path->len++;
    return TK_OK;
}

size_t path_length(const Path *path)
{
    return (path == NULL) ? 0 : path->len;
}

int path_zone_at(const Path *path, size_t index)
{
    return (path == NULL || index >= path->len) ? -1 : path->zones[index];
}

bool path_is_stop(const Path *path, size_t index)
{
    return path != NULL && index < path->len && path->stop[index] != 0;
}

double path_distance(const Path *path)
{
    return (path == NULL) ? 0.0 : path->meters;
}

/* ======================================================================
 *  Dijkstra's algorithm
 * ==================================================================== */

/*
 * Single-source shortest paths from @p src.
 * On return dist[v] holds the distance (INFINITY if unreachable) and
 * prev[v] the predecessor of v on a shortest path (-1 for src/unreached).
 *
 * The frontier is the project's own indexed MinHeap, which supports
 * decrease-key. Every vertex is therefore pushed at most once and popped
 * exactly when its distance becomes final (all weights are > 0).
 */
static TkStatus dijkstra(const Graph *graph, int src, double *dist, int *prev)
{
    for (size_t v = 0; v < graph->count; v++) {
        dist[v] = INFINITY;
        prev[v] = -1;
    }

    MinHeap *frontier = minheap_create(graph->count);
    if (frontier == NULL) {
        return TK_ERR_NOMEM;
    }

    dist[src] = 0.0;
    TkStatus st = minheap_push(frontier, (uint32_t)src, 0.0);

    uint32_t u_id = 0;
    double du = 0.0;
    while (st == TK_OK && minheap_pop(frontier, &u_id, &du) == TK_OK) {
        int u = (int)u_id;
        /* Relax every edge leaving the settled vertex u. */
        for (const Edge *e = graph->zones[u].head; e != NULL; e = e->next) {
            double candidate = du + e->meters;
            if (candidate < dist[e->to]) {
                dist[e->to] = candidate;
                prev[e->to] = u;
                if (minheap_contains(frontier, (uint32_t)e->to)) {
                    st = minheap_update(frontier, (uint32_t)e->to, candidate);
                } else {
                    st = minheap_push(frontier, (uint32_t)e->to, candidate);
                }
                if (st != TK_OK) {
                    break;
                }
            }
        }
    }

    minheap_destroy(frontier);
    return st;
}

/* Scratch arrays shared by the path functions. */
typedef struct {
    double *dist;
    int    *prev;
    int    *stack;
} DijkstraScratch;

static TkStatus scratch_init(DijkstraScratch *s, size_t n)
{
    s->dist = tk_reallocarray(NULL, n, sizeof *s->dist);
    s->prev = tk_reallocarray(NULL, n, sizeof *s->prev);
    s->stack = tk_reallocarray(NULL, n, sizeof *s->stack);
    if (s->dist == NULL || s->prev == NULL || s->stack == NULL) {
        free(s->dist);
        free(s->prev);
        free(s->stack);
        return TK_ERR_NOMEM;
    }
    return TK_OK;
}

static void scratch_free(DijkstraScratch *s)
{
    free(s->dist);
    free(s->prev);
    free(s->stack);
}

/*
 * Walks prev[] back from @p dst to the source and appends the zones in
 * forward order. If @p skip_first is true the source is not appended
 * (it is already the last zone of the path being extended).
 */
static TkStatus append_segment(Path *path, const DijkstraScratch *s,
                               int dst, bool skip_first, bool dst_is_stop)
{
    size_t depth = 0;
    for (int v = dst; v != -1; v = s->prev[v]) {
        s->stack[depth++] = v;
    }
    /* stack[depth-1] is the source, stack[0] is dst. */
    size_t begin = skip_first ? depth - 1 : depth;
    for (size_t i = begin; i-- > 0;) {
        bool stop_here = dst_is_stop && i == 0;
        TkStatus st = path_append(path, s->stack[i], stop_here);
        if (st != TK_OK) {
            return st;
        }
    }
    return TK_OK;
}

TkStatus graph_shortest_path(const Graph *graph, int src, int dst, Path **out)
{
    if (out == NULL || !graph_zone_valid(graph, src) || !graph_zone_valid(graph, dst)) {
        return TK_ERR_INVALID;
    }
    *out = NULL;

    DijkstraScratch s;
    TkStatus st = scratch_init(&s, graph->count);
    if (st != TK_OK) {
        return st;
    }

    st = dijkstra(graph, src, s.dist, s.prev);
    if (st == TK_OK && isinf(s.dist[dst])) {
        st = TK_ERR_UNREACHABLE;
    }

    Path *path = NULL;
    if (st == TK_OK) {
        path = path_create();
        st = (path == NULL) ? TK_ERR_NOMEM : append_segment(path, &s, dst, false, true);
    }
    if (st == TK_OK) {
        path->meters = s.dist[dst];
        *out = path;
    } else {
        path_destroy(path);
    }
    scratch_free(&s);
    return st;
}

TkStatus graph_plan_route(const Graph *graph, int start,
                          const int *stops, size_t count, Path **out)
{
    if (out == NULL || !graph_zone_valid(graph, start) || (count > 0 && stops == NULL)) {
        return TK_ERR_INVALID;
    }
    *out = NULL;
    for (size_t i = 0; i < count; i++) {
        if (!graph_zone_valid(graph, stops[i])) {
            return TK_ERR_INVALID;
        }
    }

    DijkstraScratch s;
    TkStatus st = scratch_init(&s, graph->count);
    if (st != TK_OK) {
        return st;
    }
    bool *visited = calloc(count > 0 ? count : 1, sizeof *visited);
    Path *path = path_create();
    if (visited == NULL || path == NULL) {
        st = TK_ERR_NOMEM;
    } else {
        st = path_append(path, start, false);
    }

    int current = start;
    size_t remaining = count;
    while (st == TK_OK && remaining > 0) {
        st = dijkstra(graph, current, s.dist, s.prev);
        if (st != TK_OK) {
            break;
        }

        /* Greedy choice: nearest unvisited stop from where we stand. */
        size_t best = SIZE_MAX;
        for (size_t i = 0; i < count; i++) {
            if (!visited[i] && (best == SIZE_MAX || s.dist[stops[i]] < s.dist[stops[best]])) {
                best = i;
            }
        }
        if (isinf(s.dist[stops[best]])) {
            st = TK_ERR_UNREACHABLE;
            break;
        }

        int target = stops[best];
        if (target == current) {
            /* Already standing there: just flag the current position. */
            path->stop[path->len - 1] = 1;
        } else {
            st = append_segment(path, &s, target, true, true);
            path->meters += s.dist[target];
        }

        /* Several stops may share one zone; tick them all off together. */
        for (size_t i = 0; i < count; i++) {
            if (!visited[i] && stops[i] == target) {
                visited[i] = true;
                remaining--;
            }
        }
        current = target;
    }

    if (st == TK_OK) {
        *out = path;
    } else {
        path_destroy(path);
    }
    free(visited);
    scratch_free(&s);
    return st;
}
