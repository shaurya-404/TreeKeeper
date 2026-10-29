#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ui.h"

#define DASHBOARD_ROWS 5

void clearScreen(void)
{
    printf("\033[2J\033[H");
}

const char *statusColor(Tree *t)
{
    if (t->diseased)
        return MAGENTA;
    if (t->water < 30)
        return RED;
    if (t->water < 60)
        return YELLOW;
    return GREEN;
}

// prints a bar like [#######---] for the water level
static void printWaterBar(Tree *t)
{
    int filled = (int)(t->water / 10 + 0.5);
    printf("%s[", statusColor(t));
    for (int i = 0; i < 10; i++)
        printf(i < filled ? "#" : "-");
    printf("] %5.1f%%" RESET, t->water);
}

void printTableHeader(void)
{
    printf(BOLD "  %-5s %-9s %-21s %-19s %-8s %-9s %s\n" RESET,
           "ID", "Species", "Zone", "Water", "Hrs left", "Status", "Assigned");
}

void printTreeRow(System *sys, Tree *t)
{
    Volunteer *v = findVolunteer(sys->vols, t->assignedTo);

    printf("  %-5d %-9s %-21s ", t->id, t->species, sys->map->zones[t->zone].name);
    printWaterBar(t);
    printf(" %7.1f  %s%-9s" RESET " %s\n", hoursLeft(t), statusColor(t), getStatus(t),
           v != NULL ? v->name : "-");
}

void showDashboard(System *sys)
{
    pthread_mutex_lock(&sys->lock);

    int day = (int)(sys->simHours / 24) + 1;
    int hour = (int)sys->simHours % 24;

    printf(BOLD GREEN "\n=============== TREEKEEPER DASHBOARD ===============\n" RESET);
    printf("  Time: Day %d, %02d:00   Speed: %.1f hrs/sec   %s\n", day, hour, sys->speed,
           sys->paused ? YELLOW "[PAUSED]" RESET : GREEN "[RUNNING]" RESET);

    // count trees by status
    int n, healthy = 0, thirsty = 0, critical = 0, wilted = 0, diseased = 0, assigned = 0;
    Tree **all = getAllTrees(sys->trees, &n);
    for (int i = 0; i < n; i++) {
        const char *s = getStatus(all[i]);
        if (strcmp(s, "HEALTHY") == 0) healthy++;
        else if (strcmp(s, "THIRSTY") == 0) thirsty++;
        else if (strcmp(s, "CRITICAL") == 0) critical++;
        else if (strcmp(s, "WILTED") == 0) wilted++;
        else diseased++;
        if (all[i]->assignedTo != -1)
            assigned++;
    }
    free(all);

    printf("  Trees: %d   " GREEN "Healthy %d  " YELLOW "Thirsty %d  " RED "Critical %d  Wilted %d  "
           MAGENTA "Diseased %d" RESET "\n", n, healthy, thirsty, critical, wilted, diseased);
    printf("  Waiting in queue: %d   Assigned to volunteers: %d\n", sys->heap->size, assigned);
    if (critical + wilted > 0)
        printf(RED BOLD "  ALERT: %d tree(s) need water urgently!\n" RESET, critical + wilted);

    // show the most urgent trees without changing the real heap:
    // copy the heap array into a temporary heap and extract from the copy
    printf(BOLD CYAN "\n  Most urgent trees (from min heap):\n" RESET);
    if (sys->heap->size == 0) {
        printf("  Queue is empty.\n");
    } else {
        MinHeap *copy = createHeap(sys->heap->size);
        if (copy != NULL) {
            for (int i = 0; i < sys->heap->size; i++)
                copy->arr[i] = sys->heap->arr[i];
            copy->size = sys->heap->size;

            printTableHeader();
            for (int i = 0; i < DASHBOARD_ROWS && copy->size > 0; i++)
                printTreeRow(sys, extractMin(copy));
            freeHeap(copy);
        }
    }

    // extracting from the copy changed the trees' heapIndex values,
    // so set them back to their positions in the real heap
    for (int i = 0; i < sys->heap->size; i++)
        sys->heap->arr[i]->heapIndex = i;

    printf(BOLD CYAN "\n  Volunteer leaderboard:\n" RESET);
    printLeaderboard(sys->vols);
    printf(BOLD GREEN "====================================================\n" RESET);

    pthread_mutex_unlock(&sys->lock);
}

// reads one line and converts it to an int, returns 0 if input is invalid
int readInt(const char *prompt, int *value)
{
    char line[100];
    char extra;
    printf("%s", prompt);
    if (fgets(line, sizeof(line), stdin) == NULL)
        return 0;
    return sscanf(line, "%d %c", value, &extra) == 1;
}

int readFloat(const char *prompt, float *value)
{
    char line[100];
    char extra;
    printf("%s", prompt);
    if (fgets(line, sizeof(line), stdin) == NULL)
        return 0;
    return sscanf(line, "%f %c", value, &extra) == 1;
}

// reads one word. Spaces inside the word are changed to '_' (so "Main Gate"
// becomes "Main_Gate"). Commas are not allowed because they would break the CSV files.
int readString(const char *prompt, char *buf, int size)
{
    char line[100];
    printf("%s", prompt);
    if (fgets(line, sizeof(line), stdin) == NULL)
        return 0;
    line[strcspn(line, "\r\n")] = '\0';

    // skip spaces at the start and remove spaces at the end
    char *word = line;
    while (*word == ' ' || *word == '\t')
        word++;
    int len = strlen(word);
    while (len > 0 && (word[len - 1] == ' ' || word[len - 1] == '\t'))
        word[--len] = '\0';

    if (len == 0 || len >= size)
        return 0;
    for (int i = 0; i < len; i++) {
        if (word[i] == ',')
            return 0;
        if (word[i] == ' ' || word[i] == '\t')
            word[i] = '_';
    }
    strcpy(buf, word);
    return 1;
}
