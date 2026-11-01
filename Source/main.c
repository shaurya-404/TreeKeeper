// TreeKeeper - Team PROTOCOL 0, CSLR31 Data Structures Lab
// min heap: which tree needs water first, hash map: tree by ID,
// graph: campus zones with Dijkstra, pthread: timer that dries the trees

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "treekeeper.h"

#define DATA_FOLDER "data"

static int isNumber(const char *s)
{
    if (*s == '\0')
        return 0;
    for (int i = 0; s[i] != '\0'; i++) {
        if (s[i] < '0' || s[i] > '9')
            return 0;
    }
    return 1;
}

// zone can be typed as its ID or its name. what was typed is left in name
static int askZone(System *sys, const char *prompt, char *name)
{
    if (!readString(prompt, name, NAME_LEN)) {
        printf(RED "Invalid zone.\n" RESET);
        name[0] = '\0';
        return -1;
    }

    int z = -1;
    if (isNumber(name)) {
        z = atoi(name);
        if (z >= sys->map->count)
            z = -1;
    } else {
        z = findZone(sys->map, name);
    }
    if (z == -1)
        printf(RED "Zone '%s' not found. Use option 10 to see all zones.\n" RESET, name);
    return z;
}

static int askYesNo(const char *prompt)
{
    char answer[NAME_LEN];
    if (!readString(prompt, answer, NAME_LEN))
        return 0;
    return answer[0] == 'y' || answer[0] == 'Y';
}

static int newSpeciesRate(const char *name)
{
    float rate;
    if (!readFloat("Water lost per hour in % (e.g. 1.5): ", &rate) || rate <= 0 || rate > 100) {
        printf(RED "Rate must be between 0 and 100.\n" RESET);
        return 0;
    }
    if (!addSpecies(name, rate)) {
        printf(RED "Could not add species (it already exists or the list is full).\n" RESET);
        return 0;
    }
    printf(GREEN "Species %s added.\n" RESET, name);
    return 1;
}

static Tree *askTree(System *sys)
{
    int id;
    if (!readInt("Enter tree ID: ", &id)) {
        printf(RED "Invalid ID.\n" RESET);
        return NULL;
    }
    pthread_mutex_lock(&sys->lock);
    Tree *t = hashSearch(sys->trees, id);
    pthread_mutex_unlock(&sys->lock);
    if (t == NULL)
        printf(RED "No tree with ID %d.\n" RESET, id);
    return t;
}

static void plantTree(System *sys)
{
    char species[NAME_LEN], zoneName[NAME_LEN];
    float water;

    printSpeciesList();
    if (!readString("Species: ", species, NAME_LEN)) {
        printf(RED "Invalid species name.\n" RESET);
        return;
    }
    if (getDryRate(species) < 0) {
        printf(YELLOW "'%s' is not in the species list.\n" RESET, species);
        if (!askYesNo("Add it as a new species? (y/n): ") || !newSpeciesRate(species))
            return;
    }

    printf("Zones:\n");
    for (int i = 0; i < sys->map->count; i++)
        printf("  [%d] %s\n", i, sys->map->zones[i].name);

    int zone = askZone(sys, "Zone (ID or name): ", zoneName);
    if (zone == -1) {
        // a new name can be added as a zone, a wrong ID cannot
        if (zoneName[0] == '\0' || isNumber(zoneName) || !askYesNo("Add it as a new zone? (y/n): "))
            return;
        zone = addZone(sys->map, zoneName);
        if (zone == -1) {
            printf(RED "Could not add zone.\n" RESET);
            return;
        }
        printf(GREEN "Zone %s added with ID %d. Use option 9 to connect it with a path.\n" RESET,
               zoneName, zone);
    }

    if (!readFloat("Current water level (0-100): ", &water) || water < 0 || water > 100) {
        printf(RED "Water level must be between 0 and 100.\n" RESET);
        return;
    }

    pthread_mutex_lock(&sys->lock);
    Tree *t = createTree(sys->nextTreeId, species, zone, water);
    if (t == NULL || !hashInsert(sys->trees, t)) {
        free(t);
        printf(RED "Could not add the tree.\n" RESET);
    } else if (!heapInsert(sys->heap, t)) {
        hashDelete(sys->trees, t->id);
        free(t);
        printf(RED "Could not add the tree.\n" RESET);
    } else {
        printf(GREEN "Planted %s in %s with ID %d. Print QR code TK-%d for the tree guard.\n" RESET,
               t->species, sys->map->zones[zone].name, t->id, t->id);
        sys->nextTreeId++;
    }
    pthread_mutex_unlock(&sys->lock);
}

static void newSpecies(void)
{
    char name[NAME_LEN];
    printSpeciesList();
    if (!readString("New species name: ", name, NAME_LEN)) {
        printf(RED "Invalid name.\n" RESET);
        return;
    }
    if (getDryRate(name) >= 0) {
        printf(RED "%s is already in the list.\n" RESET, name);
        return;
    }
    newSpeciesRate(name);
}

// volunteer scans the QR code (tree ID) after watering
static void waterTree(System *sys)
{
    char input[NAME_LEN];

    if (sys->vols->count == 0) {
        printf(RED "Add a volunteer first (option 12).\n" RESET);
        return;
    }
    Tree *t = askTree(sys);
    if (t == NULL)
        return;

    printLeaderboard(sys->vols);
    if (!readString("Watered by (volunteer ID or name): ", input, NAME_LEN)) {
        printf(RED "Invalid input.\n" RESET);
        return;
    }
    Volunteer *v;
    if (isNumber(input))
        v = findVolunteer(sys->vols, atoi(input));
    else
        v = findVolunteerByName(sys->vols, input);
    if (v == NULL) {
        printf(RED "No volunteer '%s'.\n" RESET, input);
        return;
    }

    pthread_mutex_lock(&sys->lock);
    if (t->water >= 100) {
        printf(YELLOW "Tree %d is already fully watered (100.0%%).\n" RESET, t->id);
    } else {
        t->water = 100;
        v->tasksDone++;
        printf(GREEN "Tree %d watered by %s. %s has watered %d tree(s).\n" RESET,
               t->id, v->name, v->name, v->tasksDone);
    }

    // task is done, so the tree goes back in the queue with its new priority
    t->assignedTo = -1;
    if (t->heapIndex == -1)
        heapInsert(sys->heap, t);
    else
        heapUpdate(sys->heap, t);
    pthread_mutex_unlock(&sys->lock);

    saveData(sys, DATA_FOLDER);   // so the count is not lost if the program is closed
}

static void searchTree(System *sys)
{
    Tree *t = askTree(sys);
    if (t == NULL)
        return;
    pthread_mutex_lock(&sys->lock);
    printTableHeader();
    printTreeRow(sys, t);
    printf("  Loses %.1f%% water per hour\n", t->dryRate);
    pthread_mutex_unlock(&sys->lock);
}

static void setDisease(System *sys)
{
    Tree *t = askTree(sys);
    int choice;
    if (t == NULL)
        return;
    if (!readInt("1 = mark diseased, 0 = mark healthy: ", &choice) || (choice != 0 && choice != 1)) {
        printf(RED "Enter 0 or 1.\n" RESET);
        return;
    }
    pthread_mutex_lock(&sys->lock);
    t->diseased = choice;
    pthread_mutex_unlock(&sys->lock);
    printf(GREEN "Tree %d updated.\n" RESET, t->id);
}

static void removeTree(System *sys)
{
    Tree *t = askTree(sys);
    if (t == NULL)
        return;
    pthread_mutex_lock(&sys->lock);
    int id = t->id;
    heapRemove(sys->heap, t);
    hashDelete(sys->trees, id);
    free(t);
    pthread_mutex_unlock(&sys->lock);
    printf(GREEN "Tree %d removed.\n" RESET, id);
}

static void listTrees(System *sys)
{
    pthread_mutex_lock(&sys->lock);
    int n;
    Tree **all = getAllTrees(sys->trees, &n);
    printf("\nAll trees (%d):\n", n);
    printTableHeader();
    for (int i = 0; i < n; i++)
        printTreeRow(sys, all[i]);
    free(all);
    pthread_mutex_unlock(&sys->lock);
}

static void newZone(System *sys)
{
    char name[NAME_LEN];
    if (!readString("New zone name: ", name, NAME_LEN)) {
        printf(RED "Invalid name.\n" RESET);
        return;
    }
    if (isNumber(name)) {   // numbers are read as zone IDs
        printf(RED "Zone name cannot be only a number.\n" RESET);
        return;
    }
    int id = addZone(sys->map, name);
    if (id == -1)
        printf(RED "Could not add zone (maybe it already exists).\n" RESET);
    else
        printf(GREEN "Zone %s added with ID %d.\n" RESET, name, id);
}

static void newPath(System *sys)
{
    char name[NAME_LEN];
    int dist;
    int a = askZone(sys, "From zone (ID or name): ", name);
    if (a == -1)
        return;
    int b = askZone(sys, "To zone (ID or name): ", name);
    if (b == -1)
        return;
    if (!readInt("Distance in metres: ", &dist) || dist <= 0) {
        printf(RED "Distance must be a positive number.\n" RESET);
        return;
    }
    if (addPath(sys->map, a, b, dist))
        printf(GREEN "Path added.\n" RESET);
    else
        printf(RED "Could not add path.\n" RESET);
}

static void findRoute(System *sys)
{
    char name[NAME_LEN];
    int a = askZone(sys, "From zone (ID or name): ", name);
    if (a == -1)
        return;
    int b = askZone(sys, "To zone (ID or name): ", name);
    if (b == -1)
        return;
    printf("\nShortest path (Dijkstra):\n");
    if (shortestPath(sys->map, a, b) == -1)
        printf(RED "  No path exists between these zones.\n" RESET);
}

static void newVolunteer(System *sys)
{
    char name[NAME_LEN];
    if (!readString("Volunteer name: ", name, NAME_LEN)) {
        printf(RED "Invalid name.\n" RESET);
        return;
    }
    char zoneName[NAME_LEN];
    int zone = askZone(sys, "Starting zone (ID or name): ", zoneName);
    if (zone == -1)
        return;
    if (addVolunteer(sys->vols, sys->nextVolunteerId, name, zone, 0)) {
        printf(GREEN "Volunteer %s added with ID %d.\n" RESET, name, sys->nextVolunteerId);
        sys->nextVolunteerId++;
    } else {
        printf(RED "Could not add volunteer.\n" RESET);
    }
}

// gives the k most urgent trees to a volunteer and plans the walk
static void assignTrees(System *sys)
{
    int vid, k;
    printLeaderboard(sys->vols);
    if (!readInt("Volunteer ID: ", &vid) || findVolunteer(sys->vols, vid) == NULL) {
        printf(RED "No volunteer with that ID.\n" RESET);
        return;
    }
    if (!readInt("How many trees? ", &k) || k <= 0) {
        printf(RED "Enter a positive number.\n" RESET);
        return;
    }

    pthread_mutex_lock(&sys->lock);
    Volunteer *v = findVolunteer(sys->vols, vid);
    if (k > sys->heap->size)
        k = sys->heap->size;

    if (k == 0) {
        printf("  No trees are waiting.\n");
    } else {
        int stops[k];
        printf("\nTrees assigned to %s:\n", v->name);
        printTableHeader();
        for (int i = 0; i < k; i++) {
            Tree *t = extractMin(sys->heap);
            t->assignedTo = vid;
            stops[i] = t->zone;
            printTreeRow(sys, t);
        }

        printf("\nSuggested walking route:\n");
        int total = planRoute(sys->map, v->zone, stops, k);
        if (total >= 0)
            printf("  Total walk: %d m\n", total);
    }
    pthread_mutex_unlock(&sys->lock);
}

static void changeSpeed(System *sys)
{
    float speed;
    if (!readFloat("Simulated hours per real second: ", &speed) || speed <= 0 || speed > 24) {
        printf(RED "Speed must be between 0 and 24.\n" RESET);
        return;
    }
    pthread_mutex_lock(&sys->lock);
    sys->speed = speed;
    pthread_mutex_unlock(&sys->lock);
    printf(GREEN "Speed changed.\n" RESET);
}

static void printMenu(void)
{
    printf(BOLD "\n------------------- MENU -------------------\n" RESET);
    printf("  1. Dashboard              10. Show campus map\n");
    printf("  2. Plant a tree           11. Shortest route\n");
    printf("  3. Water a tree (QR)      12. Add volunteer\n");
    printf("  4. Search tree by ID      13. Show volunteers\n");
    printf("  5. Mark diseased/healthy  14. Assign urgent trees\n");
    printf("  6. Remove a tree          15. Pause/resume timer\n");
    printf("  7. Show all trees         16. Change speed\n");
    printf("  8. Add a zone             17. Save now\n");
    printf("  9. Add a path             18. Add plant species\n");
    printf("                             0. Save and exit\n");
}

int main(void)
{
    System sys;
    sys.trees = createHashMap(16);
    sys.heap = createHeap(16);
    sys.map = createGraph(8);
    sys.vols = createVolunteerList(4);
    sys.nextTreeId = 1001;
    sys.nextVolunteerId = 1;
    sys.simHours = 0;
    sys.speed = 0.05;
    sys.paused = 0;
    sys.running = 0;

    if (!sys.trees || !sys.heap || !sys.map || !sys.vols) {
        printf("Out of memory!\n");
        freeHashMap(sys.trees);
        freeHeap(sys.heap);
        freeGraph(sys.map);
        freeVolunteerList(sys.vols);
        return 1;
    }
    pthread_mutex_init(&sys.lock, NULL);

    if (loadData(&sys, DATA_FOLDER))
        printf("Loaded saved data from '%s'.\n", DATA_FOLDER);
    else {
        printf("No saved data found, starting with an empty campus.\n");
    }

    startTimer(&sys);
    clearScreen();
    showDashboard(&sys);

    int choice = -1;
    while (choice != 0) {
        printMenu();
        if (!readInt("Enter choice: ", &choice)) {
            if (feof(stdin))
                break;   // Ctrl+D or end of an input file
            printf(RED "Please enter a number.\n" RESET);
            continue;
        }

        switch (choice) {
        case 0:  break;
        case 1:  clearScreen(); showDashboard(&sys); break;
        case 2:  plantTree(&sys); break;
        case 3:  waterTree(&sys); break;
        case 4:  searchTree(&sys); break;
        case 5:  setDisease(&sys); break;
        case 6:  removeTree(&sys); break;
        case 7:  listTrees(&sys); break;
        case 8:  newZone(&sys); break;
        case 9:  newPath(&sys); break;
        case 10: printGraph(sys.map); break;
        case 11: findRoute(&sys); break;
        case 12: newVolunteer(&sys); break;
        case 13: printLeaderboard(sys.vols); break;
        case 14: assignTrees(&sys); break;
        case 15:
            pthread_mutex_lock(&sys.lock);
            sys.paused = !sys.paused;
            pthread_mutex_unlock(&sys.lock);
            printf("Timer %s.\n", sys.paused ? "paused" : "resumed");
            break;
        case 16: changeSpeed(&sys); break;
        case 17:
            if (saveData(&sys, DATA_FOLDER))
                printf(GREEN "Saved.\n" RESET);
            break;
        case 18: newSpecies(); break;
        default: printf(RED "Invalid choice.\n" RESET);
        }
    }

    stopTimer(&sys);
    if (saveData(&sys, DATA_FOLDER))
        printf("Data saved to '%s'. Bye!\n", DATA_FOLDER);

    // the hash map also frees the trees
    freeHeap(sys.heap);
    freeHashMap(sys.trees);
    freeGraph(sys.map);
    freeVolunteerList(sys.vols);
    pthread_mutex_destroy(&sys.lock);
    return 0;
}
