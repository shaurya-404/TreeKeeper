/**
 * @file seed.c
 * @brief Demonstration data: a campus map loosely modelled on NIT Trichy.
 *
 * Distances are illustrative walking distances in metres, not surveyed.
 */
#include "app.h"

typedef struct {
    const char *a;
    const char *b;
    double      meters;
} SeedPath;

typedef struct {
    Species     species;
    const char *zone;
    double      water;
} SeedTree;

typedef struct {
    const char *name;
    const char *zone;
} SeedVolunteer;

static const char *const SEED_ZONES[] = {
    "Main_Gate", "Admin_Block", "Library", "Lecture_Hall_Complex", "Orion",
    "Octagon", "Hostel_Zone", "Sports_Complex", "Golden_Jubilee", "Pond_Area",
};

static const SeedPath SEED_PATHS[] = {
    { "Main_Gate",            "Admin_Block",          450 },
    { "Admin_Block",          "Library",              300 },
    { "Admin_Block",          "Golden_Jubilee",       350 },
    { "Library",              "Lecture_Hall_Complex", 250 },
    { "Library",              "Octagon",              400 },
    { "Lecture_Hall_Complex", "Orion",                200 },
    { "Orion",                "Octagon",              300 },
    { "Octagon",              "Hostel_Zone",          600 },
    { "Hostel_Zone",          "Sports_Complex",       350 },
    { "Sports_Complex",       "Pond_Area",            500 },
    { "Golden_Jubilee",       "Pond_Area",            700 },
    { "Golden_Jubilee",       "Lecture_Hall_Complex", 550 },
    { "Main_Gate",            "Pond_Area",           1200 },
};

static const SeedTree SEED_TREES[] = {
    { SPECIES_NEEM,     "Main_Gate",            92 },
    { SPECIES_BANYAN,   "Admin_Block",          71 },
    { SPECIES_MANGO,    "Library",              34 },
    { SPECIES_BAMBOO,   "Library",              48 },
    { SPECIES_PEEPAL,   "Lecture_Hall_Complex", 65 },
    { SPECIES_TEAK,     "Orion",                22 },
    { SPECIES_GULMOHAR, "Octagon",              57 },
    { SPECIES_ARJUNA,   "Hostel_Zone",          15 },
    { SPECIES_NEEM,     "Hostel_Zone",          80 },
    { SPECIES_BAMBOO,   "Sports_Complex",       28 },
    { SPECIES_MANGO,    "Golden_Jubilee",       44 },
    { SPECIES_BANYAN,   "Pond_Area",            88 },
    { SPECIES_TEAK,     "Pond_Area",            39 },
    { SPECIES_GULMOHAR, "Main_Gate",            12 },
    { SPECIES_PEEPAL,   "Sports_Complex",       96 },
    { SPECIES_ARJUNA,   "Admin_Block",          51 },
};

static const SeedVolunteer SEED_VOLUNTEERS[] = {
    { "Shaurya",   "Hostel_Zone" },
    { "Shravanth", "Library" },
    { "Shreya",    "Admin_Block" },
};

TkStatus seed_demo_data(AppContext *app)
{
    if (app == NULL || app->graph == NULL || app->registry == NULL || app->roster == NULL) {
        return TK_ERR_INVALID;
    }

    TkStatus st = TK_OK;
    for (size_t i = 0; i < TK_ARRAY_LEN(SEED_ZONES) && st == TK_OK; i++) {
        st = graph_add_zone(app->graph, SEED_ZONES[i], NULL);
    }
    for (size_t i = 0; i < TK_ARRAY_LEN(SEED_PATHS) && st == TK_OK; i++) {
        st = graph_add_path(app->graph,
                            graph_find_zone(app->graph, SEED_PATHS[i].a),
                            graph_find_zone(app->graph, SEED_PATHS[i].b),
                            SEED_PATHS[i].meters);
    }
    for (size_t i = 0; i < TK_ARRAY_LEN(SEED_VOLUNTEERS) && st == TK_OK; i++) {
        st = roster_add(app->roster, SEED_VOLUNTEERS[i].name,
                        graph_find_zone(app->graph, SEED_VOLUNTEERS[i].zone), NULL);
    }
    for (size_t i = 0; i < TK_ARRAY_LEN(SEED_TREES) && st == TK_OK; i++) {
        st = registry_plant(app->registry, SEED_TREES[i].species,
                            graph_find_zone(app->graph, SEED_TREES[i].zone),
                            SEED_TREES[i].water, NULL);
    }
    return st;
}
