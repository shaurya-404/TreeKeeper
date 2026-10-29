#include <stdio.h>
#include <stdlib.h>
#include "hashmap.h"

static int hashFunction(int key, int size)
{
    if (key < 0)
        key = -key;
    return key % size;
}

HashMap *createHashMap(int size)
{
    HashMap *map = malloc(sizeof(HashMap));
    if (map == NULL)
        return NULL;

    map->buckets = calloc(size, sizeof(Node *));
    if (map->buckets == NULL) {
        free(map);
        return NULL;
    }
    map->size = size;
    map->count = 0;
    return map;
}

// doubles the number of buckets and moves every node to its new bucket
static int rehash(HashMap *map)
{
    int newSize = map->size * 2;
    Node **newBuckets = calloc(newSize, sizeof(Node *));
    if (newBuckets == NULL)
        return 0;

    for (int i = 0; i < map->size; i++) {
        Node *curr = map->buckets[i];
        while (curr != NULL) {
            Node *next = curr->next;
            int index = hashFunction(curr->key, newSize);
            curr->next = newBuckets[index];
            newBuckets[index] = curr;
            curr = next;
        }
    }
    free(map->buckets);
    map->buckets = newBuckets;
    map->size = newSize;
    return 1;
}

// returns 1 on success, 0 if id already exists or malloc fails
int hashInsert(HashMap *map, Tree *t)
{
    if (map == NULL || t == NULL)
        return 0;
    if (hashSearch(map, t->id) != NULL)
        return 0;

    // keep the load factor below 0.75 so chains stay short
    if (map->count + 1 > map->size * 3 / 4) {
        if (!rehash(map))
            return 0;
    }

    Node *node = malloc(sizeof(Node));
    if (node == NULL)
        return 0;

    int index = hashFunction(t->id, map->size);
    node->key = t->id;
    node->tree = t;
    node->next = map->buckets[index];   // insert at head of the chain
    map->buckets[index] = node;
    map->count++;
    return 1;
}

// O(1) on average
Tree *hashSearch(HashMap *map, int key)
{
    if (map == NULL)
        return NULL;

    Node *curr = map->buckets[hashFunction(key, map->size)];
    while (curr != NULL) {
        if (curr->key == key)
            return curr->tree;
        curr = curr->next;
    }
    return NULL;
}

// removes the node (does not free the tree), returns 1 if found
int hashDelete(HashMap *map, int key)
{
    if (map == NULL)
        return 0;

    int index = hashFunction(key, map->size);
    Node *curr = map->buckets[index];
    Node *prev = NULL;

    while (curr != NULL) {
        if (curr->key == key) {
            if (prev == NULL)
                map->buckets[index] = curr->next;
            else
                prev->next = curr->next;
            free(curr);
            map->count--;
            return 1;
        }
        prev = curr;
        curr = curr->next;
    }
    return 0;
}

// returns a malloc'd array with every tree (caller must free the array)
Tree **getAllTrees(HashMap *map, int *n)
{
    *n = 0;
    if (map == NULL || map->count == 0)
        return NULL;

    Tree **list = malloc(map->count * sizeof(Tree *));
    if (list == NULL)
        return NULL;

    for (int i = 0; i < map->size; i++) {
        for (Node *curr = map->buckets[i]; curr != NULL; curr = curr->next)
            list[(*n)++] = curr->tree;
    }
    return list;
}

// frees all nodes and also the trees stored in them
void freeHashMap(HashMap *map)
{
    if (map == NULL)
        return;

    for (int i = 0; i < map->size; i++) {
        Node *curr = map->buckets[i];
        while (curr != NULL) {
            Node *next = curr->next;
            free(curr->tree);
            free(curr);
            curr = next;
        }
    }
    free(map->buckets);
    free(map);
}
