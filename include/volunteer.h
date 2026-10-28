#ifndef VOLUNTEER_H
#define VOLUNTEER_H

#include "tree.h"

typedef struct {
    int id;
    char name[NAME_LEN];
    int zone;          // zone where the volunteer starts walking from
    int tasksDone;     // number of trees watered
} Volunteer;

// dynamic array of volunteers
typedef struct {
    Volunteer *list;
    int count;
    int capacity;
} VolunteerList;

VolunteerList *createVolunteerList(int capacity);
int addVolunteer(VolunteerList *vl, int id, const char *name, int zone, int tasksDone);
Volunteer *findVolunteer(VolunteerList *vl, int id);
void printLeaderboard(VolunteerList *vl);
void freeVolunteerList(VolunteerList *vl);

#endif
