#include <stdio.h>
#include <stdlib.h>
#include "heap.h"

// for node i: parent = (i-1)/2, left = 2i+1, right = 2i+2

MinHeap *createHeap(int capacity)
{
    if (capacity < 1)
        capacity = 1;

    MinHeap *heap = malloc(sizeof(MinHeap));
    if (heap == NULL)
        return NULL;

    heap->arr = malloc(capacity * sizeof(Tree *));
    if (heap->arr == NULL) {
        free(heap);
        return NULL;
    }
    heap->size = 0;
    heap->capacity = capacity;
    return heap;
}

// swaps two trees and updates their stored positions
static void swap(MinHeap *heap, int i, int j)
{
    Tree *temp = heap->arr[i];
    heap->arr[i] = heap->arr[j];
    heap->arr[j] = temp;
    heap->arr[i]->heapIndex = i;
    heap->arr[j]->heapIndex = j;
}

static void heapifyUp(MinHeap *heap, int i)
{
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (hoursLeft(heap->arr[i]) >= hoursLeft(heap->arr[parent]))
            break;
        swap(heap, i, parent);
        i = parent;
    }
}

static void heapifyDown(MinHeap *heap, int i)
{
    while (1) {
        int smallest = i;
        int left = 2 * i + 1;
        int right = 2 * i + 2;

        if (left < heap->size && hoursLeft(heap->arr[left]) < hoursLeft(heap->arr[smallest]))
            smallest = left;
        if (right < heap->size && hoursLeft(heap->arr[right]) < hoursLeft(heap->arr[smallest]))
            smallest = right;

        if (smallest == i)
            break;
        swap(heap, i, smallest);
        i = smallest;
    }
}

// returns 1 on success, 0 if memory could not be allocated
int heapInsert(MinHeap *heap, Tree *t)
{
    if (heap == NULL || t == NULL)
        return 0;

    // array is full, so double its size
    if (heap->size == heap->capacity) {
        Tree **bigger = realloc(heap->arr, 2 * heap->capacity * sizeof(Tree *));
        if (bigger == NULL)
            return 0;
        heap->arr = bigger;
        heap->capacity *= 2;
    }

    heap->arr[heap->size] = t;
    t->heapIndex = heap->size;
    heap->size++;
    heapifyUp(heap, heap->size - 1);
    return 1;
}

Tree *heapPeek(MinHeap *heap)
{
    if (heap == NULL || heap->size == 0)
        return NULL;
    return heap->arr[0];
}

// removes and returns the most urgent tree
Tree *extractMin(MinHeap *heap)
{
    if (heap == NULL || heap->size == 0)
        return NULL;

    Tree *min = heap->arr[0];
    heapRemove(heap, min);
    return min;
}

// removes any tree from the heap using its stored index, O(log n)
void heapRemove(MinHeap *heap, Tree *t)
{
    if (heap == NULL || t == NULL || t->heapIndex < 0)
        return;

    int i = t->heapIndex;
    int last = heap->size - 1;

    swap(heap, i, last);   // move it to the end
    heap->size--;
    t->heapIndex = -1;

    // the tree that was moved into position i may need to go up or down
    if (i < heap->size) {
        Tree *moved = heap->arr[i];
        heapifyUp(heap, i);
        heapifyDown(heap, moved->heapIndex);
    }
}

// call this after a tree's water level changes
void heapUpdate(MinHeap *heap, Tree *t)
{
    if (heap == NULL || t == NULL || t->heapIndex < 0)
        return;
    heapifyUp(heap, t->heapIndex);
    heapifyDown(heap, t->heapIndex);
}

// rebuilds the whole heap in O(n), used after the timer changes all water levels
void buildHeap(MinHeap *heap)
{
    for (int i = heap->size / 2 - 1; i >= 0; i--)
        heapifyDown(heap, i);
}

// only frees the heap, the trees are freed by the hash map
void freeHeap(MinHeap *heap)
{
    if (heap == NULL)
        return;
    free(heap->arr);
    free(heap);
}
