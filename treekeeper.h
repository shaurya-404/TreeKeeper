#ifndef TREEKEEPER_H
#define TREEKEEPER_H
#include "tree.h"

// Min-Heap Functions (Task Prioritization)
MinHeap* createMinHeap(int capacity);
void insertTree(MinHeap* heap, int id, const char* species, int timeToWater, int locNode);
Tree getNextTask(MinHeap* heap);
void minHeapify(MinHeap* heap, int idx);
void swapTrees(Tree* a, Tree* b);

// Graph Functions (Optimal Walking Path)
Graph* createGraph(int vertices);
void addPath(Graph* graph, int src, int dest, int weight);
void calculateOptimalRoute(Graph* graph, int startNode, int endNode);

#endif