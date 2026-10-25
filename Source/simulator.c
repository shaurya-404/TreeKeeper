/**
 * @file simulator.c
 * @brief pthread-based real-time simulation clock.
 */
#include "simulator.h"

#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>

struct Simulator {
    TreeRegistry   *registry; /* not owned */
    pthread_t       thread;
    pthread_mutex_t lock;     /* guards every field below */
    pthread_cond_t  wake;
    bool            running;
    bool            stop_requested;
    bool            paused;
    double          speed;    /* simulated hours per real second */
};

static double monotonic_seconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* Absolute CLOCK_REALTIME deadline @p ms from now (for timedwait). */
static struct timespec deadline_after_ms(long ms)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += ms / 1000;
    ts.tv_nsec += (ms % 1000) * 1000000L;
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000L;
    }
    return ts;
}

static void *simulation_loop(void *arg)
{
    Simulator *sim = arg;
    double last = monotonic_seconds();

    pthread_mutex_lock(&sim->lock);
    while (!sim->stop_requested) {
        struct timespec deadline = deadline_after_ms(SIMULATOR_TICK_MS);
        int rc = 0;
        /* Loop guards against spurious wake-ups. */
        while (!sim->stop_requested && rc != ETIMEDOUT) {
            rc = pthread_cond_timedwait(&sim->wake, &sim->lock, &deadline);
        }
        if (sim->stop_requested) {
            break;
        }

        double now = monotonic_seconds();
        double elapsed = now - last;
        last = now;
        bool paused = sim->paused;
        double speed = sim->speed;

        /* Never hold our lock while taking the registry's lock. */
        pthread_mutex_unlock(&sim->lock);
        if (!paused && elapsed > 0.0) {
            registry_advance(sim->registry, elapsed * speed);
        }
        pthread_mutex_lock(&sim->lock);
    }
    pthread_mutex_unlock(&sim->lock);
    return NULL;
}

Simulator *simulator_create(TreeRegistry *reg, double hours_per_second)
{
    if (reg == NULL || !isfinite(hours_per_second) || hours_per_second <= 0.0 ||
        hours_per_second > SIMULATOR_MAX_SPEED) {
        return NULL;
    }
    Simulator *sim = calloc(1, sizeof *sim);
    if (sim == NULL) {
        return NULL;
    }
    if (pthread_mutex_init(&sim->lock, NULL) != 0) {
        free(sim);
        return NULL;
    }
    if (pthread_cond_init(&sim->wake, NULL) != 0) {
        pthread_mutex_destroy(&sim->lock);
        free(sim);
        return NULL;
    }
    sim->registry = reg;
    sim->speed = hours_per_second;
    return sim;
}

void simulator_destroy(Simulator *sim)
{
    if (sim == NULL) {
        return;
    }
    simulator_stop(sim);
    pthread_cond_destroy(&sim->wake);
    pthread_mutex_destroy(&sim->lock);
    free(sim);
}

TkStatus simulator_start(Simulator *sim)
{
    if (sim == NULL) {
        return TK_ERR_INVALID;
    }
    pthread_mutex_lock(&sim->lock);
    if (sim->running) {
        pthread_mutex_unlock(&sim->lock);
        return TK_ERR_EXISTS;
    }
    sim->stop_requested = false;

    /*
     * Block all signals in the worker so SIGINT (Ctrl+C) is always
     * delivered to the main thread, which owns the graceful shutdown.
     * The new thread inherits the mask; we restore ours afterwards.
     */
    sigset_t all, previous;
    sigfillset(&all);
    pthread_sigmask(SIG_SETMASK, &all, &previous);
    int rc = pthread_create(&sim->thread, NULL, simulation_loop, sim);
    pthread_sigmask(SIG_SETMASK, &previous, NULL);

    sim->running = (rc == 0);
    pthread_mutex_unlock(&sim->lock);
    return (rc == 0) ? TK_OK : TK_ERR_THREAD;
}

void simulator_stop(Simulator *sim)
{
    if (sim == NULL) {
        return;
    }
    pthread_mutex_lock(&sim->lock);
    if (!sim->running) {
        pthread_mutex_unlock(&sim->lock);
        return;
    }
    sim->stop_requested = true;
    pthread_cond_signal(&sim->wake);
    pthread_mutex_unlock(&sim->lock);

    pthread_join(sim->thread, NULL);

    pthread_mutex_lock(&sim->lock);
    sim->running = false;
    pthread_mutex_unlock(&sim->lock);
}

void simulator_set_paused(Simulator *sim, bool paused)
{
    if (sim == NULL) {
        return;
    }
    pthread_mutex_lock(&sim->lock);
    sim->paused = paused;
    pthread_cond_signal(&sim->wake);
    pthread_mutex_unlock(&sim->lock);
}

bool simulator_is_paused(Simulator *sim)
{
    if (sim == NULL) {
        return true;
    }
    pthread_mutex_lock(&sim->lock);
    bool paused = sim->paused;
    pthread_mutex_unlock(&sim->lock);
    return paused;
}

bool simulator_is_running(Simulator *sim)
{
    if (sim == NULL) {
        return false;
    }
    pthread_mutex_lock(&sim->lock);
    bool running = sim->running;
    pthread_mutex_unlock(&sim->lock);
    return running;
}

TkStatus simulator_set_speed(Simulator *sim, double hours_per_second)
{
    if (sim == NULL || !isfinite(hours_per_second) || hours_per_second <= 0.0 ||
        hours_per_second > SIMULATOR_MAX_SPEED) {
        return TK_ERR_INVALID;
    }
    pthread_mutex_lock(&sim->lock);
    sim->speed = hours_per_second;
    pthread_mutex_unlock(&sim->lock);
    return TK_OK;
}

double simulator_speed(Simulator *sim)
{
    if (sim == NULL) {
        return 0.0;
    }
    pthread_mutex_lock(&sim->lock);
    double speed = sim->speed;
    pthread_mutex_unlock(&sim->lock);
    return speed;
}
