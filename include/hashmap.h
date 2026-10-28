#ifndef HASHMAP_H
#define HASHMAP_H

#include "tree.h"

// hash map with separate chaining, key = tree id, value = pointer to tree
typedef struct Node {
    int key;
    Tree *tree;
    struct Node *next;
} Node;

typedef struct {
    Node **buckets;
    int size;     // number of buckets
    int count;    // number of trees stored
} HashMap;

HashMap *createHashMap(int size);
int hashInsert(HashMap *map, Tree *t);
Tree *hashSearch(HashMap *map, int key);
int hashDelete(HashMap *map, int key);
Tree **getAllTrees(HashMap *map, int *n);
void freeHashMap(HashMap *map);

#endif
