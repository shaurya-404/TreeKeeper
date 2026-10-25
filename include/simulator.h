/**
 * @file simulator.h
 * @brief Background POSIX thread that makes simulated time pass.
 *
 * Every SIMULATOR_TICK_MS of real time the worker thread measures how much
 * real time actually elapsed (CLOCK_MONOTONIC), multiplies it by the speed
 * (simulated hours per real second) and calls registry_advance(). The
 * registry's own mutex keeps this safe against the UI thread.
 *
 * The thread sleeps on a condition variable with a timeout instead of
 * sleep(), so pause/resume/stop requests wake it immediately.
 */
#ifndef SIMULATOR_H
#define SIMULATOR_H

#include <stdbool.h>

#include "registry.h"
#include "tk_common.h"

#define SIMULATOR_TICK_MS     250
#define SIMULATOR_MAX_SPEED   1000.0
#define SIMULATOR_DEFAULT_SPEED 1.0 /* 1 simulated hour per real second */

typedef struct Simulator Simulator;

/** Creates a stopped simulator driving @p reg (not owned). */
Simulator *simulator_create(TreeRegistry *reg, double hours_per_second);

/** Stops the thread if running and frees the simulator. NULL-safe. */
void simulator_destroy(Simulator *sim);

/** Starts the worker thread. TK_ERR_EXISTS if already running. */
TkStatus simulator_start(Simulator *sim);

/** Asks the worker to finish and joins it. No-op if not running. */
void simulator_stop(Simulator *sim);

void simulator_set_paused(Simulator *sim, bool paused);
bool simulator_is_paused(Simulator *sim);
bool simulator_is_running(Simulator *sim);

/** Sets the speed in simulated hours per real second (0 < s <= max). */
TkStatus simulator_set_speed(Simulator *sim, double hours_per_second);
double simulator_speed(Simulator *sim);

#endif /* SIMULATOR_H */
