#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "tree.h"

#define NUM_SPECIES 8

// species names and how fast each one loses water (% per hour)
static const char *speciesNames[NUM_SPECIES] = {
    "Neem", "Banyan", "Peepal", "Mango", "Teak", "Bamboo", "Gulmohar", "Arjuna"
};
static const float speciesDryRate[NUM_SPECIES] = {
    1.2, 1.6, 1.5, 2.2, 1.8, 2.8, 2.0, 1.7
};

// returns the dry rate of a species, or -1 if the species is not known
float getDryRate(const char *species)
{
    for (int i = 0; i < NUM_SPECIES; i++) {
        if (strcasecmp(species, speciesNames[i]) == 0)
            return speciesDryRate[i];
    }
    return -1;
}

void printSpeciesList(void)
{
    printf("Available species:\n");
    for (int i = 0; i < NUM_SPECIES; i++)
        printf("  %-10s (loses %.1f%% water per hour)\n", speciesNames[i], speciesDryRate[i]);
}

Tree *createTree(int id, const char *species, int zone, float water)
{
    float rate = getDryRate(species);
    if (rate < 0)
        return NULL;

    Tree *t = malloc(sizeof(Tree));
    if (t == NULL)
        return NULL;

    t->id = id;
    strncpy(t->species, species, NAME_LEN - 1);
    t->species[NAME_LEN - 1] = '\0';
    t->zone = zone;
    t->water = water;
    t->dryRate = rate;
    t->diseased = 0;
    t->assignedTo = -1;
    t->heapIndex = -1;
    return t;
}

// hours left before the tree runs out of water
// this is the priority used in the min heap (smaller = more urgent)
float hoursLeft(Tree *t)
{
    return t->water / t->dryRate;
}

const char *getStatus(Tree *t)
{
    if (t->diseased)
        return "DISEASED";
    if (t->water <= 0)
        return "WILTED";
    if (t->water < 30)
        return "CRITICAL";
    if (t->water < 60)
        return "THIRSTY";
    return "HEALTHY";
}
