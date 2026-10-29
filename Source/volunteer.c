#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "volunteer.h"

VolunteerList *createVolunteerList(int capacity)
{
    if (capacity < 1)
        capacity = 1;

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

// returns 1 on success, 0 on error (bad name, duplicate id, no memory)
int addVolunteer(VolunteerList *vl, int id, const char *name, int zone, int tasksDone)
{
    if (vl == NULL || name == NULL || strlen(name) == 0 || strlen(name) >= NAME_LEN)
        return 0;
    if (findVolunteer(vl, id) != NULL)
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

// prints volunteers sorted by tasks done (simple selection sort on a copy)
void printLeaderboard(VolunteerList *vl)
{
    if (vl->count == 0) {
        printf("  No volunteers yet.\n");
        return;
    }

    Volunteer *copy = malloc(vl->count * sizeof(Volunteer));
    if (copy == NULL)
        return;
    memcpy(copy, vl->list, vl->count * sizeof(Volunteer));

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
    free(copy);
}

void freeVolunteerList(VolunteerList *vl)
{
    if (vl == NULL)
        return;
    free(vl->list);
    free(vl);
}
