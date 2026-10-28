#ifndef GRAPH_H
#define GRAPH_H

#include "tree.h"

#define INF 1000000000

// one walking path from a zone to a neighbouring zone
typedef struct Edge {
    int to;
    int dist;            // distance in metres
    struct Edge *next;
} Edge;

// a zone of the campus (vertex of the graph)
typedef struct {
    char name[NAME_LEN];
    Edge *head;          // adjacency list
} Zone;

typedef struct {
    Zone *zones;
    int count;
    int capacity;
} Graph;

Graph *createGraph(int capacity);
int addZone(Graph *g, const char *name);
int findZone(Graph *g, const char *name);
int addPath(Graph *g, int a, int b, int dist);
int removePath(Graph *g, int a, int b);
void printGraph(Graph *g);

void dijkstra(Graph *g, int src, int dist[], int parent[]);
int shortestPath(Graph *g, int src, int dest);
int planRoute(Graph *g, int start, int stops[], int n);

void freeGraph(Graph *g);

#endif
