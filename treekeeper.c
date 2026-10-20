#include "treekeeper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- Min-Heap Implementation ---
MinHeap* createMinHeap(int capacity) {
    MinHeap* heap = (MinHeap*)malloc(sizeof(MinHeap));
    heap->capacity = capacity;
    heap->size = 0;
    heap->array = (Tree*)malloc(capacity * sizeof(Tree));
    return heap;
}

void swapTrees(Tree* a, Tree* b) {
    Tree temp = *a;
    *a = *b;
    *b = temp;
}

void minHeapify(MinHeap* heap, int idx) {
    int smallest = idx;
    int left = 2 * idx + 1;
    int right = 2 * idx + 2;

    if (left < heap->size && heap->array[left].nextWateringTime < heap->array[smallest].nextWateringTime)
        smallest = left;
    if (right < heap->size && heap->array[right].nextWateringTime < heap->array[smallest].nextWateringTime)
        smallest = right;
    
    if (smallest != idx) {
        swapTrees(&heap->array[idx], &heap->array[smallest]);
        minHeapify(heap, smallest);
    }
}

// Log a new sapling into the Min-Heap
void insertTree(MinHeap* heap, int id, const char* species, int timeToWater, int locNode) {
    if (heap->size == heap->capacity) {
        printf("Heap overflow. Cannot log more trees.\n");
        return;
    }
    int i = heap->size++;
    heap->array[i].id = id;
    strncpy(heap->array[i].species, species, 49);
    heap->array[i].species[49] = '\0';
    heap->array[i].nextWateringTime = timeToWater;
    heap->array[i].locationNode = locNode;

    // Fix the min heap property
    while (i != 0 && heap->array[(i - 1) / 2].nextWateringTime > heap->array[i].nextWateringTime) {
        swapTrees(&heap->array[i], &heap->array[(i - 1) / 2]);
        i = (i - 1) / 2;
    }
    printf("[LOGGED] Sapling ID %d (%s) added.\n", id, species);
}

// Retrieve the most urgent watering task
Tree getNextTask(MinHeap* heap) {
    if (heap->size <= 0) {
        Tree emptyTree = {-1, "", -1, -1};
        return emptyTree;
    }
    if (heap->size == 1) {
        heap->size--;
        return heap->array[0];
    }
    Tree root = heap->array[0];
    heap->array[0] = heap->array[heap->size - 1];
    heap->size--;
    minHeapify(heap, 0);
    return root;
}

// --- Graph Implementation ---
Graph* createGraph(int vertices) {
    Graph* graph = (Graph*)malloc(sizeof(Graph));
    graph->numVertices = vertices;
    graph->array = (AdjList*)malloc(vertices * sizeof(AdjList));
    for (int i = 0; i < vertices; ++i)
        graph->array[i].head = NULL;
    return graph;
}

// Build the plantation map graph (undirected)
void addPath(Graph* graph, int src, int dest, int weight) {
    AdjListNode* newNode = (AdjListNode*)malloc(sizeof(AdjListNode));
    newNode->dest = dest;
    newNode->weight = weight;
    newNode->next = graph->array[src].head;
    graph->array[src].head = newNode;

    newNode = (AdjListNode*)malloc(sizeof(AdjListNode));
    newNode->dest = src;
    newNode->weight = weight;
    newNode->next = graph->array[dest].head;
    graph->array[dest].head = newNode;
}

// Placeholder for Dijkstra's/BFS Algorithm implementation
void calculateOptimalRoute(Graph* graph, int startNode, int endNode) {
    printf("[ROUTING] Calculating optimal walking path from Zone %d to Zone %d...\n", startNode, endNode);
    // Traversal logic goes here
}