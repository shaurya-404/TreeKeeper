/**
 * @file volunteer.c
 * @brief Volunteer roster implementation.
 */
#include "volunteer.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define ROSTER_MIN_CAPACITY 8u

struct Roster {
    VolunteerInfo *items;
    size_t         count;
    size_t         capacity;
    int            next_id;
};

Roster *roster_create(void)
{
    Roster *roster = malloc(sizeof *roster);
    if (roster == NULL) {
        return NULL;
    }
    roster->items = tk_reallocarray(NULL, ROSTER_MIN_CAPACITY, sizeof *roster->items);
    if (roster->items == NULL) {
        free(roster);
        return NULL;
    }
    roster->count = 0;
    roster->capacity = ROSTER_MIN_CAPACITY;
    roster->next_id = 1;
    return roster;
}

void roster_destroy(Roster *roster)
{
    if (roster == NULL) {
        return;
    }
    free(roster->items);
    free(roster);
}

/* Linear search: rosters are small (tens of people), so O(n) is fine. */
static VolunteerInfo *find_by_id(const Roster *roster, int id)
{
    for (size_t i = 0; i < roster->count; i++) {
        if (roster->items[i].id == id) {
            return &roster->items[i];
        }
    }
    return NULL;
}

static bool name_taken(const Roster *roster, const char *name)
{
    for (size_t i = 0; i < roster->count; i++) {
        if (strcasecmp(roster->items[i].name, name) == 0) {
            return true;
        }
    }
    return false;
}

static TkStatus append(Roster *roster, const VolunteerInfo *info)
{
    if (roster->count == roster->capacity) {
        size_t new_capacity = roster->capacity * 2;
        VolunteerInfo *grown = tk_reallocarray(roster->items, new_capacity, sizeof *grown);
        if (grown == NULL) {
            return TK_ERR_NOMEM;
        }
        roster->items = grown;
        roster->capacity = new_capacity;
    }
    roster->items[roster->count++] = *info;
    return TK_OK;
}

TkStatus roster_add(Roster *roster, const char *name, int zone_id, int *out_id)
{
    if (roster == NULL || zone_id < 0 || tk_validate_name(name) != TK_OK) {
        return TK_ERR_INVALID;
    }
    if (name_taken(roster, name)) {
        return TK_ERR_EXISTS;
    }

    VolunteerInfo info;
    memset(&info, 0, sizeof info);
    info.id = roster->next_id;
    memcpy(info.name, name, strlen(name) + 1);
    info.zone_id = zone_id;

    TkStatus st = append(roster, &info);
    if (st == TK_OK) {
        roster->next_id++;
        if (out_id != NULL) {
            *out_id = info.id;
        }
    }
    return st;
}

TkStatus roster_restore(Roster *roster, const VolunteerInfo *info)
{
    if (roster == NULL || info == NULL || info->id <= 0 || info->zone_id < 0 ||
        memchr(info->name, '\0', sizeof info->name) == NULL ||
        tk_validate_name(info->name) != TK_OK) {
        return TK_ERR_INVALID;
    }
    if (find_by_id(roster, info->id) != NULL || name_taken(roster, info->name)) {
        return TK_ERR_EXISTS;
    }
    TkStatus st = append(roster, info);
    if (st == TK_OK && info->id >= roster->next_id) {
        roster->next_id = info->id + 1;
    }
    return st;
}

TkStatus roster_get(const Roster *roster, int id, VolunteerInfo *out)
{
    if (roster == NULL || out == NULL) {
        return TK_ERR_INVALID;
    }
    const VolunteerInfo *v = find_by_id(roster, id);
    if (v == NULL) {
        return TK_ERR_NOT_FOUND;
    }
    *out = *v;
    return TK_OK;
}

const char *roster_name(const Roster *roster, int id)
{
    if (roster == NULL) {
        return NULL;
    }
    const VolunteerInfo *v = find_by_id(roster, id);
    return (v != NULL) ? v->name : NULL;
}

TkStatus roster_credit(Roster *roster, int id)
{
    if (roster == NULL) {
        return TK_ERR_INVALID;
    }
    VolunteerInfo *v = find_by_id(roster, id);
    if (v == NULL) {
        return TK_ERR_NOT_FOUND;
    }
    v->tasks_completed++;
    return TK_OK;
}

size_t roster_count(const Roster *roster)
{
    return (roster == NULL) ? 0 : roster->count;
}

TkStatus roster_at(const Roster *roster, size_t index, VolunteerInfo *out)
{
    if (roster == NULL || out == NULL || index >= roster->count) {
        return TK_ERR_INVALID;
    }
    *out = roster->items[index];
    return TK_OK;
}

static int compare_leaderboard(const void *a, const void *b)
{
    const VolunteerInfo *va = a;
    const VolunteerInfo *vb = b;
    if (va->tasks_completed != vb->tasks_completed) {
        return (va->tasks_completed < vb->tasks_completed) ? 1 : -1;
    }
    return (va->id > vb->id) - (va->id < vb->id);
}

size_t roster_leaderboard(const Roster *roster, size_t k, VolunteerInfo *out)
{
    if (roster == NULL || out == NULL || k == 0 || roster->count == 0) {
        return 0;
    }
    VolunteerInfo *sorted = tk_reallocarray(NULL, roster->count, sizeof *sorted);
    if (sorted == NULL) {
        return 0;
    }
    memcpy(sorted, roster->items, roster->count * sizeof *sorted);
    qsort(sorted, roster->count, sizeof *sorted, compare_leaderboard);

    size_t n = (k < roster->count) ? k : roster->count;
    memcpy(out, sorted, n * sizeof *out);
    free(sorted);
    return n;
}
