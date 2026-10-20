#ifndef TREE_H
#define TREE_H

// Tree structure for the Min-Heap
typedef struct {
    int id;
    char species[50];
    int nextWateringTime; // Simulated timestamp for priority
    int locationNode;     // Graph node ID for routing
} Tree;

// Structure for Graph Adjacency List Node
typedef struct AdjListNode {
    int dest;
    int weight;
    struct AdjListNode* next;
} AdjListNode;

// Structure for Adjacency List
typedef struct {
    AdjListNode* head;
} AdjList;

// Structure for the Graph
typedef struct {
    int numVertices;
    AdjList* array;
} Graph;

// Structure for the Min-Heap
typedef struct {
    Tree* array;
    int capacity;
    int size;
} MinHeap;

#endif