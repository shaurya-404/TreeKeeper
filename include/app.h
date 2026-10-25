/**
 * @file app.h
 * @brief Composition root: the set of modules a running TreeKeeper owns.
 *
 * This struct is deliberately NOT opaque: it is the glue between modules,
 * not a data structure. Each member is itself an opaque handle.
 */
#ifndef APP_H
#define APP_H

#include <stdbool.h>

#include "graph.h"
#include "registry.h"
#include "simulator.h"
#include "strbuf.h"
#include "volunteer.h"

typedef struct {
    Graph        *graph;
    TreeRegistry *registry;
    Roster       *roster;
    Simulator    *sim;
    StrBuf       *output;      /* text produced by the last command      */
    const char   *data_dir;    /* where state is saved / loaded          */
    bool          interactive; /* stdin and stdout are terminals         */
    bool          quit;        /* set by the quit command                */
} AppContext;

/**
 * Fills empty containers with a demonstration campus (zones, walkways,
 * volunteers and saplings at varied water levels).
 */
TkStatus seed_demo_data(AppContext *app);

#endif /* APP_H */
