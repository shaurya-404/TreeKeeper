#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "volunteer.h"

VolunteerList *createVolunteerList(int capacity)
{
    VolunteerList *vl = malloc(sizeof(VolunteerList));
    if (vl == NULL)
        return NULL;
    vl->list = malloc(capacity * sizeof(Volunteer));
    if (vl->list == NULL) {
        free(vl);
        return NULL;
    }
    vl->count = 0;
    vl->capacity = capacity;
    return vl;
}

// returns 0 for a bad name, a duplicate id or no memory
int addVolunteer(VolunteerList *vl, int id, const char *name, int zone, int tasksDone)
{
    if (strlen(name) == 0 || strlen(name) >= NAME_LEN || findVolunteer(vl, id) != NULL)
        return 0;

    if (vl->count == vl->capacity) {
        Volunteer *bigger = realloc(vl->list, 2 * vl->capacity * sizeof(Volunteer));
        if (bigger == NULL)
            return 0;
        vl->list = bigger;
        vl->capacity *= 2;
    }

    Volunteer *v = &vl->list[vl->count];
    v->id = id;
    strcpy(v->name, name);
    v->zone = zone;
    v->tasksDone = tasksDone;
    vl->count++;
    return 1;
}

Volunteer *findVolunteer(VolunteerList *vl, int id)
{
    for (int i = 0; i < vl->count; i++) {
        if (vl->list[i].id == id)
            return &vl->list[i];
    }
    return NULL;
}

Volunteer *findVolunteerByName(VolunteerList *vl, const char *name)
{
    for (int i = 0; i < vl->count; i++) {
        if (strcasecmp(vl->list[i].name, name) == 0)
            return &vl->list[i];
    }
    return NULL;
}

// sorted by trees watered, selection sort on a copy so ids stay in order in the list
void printLeaderboard(VolunteerList *vl)
{
    if (vl->count == 0) {
        printf("  No volunteers yet.\n");
        return;
    }

    Volunteer copy[vl->count];
    for (int i = 0; i < vl->count; i++)
        copy[i] = vl->list[i];

    for (int i = 0; i < vl->count - 1; i++) {
        int max = i;
        for (int j = i + 1; j < vl->count; j++) {
            if (copy[j].tasksDone > copy[max].tasksDone)
                max = j;
        }
        Volunteer temp = copy[i];
        copy[i] = copy[max];
        copy[max] = temp;
    }

    for (int i = 0; i < vl->count; i++)
        printf("  %d. %-15s (id %d)  %d tree(s) watered\n", i + 1, copy[i].name, copy[i].id, copy[i].tasksDone);
}

void freeVolunteerList(VolunteerList *vl)
{
    if (vl == NULL)
        return;
    free(vl->list);
    free(vl);
}
