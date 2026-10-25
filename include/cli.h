/**
 * @file cli.h
 * @brief Interactive command interpreter and live monitor.
 */
#ifndef CLI_H
#define CLI_H

#include <signal.h>

#include "app.h"

/**
 * Set to 1 by the SIGINT/SIGTERM handler. The CLI notices it, leaves its
 * loop and lets main() shut down cleanly (stop thread, save, free).
 */
extern volatile sig_atomic_t g_cli_stop_requested;

/**
 * Runs the read-eval-print loop until "quit", end of input or a stop
 * request. In non-interactive mode (piped input) each command is echoed
 * and its output printed, which makes scripted test runs readable.
 */
void cli_run(AppContext *app);

/** Executes a single command line (exposed for tests). */
void cli_execute(AppContext *app, char *line);

#endif /* CLI_H */
