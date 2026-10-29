#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "tree.h"

// species names and how fast each one loses water (% per hour)
// the first 8 are built in, more can be added from the menu
static Species speciesList[MAX_SPECIES] = {
    {"Neem", 1.2}, {"Banyan", 1.6}, {"Peepal", 1.5}, {"Mango", 2.2},
    {"Teak", 1.8}, {"Bamboo", 2.8}, {"Gulmohar", 2.0}, {"Arjuna", 1.7}
};
static int speciesCount = 8;

// returns the position of a species in the list, or -1 if not found
static int findSpecies(const char *species)
{
    for (int i = 0; i < speciesCount; i++) {
        if (strcasecmp(species, speciesList[i].name) == 0)
            return i;
    }
    return -1;
}

// returns the dry rate of a species, or -1 if the species is not known
float getDryRate(const char *species)
{
    int i = findSpecies(species);
    if (i == -1)
        return -1;
    return speciesList[i].dryRate;
}

// returns 1 if added, 0 if the species already exists, the list is full
// or the dry rate is not valid
int addSpecies(const char *name, float dryRate)
{
    if (getDryRate(name) >= 0 || speciesCount == MAX_SPECIES)
        return 0;
    if (dryRate <= 0 || dryRate > 100 || strlen(name) >= NAME_LEN)
        return 0;

    strcpy(speciesList[speciesCount].name, name);
    speciesList[speciesCount].dryRate = dryRate;
    speciesCount++;
    return 1;
}

int getSpeciesCount(void)
{
    return speciesCount;
}

Species *getSpecies(int i)
{
    if (i < 0 || i >= speciesCount)
        return NULL;
    return &speciesList[i];
}

void printSpeciesList(void)
{
    printf("Available species:\n");
    for (int i = 0; i < speciesCount; i++)
        printf("  %-10s (loses %.1f%% water per hour)\n", speciesList[i].name, speciesList[i].dryRate);
}

Tree *createTree(int id, const char *species, int zone, float water)
{
    int s = findSpecies(species);
    if (s == -1)
        return NULL;

    Tree *t = malloc(sizeof(Tree));
    if (t == NULL)
        return NULL;

    t->id = id;
    strcpy(t->species, speciesList[s].name);   // use the name as written in the list
    t->zone = zone;
    t->water = water;
    t->dryRate = speciesList[s].dryRate;
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
