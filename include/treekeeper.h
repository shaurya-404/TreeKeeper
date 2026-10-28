#ifndef TREEKEEPER_H
#define TREEKEEPER_H

#include <pthread.h>
#include "tree.h"
#include "hashmap.h"
#include "heap.h"
#include "graph.h"
#include "volunteer.h"

// everything the program needs, passed around as one struct
typedef struct {
    HashMap *trees;          // all trees, searched by id
    MinHeap *heap;           // trees waiting to be watered (not assigned)
    Graph *map;              // campus zones and paths
    VolunteerList *vols;

    int nextTreeId;
    int nextVolunteerId;
    float simHours;          // simulated time since start
    float speed;             // simulated hours per real second

    int paused;
    int running;             // timer thread runs while this is 1
    pthread_t timerThread;
    pthread_mutex_t lock;    // protects trees and heap (timer thread + menu)
} System;

// simulation.c
void startTimer(System *sys);
void stopTimer(System *sys);
void simulateHours(System *sys, float hours);

// fileio.c
int saveData(System *sys, const char *folder);
int loadData(System *sys, const char *folder);
void loadSampleData(System *sys);

#endif
