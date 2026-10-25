/**
 * @file volunteer.h
 * @brief Volunteer roster (dynamic array) with a completed-task leaderboard.
 *
 * Thread safety: none. Only the main (UI) thread uses the roster.
 */
#ifndef VOLUNTEER_H
#define VOLUNTEER_H

#include <stddef.h>

#include "tk_common.h"

typedef struct {
    int      id;
    char     name[TK_NAME_MAX];
    int      zone_id;         /* where the volunteer starts walking from */
    unsigned tasks_completed; /* trees watered: drives the leaderboard   */
} VolunteerInfo;

typedef struct Roster Roster;

Roster *roster_create(void);
/** Safe to call with NULL. */
void roster_destroy(Roster *roster);

/** Registers a volunteer (name must pass tk_validate_name()). */
TkStatus roster_add(Roster *roster, const char *name, int zone_id, int *out_id);

/** Re-inserts a volunteer loaded from disk (keeps its id). */
TkStatus roster_restore(Roster *roster, const VolunteerInfo *info);

TkStatus roster_get(const Roster *roster, int id, VolunteerInfo *out);

/** Returns the volunteer's name, or NULL if @p id is unknown. */
const char *roster_name(const Roster *roster, int id);

/** Adds one completed task to the volunteer's score. */
TkStatus roster_credit(Roster *roster, int id);

size_t roster_count(const Roster *roster);

/** Reads the volunteer stored at position @p index (0..count-1). */
TkStatus roster_at(const Roster *roster, size_t index, VolunteerInfo *out);

/**
 * Copies up to @p k volunteers into @p out ordered by tasks completed
 * (descending, ties by id). Returns the number written.
 */
size_t roster_leaderboard(const Roster *roster, size_t k, VolunteerInfo *out);

#endif /* VOLUNTEER_H */
