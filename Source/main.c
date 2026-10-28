/*
 * TreeKeeper - Afforestation and Maintenance Scheduling System
 * Team PROTOCOL 0 - CSLR31 Data Structures Lab
 *
 * Data structures used:
 *   Min Heap  - which tree needs water first
 *   Hash Map  - find a tree by its ID in O(1)
 *   Graph     - campus zones and paths, Dijkstra for shortest walk
 *   pthreads  - background timer that reduces water levels
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "treekeeper.h"
#include "ui.h"

#define DATA_FOLDER "data"

// asks for a zone name and returns its index, or -1 if not found
static int askZone(System *sys, const char *prompt)
{
    char name[NAME_LEN];
    if (!readString(prompt, name, NAME_LEN)) {
        printf(RED "Invalid name.\n" RESET);
        return -1;
    }
    int z = findZone(sys->map, name);
    if (z == -1)
        printf(RED "Zone '%s' not found. Use option 10 to see all zones.\n" RESET, name);
    return z;
}

static Tree *askTree(System *sys)
{
    int id;
    if (!readInt("Enter tree ID: ", &id)) {
        printf(RED "Invalid ID.\n" RESET);
        return NULL;
    }
    Tree *t = hashSearch(sys->trees, id);
    if (t == NULL)
        printf(RED "No tree with ID %d.\n" RESET, id);
    return t;
}

static void plantTree(System *sys)
{
    char species[NAME_LEN];
    float water;

    printSpeciesList();
    if (!readString("Species: ", species, NAME_LEN) || getDryRate(species) < 0) {
        printf(RED "Unknown species.\n" RESET);
        return;
    }
    int zone = askZone(sys, "Zone name: ");
    if (zone == -1)
        return;
    if (!readFloat("Current water level (0-100): ", &water) || water < 0 || water > 100) {
        printf(RED "Water level must be between 0 and 100.\n" RESET);
        return;
    }

    pthread_mutex_lock(&sys->lock);
    Tree *t = createTree(sys->nextTreeId, species, zone, water);
    if (t == NULL || !hashInsert(sys->trees, t) || !heapInsert(sys->heap, t)) {
        hashDelete(sys->trees, sys->nextTreeId);
        free(t);
        printf(RED "Could not add the tree (out of memory).\n" RESET);
    } else {
        printf(GREEN "Planted %s with ID %d. Print QR code TK-%d for the tree guard.\n" RESET,
               t->species, t->id, t->id);
        sys->nextTreeId++;
    }
    pthread_mutex_unlock(&sys->lock);
}

// called when a volunteer scans the QR code after watering
static void waterTree(System *sys)
{
    Tree *t = askTree(sys);
    if (t == NULL)
        return;

    pthread_mutex_lock(&sys->lock);
    t->water = 100;

    Volunteer *v = findVolunteer(sys->vols, t->assignedTo);
    if (v != NULL) {
        v->tasksDone++;
        printf(GREEN "Tree %d watered by %s.\n" RESET, t->id, v->name);
    } else {
        printf(GREEN "Tree %d watered.\n" RESET, t->id);
    }

    // put it back in the queue (or update its position if it was already there)
    if (t->assignedTo != -1) {
        t->assignedTo = -1;
        heapInsert(sys->heap, t);
    } else {
        heapUpdate(sys->heap, t);
    }
    pthread_mutex_unlock(&sys->lock);
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
    if (!readString("New zone name (no spaces): ", name, NAME_LEN)) {
        printf(RED "Invalid name.\n" RESET);
        return;
    }
    int id = addZone(sys->map, name);
    if (id == -1)
        printf(RED "Could not add zone (maybe it already exists).\n" RESET);
    else
        printf(GREEN "Zone %s added with index %d.\n" RESET, name, id);
}

static void newPath(System *sys)
{
    int a = askZone(sys, "From zone: ");
    if (a == -1)
        return;
    int b = askZone(sys, "To zone: ");
    if (b == -1)
        return;
    int dist;
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
    int a = askZone(sys, "From zone: ");
    if (a == -1)
        return;
    int b = askZone(sys, "To zone: ");
    if (b == -1)
        return;
    printf("\nShortest path (Dijkstra):\n");
    if (shortestPath(sys->map, a, b) == -1)
        printf(RED "  No path exists between these zones.\n" RESET);
}

static void newVolunteer(System *sys)
{
    char name[NAME_LEN];
    if (!readString("Volunteer name (no spaces): ", name, NAME_LEN)) {
        printf(RED "Invalid name.\n" RESET);
        return;
    }
    int zone = askZone(sys, "Starting zone: ");
    if (zone == -1)
        return;
    if (addVolunteer(sys->vols, sys->nextVolunteerId, name, zone, 0)) {
        printf(GREEN "Volunteer %s added with ID %d.\n" RESET, name, sys->nextVolunteerId);
        sys->nextVolunteerId++;
    } else {
        printf(RED "Could not add volunteer.\n" RESET);
    }
}

// gives the k most urgent trees to a volunteer and plans the walking route
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
    int *stops = malloc(k * sizeof(int));
    int count = 0;

    if (stops != NULL) {
        printf("\nTrees assigned to %s:\n", v->name);
        printTableHeader();
        while (count < k && sys->heap->size > 0) {
            Tree *t = extractMin(sys->heap);   // most urgent tree
            t->assignedTo = vid;
            stops[count++] = t->zone;
            printTreeRow(sys, t);
        }

        if (count == 0) {
            printf("  No trees are waiting.\n");
        } else {
            printf("\nSuggested walking route:\n");
            int total = planRoute(sys->map, v->zone, stops, count);
            if (total >= 0)
                printf("  Total walk: %d m\n", total);
        }
        free(stops);
    }
    pthread_mutex_unlock(&sys->lock);
}

static void changeSpeed(System *sys)
{
    float speed;
    if (!readFloat("Simulated hours per real second: ", &speed) || speed <= 0 || speed > 100) {
        printf(RED "Speed must be between 0 and 100.\n" RESET);
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
    printf("  9. Add a path              0. Save and exit\n");
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
    sys.speed = 1;
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
        printf("No saved data found, loading sample campus.\n");
        loadSampleData(&sys);
    }

    startTimer(&sys);
    clearScreen();
    showDashboard(&sys);

    int choice = -1;
    while (choice != 0) {
        printMenu();
        if (!readInt("Enter choice: ", &choice)) {
            if (feof(stdin))
                break;   // input ended (Ctrl+D or end of a test file)
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
        default: printf(RED "Invalid choice.\n" RESET);
        }
    }

    stopTimer(&sys);
    if (saveData(&sys, DATA_FOLDER))
        printf("Data saved to '%s'. Bye!\n", DATA_FOLDER);

    // free everything (the hash map also frees the trees)
    freeHeap(sys.heap);
    freeHashMap(sys.trees);
    freeGraph(sys.map);
    freeVolunteerList(sys.vols);
    pthread_mutex_destroy(&sys.lock);
    return 0;
}
