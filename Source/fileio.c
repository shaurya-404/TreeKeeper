#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "treekeeper.h"

// data is stored as CSV files inside one folder:
//   settings.csv    simHours,nextTreeId,nextVolunteerId,speed
//   species.csv     name,dryRate
//   zones.csv       name                    (line number = zone index)
//   paths.csv       zoneA,zoneB,distance
//   trees.csv       id,species,zone,water,diseased,assignedTo
//   volunteers.csv  id,name,zone,tasksDone

static FILE *openFile(const char *folder, const char *file, const char *mode)
{
    char path[256];
    snprintf(path, sizeof(path), "%s/%s", folder, file);
    return fopen(path, mode);
}

int saveData(System *sys, const char *folder)
{
    mkdir(folder, 0755);   // fails harmlessly if the folder already exists

    FILE *fs = openFile(folder, "settings.csv", "w");
    FILE *fz = openFile(folder, "zones.csv", "w");
    FILE *fp = openFile(folder, "paths.csv", "w");
    FILE *ft = openFile(folder, "trees.csv", "w");
    FILE *fv = openFile(folder, "volunteers.csv", "w");
    FILE *fsp = openFile(folder, "species.csv", "w");

    if (!fs || !fz || !fp || !ft || !fv || !fsp) {
        printf("Error: could not open files in '%s' for saving.\n", folder);
        if (fs) fclose(fs);
        if (fz) fclose(fz);
        if (fp) fclose(fp);
        if (ft) fclose(ft);
        if (fv) fclose(fv);
        if (fsp) fclose(fsp);
        return 0;
    }

    pthread_mutex_lock(&sys->lock);

    fprintf(fs, "%.2f,%d,%d,%.2f\n", sys->simHours, sys->nextTreeId, sys->nextVolunteerId, sys->speed);

    for (int i = 0; i < getSpeciesCount(); i++)
        fprintf(fsp, "%s,%.2f\n", getSpecies(i)->name, getSpecies(i)->dryRate);

    for (int i = 0; i < sys->map->count; i++) {
        fprintf(fz, "%s\n", sys->map->zones[i].name);
        for (Edge *e = sys->map->zones[i].head; e != NULL; e = e->next) {
            if (i < e->to)   // each path is stored in both lists, save it once
                fprintf(fp, "%d,%d,%d\n", i, e->to, e->dist);
        }
    }

    int n;
    Tree **all = getAllTrees(sys->trees, &n);
    for (int i = 0; i < n; i++) {
        Tree *t = all[i];
        fprintf(ft, "%d,%s,%d,%.2f,%d,%d\n", t->id, t->species, t->zone, t->water, t->diseased, t->assignedTo);
    }
    free(all);

    for (int i = 0; i < sys->vols->count; i++) {
        Volunteer *v = &sys->vols->list[i];
        fprintf(fv, "%d,%s,%d,%d\n", v->id, v->name, v->zone, v->tasksDone);
    }

    pthread_mutex_unlock(&sys->lock);

    fclose(fs);
    fclose(fz);
    fclose(fp);
    fclose(ft);
    fclose(fv);
    fclose(fsp);
    return 1;
}

// adds a tree to the hash map, and to the heap if it is not assigned
static int addTree(System *sys, Tree *t)
{
    if (!hashInsert(sys->trees, t))
        return 0;
    if (t->assignedTo == -1)
        heapInsert(sys->heap, t);
    return 1;
}

// returns 1 if data was loaded, 0 if there is no saved data
int loadData(System *sys, const char *folder)
{
    char line[200];
    int skipped = 0;

    FILE *fz = openFile(folder, "zones.csv", "r");
    if (fz == NULL)
        return 0;
    while (fgets(line, sizeof(line), fz)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strlen(line) > 0 && addZone(sys->map, line) == -1)
            skipped++;
    }
    fclose(fz);

    FILE *fs = openFile(folder, "settings.csv", "r");
    if (fs != NULL) {
        if (fscanf(fs, "%f,%d,%d,%f", &sys->simHours, &sys->nextTreeId, &sys->nextVolunteerId, &sys->speed) != 4)
            skipped++;
        fclose(fs);
    }

    // species must be loaded before the trees, otherwise trees of a new species are skipped
    FILE *fsp = openFile(folder, "species.csv", "r");
    if (fsp != NULL) {
        char name[NAME_LEN];
        float rate;
        while (fgets(line, sizeof(line), fsp)) {
            if (sscanf(line, "%29[^,],%f", name, &rate) != 2) {
                skipped++;
                continue;
            }
            if (getDryRate(name) < 0 && !addSpecies(name, rate))   // built in species are already there
                skipped++;
        }
        fclose(fsp);
    }

    FILE *fp = openFile(folder, "paths.csv", "r");
    if (fp != NULL) {
        int a, b, d;
        while (fgets(line, sizeof(line), fp)) {
            if (sscanf(line, "%d,%d,%d", &a, &b, &d) != 3 || !addPath(sys->map, a, b, d))
                skipped++;
        }
        fclose(fp);
    }

    FILE *fv = openFile(folder, "volunteers.csv", "r");
    if (fv != NULL) {
        int id, zone, tasks;
        char name[NAME_LEN];
        while (fgets(line, sizeof(line), fv)) {
            if (sscanf(line, "%d,%29[^,],%d,%d", &id, name, &zone, &tasks) != 4 ||
                zone < 0 || zone >= sys->map->count ||
                !addVolunteer(sys->vols, id, name, zone, tasks))
                skipped++;
        }
        fclose(fv);
    }

    FILE *ft = openFile(folder, "trees.csv", "r");
    if (ft != NULL) {
        int id, zone, diseased, assigned;
        float water;
        char species[NAME_LEN];
        while (fgets(line, sizeof(line), ft)) {
            if (sscanf(line, "%d,%29[^,],%d,%f,%d,%d", &id, species, &zone, &water, &diseased, &assigned) != 6 ||
                zone < 0 || zone >= sys->map->count || water < 0 || water > 100) {
                skipped++;
                continue;
            }
            Tree *t = createTree(id, species, zone, water);
            if (t == NULL) {
                skipped++;
                continue;
            }
            t->diseased = diseased;
            t->assignedTo = (findVolunteer(sys->vols, assigned) != NULL) ? assigned : -1;
            if (!addTree(sys, t)) {
                free(t);
                skipped++;
                continue;
            }
            if (id >= sys->nextTreeId)
                sys->nextTreeId = id + 1;
        }
        fclose(ft);
    }

    if (sys->speed <= 0)
        sys->speed = 1;
    if (skipped > 0)
        printf("Warning: %d invalid line(s) in the data files were skipped.\n", skipped);
    return 1;
}

// demo data used the first time the program runs
void loadSampleData(System *sys)
{
    const char *zones[] = {
        "Main_Gate", "Admin_Block", "Library", "Lecture_Hall_Complex", "Orion",
        "Octagon", "Hostel_Zone", "Sports_Complex", "Golden_Jubilee", "Pond_Area"
    };
    int paths[][3] = {
        {0, 1, 450}, {1, 2, 300}, {1, 8, 350}, {2, 3, 250}, {2, 5, 400},
        {3, 4, 200}, {4, 5, 300}, {5, 6, 600}, {6, 7, 350}, {7, 9, 500},
        {8, 9, 700}, {8, 3, 550}, {0, 9, 1200}
    };
    struct { const char *species; int zone; float water; } trees[] = {
        {"Neem", 0, 92}, {"Banyan", 1, 71}, {"Mango", 2, 34}, {"Bamboo", 2, 48},
        {"Peepal", 3, 65}, {"Teak", 4, 22}, {"Gulmohar", 5, 57}, {"Arjuna", 6, 15},
        {"Neem", 6, 80}, {"Bamboo", 7, 28}, {"Mango", 8, 44}, {"Banyan", 9, 88},
        {"Teak", 9, 39}, {"Gulmohar", 0, 12}, {"Peepal", 7, 96}, {"Arjuna", 1, 51}
    };

    for (int i = 0; i < 10; i++)
        addZone(sys->map, zones[i]);
    for (int i = 0; i < 13; i++)
        addPath(sys->map, paths[i][0], paths[i][1], paths[i][2]);

    addVolunteer(sys->vols, sys->nextVolunteerId++, "Shaurya", 6, 0);
    addVolunteer(sys->vols, sys->nextVolunteerId++, "Shravanth", 2, 0);
    addVolunteer(sys->vols, sys->nextVolunteerId++, "Shreya", 1, 0);

    for (int i = 0; i < 16; i++) {
        Tree *t = createTree(sys->nextTreeId, trees[i].species, trees[i].zone, trees[i].water);
        if (t != NULL && addTree(sys, t))
            sys->nextTreeId++;
        else
            free(t);
    }
}
