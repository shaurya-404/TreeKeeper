#ifndef VOLUNTEER_H
#define VOLUNTEER_H

#include "tree.h"

typedef struct {
    int id;
    char name[NAME_LEN];
    int zone;          // where the volunteer starts walking from
    int tasksDone;     // trees watered
} Volunteer;

// dynamic array
typedef struct {
    Volunteer *list;
    int count;
    int capacity;
} VolunteerList;

VolunteerList *createVolunteerList(int capacity);
int addVolunteer(VolunteerList *vl, int id, const char *name, int zone, int tasksDone);
Volunteer *findVolunteer(VolunteerList *vl, int id);
Volunteer *findVolunteerByName(VolunteerList *vl, const char *name);
void printLeaderboard(VolunteerList *vl);
void freeVolunteerList(VolunteerList *vl);

#endif
