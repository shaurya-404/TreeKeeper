#ifndef HEAP_H
#define HEAP_H

#include "tree.h"

// min heap of trees, the tree that will dry out first is at the root
typedef struct {
    Tree **arr;
    int size;
    int capacity;
} MinHeap;

MinHeap *createHeap(int capacity);
int heapInsert(MinHeap *heap, Tree *t);
Tree *heapPeek(MinHeap *heap);
Tree *extractMin(MinHeap *heap);
void heapRemove(MinHeap *heap, Tree *t);
void heapUpdate(MinHeap *heap, Tree *t);
void buildHeap(MinHeap *heap);
void freeHeap(MinHeap *heap);

#endif
