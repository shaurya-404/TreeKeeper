#ifndef UI_H
#define UI_H

#include "treekeeper.h"

// ANSI colour codes
#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"

void clearScreen(void);
const char *statusColor(Tree *t);
void printTreeRow(System *sys, Tree *t);
void printTableHeader(void);
void showDashboard(System *sys);

// input helpers (read a whole line so wrong input does not break scanf)
int readInt(const char *prompt, int *value);
int readFloat(const char *prompt, float *value);
int readString(const char *prompt, char *buf, int size);

#endif
