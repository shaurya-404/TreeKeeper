#ifndef TREE_H
#define TREE_H

#define NAME_LEN 30
#define MAX_SPECIES 50

// a type of plant and how fast it loses water
typedef struct {
    char name[NAME_LEN];
    float dryRate;           // percent of water lost every hour
} Species;

// one planted sapling
typedef struct {
    int id;                  // tree ID (printed as QR code on the tree guard)
    char species[NAME_LEN];
    int zone;                // index of the zone in the graph
    float water;             // water level in percent (0 - 100)
    float dryRate;           // percent of water lost every hour
    int diseased;            // 1 if a volunteer reported disease
    int assignedTo;          // volunteer id, -1 if not assigned
    int heapIndex;           // position in the min heap, -1 if not in heap
} Tree;

// species functions
float getDryRate(const char *species);
int addSpecies(const char *name, float dryRate);
int getSpeciesCount(void);
Species *getSpecies(int i);
void printSpeciesList(void);

// tree functions
Tree *createTree(int id, const char *species, int zone, float water);
float hoursLeft(Tree *t);
const char *getStatus(Tree *t);

#endif
