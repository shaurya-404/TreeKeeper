#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "graph.h"

Graph *createGraph(int capacity)
{
    if (capacity < 1)
        capacity = 1;

    Graph *g = malloc(sizeof(Graph));
    if (g == NULL)
        return NULL;

    g->zones = malloc(capacity * sizeof(Zone));
    if (g->zones == NULL) {
        free(g);
        return NULL;
    }
    g->count = 0;
    g->capacity = capacity;
    return g;
}

// returns the index of the new zone, or -1 on error
int addZone(Graph *g, const char *name)
{
    if (g == NULL || name == NULL || strlen(name) == 0 || strlen(name) >= NAME_LEN)
        return -1;
    if (findZone(g, name) != -1)
        return -1;   // zone already exists

    if (g->count == g->capacity) {
        Zone *bigger = realloc(g->zones, 2 * g->capacity * sizeof(Zone));
        if (bigger == NULL)
            return -1;
        g->zones = bigger;
        g->capacity *= 2;
    }

    strcpy(g->zones[g->count].name, name);
    g->zones[g->count].head = NULL;
    g->count++;
    return g->count - 1;
}

// returns index of the zone with this name, -1 if not found
int findZone(Graph *g, const char *name)
{
    for (int i = 0; i < g->count; i++) {
        if (strcasecmp(g->zones[i].name, name) == 0)
            return i;
    }
    return -1;
}

static Edge *findEdge(Graph *g, int from, int to)
{
    for (Edge *e = g->zones[from].head; e != NULL; e = e->next) {
        if (e->to == to)
            return e;
    }
    return NULL;
}

static int addEdge(Graph *g, int from, int to, int dist)
{
    Edge *e = malloc(sizeof(Edge));
    if (e == NULL)
        return 0;
    e->to = to;
    e->dist = dist;
    e->next = g->zones[from].head;
    g->zones[from].head = e;
    return 1;
}

// adds an undirected path, if it already exists the distance is updated
int addPath(Graph *g, int a, int b, int dist)
{
    if (g == NULL || a < 0 || b < 0 || a >= g->count || b >= g->count || a == b || dist <= 0)
        return 0;

    Edge *e = findEdge(g, a, b);
    if (e != NULL) {
        e->dist = dist;
        findEdge(g, b, a)->dist = dist;
        return 1;
    }

    if (!addEdge(g, a, b, dist))
        return 0;
    if (!addEdge(g, b, a, dist)) {
        removePath(g, a, b);
        return 0;
    }
    return 1;
}

static int deleteEdge(Graph *g, int from, int to)
{
    Edge *curr = g->zones[from].head;
    Edge *prev = NULL;
    while (curr != NULL) {
        if (curr->to == to) {
            if (prev == NULL)
                g->zones[from].head = curr->next;
            else
                prev->next = curr->next;
            free(curr);
            return 1;
        }
        prev = curr;
        curr = curr->next;
    }
    return 0;
}

int removePath(Graph *g, int a, int b)
{
    if (g == NULL || a < 0 || b < 0 || a >= g->count || b >= g->count)
        return 0;
    int found = deleteEdge(g, a, b);
    deleteEdge(g, b, a);
    return found;
}

void printGraph(Graph *g)
{
    printf("\nCampus map (adjacency list):\n");
    for (int i = 0; i < g->count; i++) {
        printf("  [%d] %-22s ->", i, g->zones[i].name);
        if (g->zones[i].head == NULL)
            printf(" (no paths)");
        for (Edge *e = g->zones[i].head; e != NULL; e = e->next)
            printf(" %s(%dm)", g->zones[e->to].name, e->dist);
        printf("\n");
    }
}

// Dijkstra's algorithm (array version, O(V^2))
// after it runs, dist[v] = shortest distance from src to v
// and parent[v] = previous zone on that shortest path
void dijkstra(Graph *g, int src, int dist[], int parent[])
{
    int *visited = calloc(g->count, sizeof(int));
    if (visited == NULL)
        return;

    for (int i = 0; i < g->count; i++) {
        dist[i] = INF;
        parent[i] = -1;
    }
    dist[src] = 0;

    for (int count = 0; count < g->count; count++) {
        // pick the unvisited zone with the smallest distance
        int u = -1;
        for (int i = 0; i < g->count; i++) {
            if (!visited[i] && (u == -1 || dist[i] < dist[u]))
                u = i;
        }
        if (u == -1 || dist[u] == INF)
            break;   // the remaining zones cannot be reached
        visited[u] = 1;

        // relax all paths going out of u
        for (Edge *e = g->zones[u].head; e != NULL; e = e->next) {
            if (!visited[e->to] && dist[u] + e->dist < dist[e->to]) {
                dist[e->to] = dist[u] + e->dist;
                parent[e->to] = u;
            }
        }
    }
    free(visited);
}

// prints the path from src to v by following the parent array (recursive)
static void printPath(Graph *g, int parent[], int v)
{
    if (parent[v] == -1) {
        printf("%s", g->zones[v].name);
        return;
    }
    printPath(g, parent, parent[v]);
    printf(" -> %s", g->zones[v].name);
}

// prints the shortest path, returns the distance or -1 if unreachable
int shortestPath(Graph *g, int src, int dest)
{
    if (g == NULL || src < 0 || dest < 0 || src >= g->count || dest >= g->count)
        return -1;

    int *dist = malloc(g->count * sizeof(int));
    int *parent = malloc(g->count * sizeof(int));
    if (dist == NULL || parent == NULL) {
        free(dist);
        free(parent);
        return -1;
    }

    dijkstra(g, src, dist, parent);
    int result = -1;
    if (dist[dest] != INF) {
        printf("  ");
        printPath(g, parent, dest);
        printf("\n  Total distance: %d m\n", dist[dest]);
        result = dist[dest];
    }
    free(dist);
    free(parent);
    return result;
}

// plans a walk from start that visits every zone in stops[]
// each time it goes to the nearest stop that is not visited yet (greedy)
// returns total distance, or -1 if some stop cannot be reached
int planRoute(Graph *g, int start, int stops[], int n)
{
    int *dist = malloc(g->count * sizeof(int));
    int *parent = malloc(g->count * sizeof(int));
    int *done = calloc(n > 0 ? n : 1, sizeof(int));
    if (dist == NULL || parent == NULL || done == NULL) {
        free(dist);
        free(parent);
        free(done);
        return -1;
    }

    int current = start;
    int total = 0;
    printf("  Start at %s\n", g->zones[start].name);

    for (int step = 0; step < n; step++) {
        dijkstra(g, current, dist, parent);

        int best = -1;
        for (int i = 0; i < n; i++) {
            if (!done[i] && (best == -1 || dist[stops[i]] < dist[stops[best]]))
                best = i;
        }
        if (dist[stops[best]] == INF) {
            printf("  %s cannot be reached!\n", g->zones[stops[best]].name);
            total = -1;
            break;
        }

        done[best] = 1;
        if (stops[best] != current) {
            printf("  Walk: ");
            printPath(g, parent, stops[best]);
            printf("  (%d m)\n", dist[stops[best]]);
            total += dist[stops[best]];
            current = stops[best];
        }
    }

    free(dist);
    free(parent);
    free(done);
    return total;
}

void freeGraph(Graph *g)
{
    if (g == NULL)
        return;
    for (int i = 0; i < g->count; i++) {
        Edge *e = g->zones[i].head;
        while (e != NULL) {
            Edge *next = e->next;
            free(e);
            e = next;
        }
    }
    free(g->zones);
    free(g);
}
