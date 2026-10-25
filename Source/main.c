/**
 * @file main.c
 * @brief Entry point: parses options, wires the modules together, and
 *        guarantees an orderly shutdown (stop thread -> save -> free).
 *
 * TreeKeeper - A Data-Driven Afforestation and Maintenance Scheduling System
 * Team PROTOCOL 0, CSLR31 Data Structures Laboratory, NIT Tiruchirappalli.
 */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "app.h"
#include "cli.h"
#include "storage.h"
#include "ui.h"

#define DEFAULT_DATA_DIR "data"

typedef struct {
    const char *data_dir;
    double      speed;       /* <= 0 means "use saved or default" */
    bool        fresh;       /* ignore saved state, load demo data */
    bool        empty;       /* ignore saved state, start blank    */
    bool        no_save;
    bool        no_color;
} Options;

static void print_usage(const char *prog)
{
    printf("Usage: %s [options]\n\n"
           "  -d, --data DIR     directory for saved state (default: %s)\n"
           "  -s, --speed H      simulated hours per real second (default: %.1f)\n"
           "      --fresh        ignore saved state and load the demo campus\n"
           "      --empty        ignore saved state and start with no data\n"
           "      --no-save      do not write state on exit\n"
           "      --no-color     disable ANSI colours\n"
           "  -h, --help         show this message\n",
           prog, DEFAULT_DATA_DIR, SIMULATOR_DEFAULT_SPEED);
}

/* Returns 0 to continue, 1 to exit successfully, -1 on a usage error. */
static int parse_options(int argc, char **argv, Options *opt)
{
    *opt = (Options){ .data_dir = DEFAULT_DATA_DIR, .speed = 0.0 };
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        bool has_value = (i + 1 < argc);
        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            print_usage(argv[0]);
            return 1;
        } else if ((strcmp(arg, "-d") == 0 || strcmp(arg, "--data") == 0) && has_value) {
            opt->data_dir = argv[++i];
        } else if ((strcmp(arg, "-s") == 0 || strcmp(arg, "--speed") == 0) && has_value) {
            if (tk_parse_double(argv[++i], 0.001, SIMULATOR_MAX_SPEED, &opt->speed) != TK_OK) {
                fprintf(stderr, "error: --speed must be between 0.001 and %.0f\n", SIMULATOR_MAX_SPEED);
                return -1;
            }
        } else if (strcmp(arg, "--fresh") == 0) {
            opt->fresh = true;
        } else if (strcmp(arg, "--empty") == 0) {
            opt->empty = true;
        } else if (strcmp(arg, "--no-save") == 0) {
            opt->no_save = true;
        } else if (strcmp(arg, "--no-color") == 0) {
            opt->no_color = true;
        } else {
            fprintf(stderr, "error: unknown or incomplete option '%s'\n", arg);
            print_usage(argv[0]);
            return -1;
        }
    }
    if (opt->fresh && opt->empty) {
        fprintf(stderr, "error: --fresh and --empty are mutually exclusive\n");
        return -1;
    }
    return 0;
}

static void handle_stop_signal(int signo)
{
    (void)signo;
    g_cli_stop_requested = 1; /* async-signal-safe: just set a flag */
}

static void install_signal_handlers(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = handle_stop_signal;
    sigemptyset(&sa.sa_mask);
    /* No SA_RESTART: a blocking getline() returns so the loop can exit. */
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

static void app_destroy(AppContext *app)
{
    /* The thread must stop before the registry it uses is freed. */
    simulator_destroy(app->sim);
    registry_destroy(app->registry);
    graph_destroy(app->graph);
    roster_destroy(app->roster);
    strbuf_destroy(app->output);
}

/* Loads saved state or seeds demo data. Returns the speed to use. */
static TkStatus load_initial_state(AppContext *app, const Options *opt, double *speed)
{
    *speed = SIMULATOR_DEFAULT_SPEED;
    if (opt->empty) {
        return TK_OK;
    }
    if (!opt->fresh && storage_exists(opt->data_dir)) {
        StorageReport report;
        TkStatus st = storage_load(opt->data_dir, app->graph, app->registry, app->roster, &report);
        if (st != TK_OK) {
            return st;
        }
        if (report.speed > 0.0 && report.speed <= SIMULATOR_MAX_SPEED) {
            *speed = report.speed;
        }
        fprintf(stderr, "Loaded %zu zones, %zu paths, %zu volunteers, %zu trees from %s/",
                report.zones, report.paths, report.volunteers, report.trees, opt->data_dir);
        if (report.skipped_rows > 0) {
            fprintf(stderr, " (%zu invalid row(s) skipped)", report.skipped_rows);
        }
        fputc('\n', stderr);
        return TK_OK;
    }
    return seed_demo_data(app);
}

int main(int argc, char **argv)
{
    Options opt;
    int parsed = parse_options(argc, argv, &opt);
    if (parsed != 0) {
        return parsed > 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    ui_init(opt.no_color);
    install_signal_handlers();

    AppContext app;
    memset(&app, 0, sizeof app);
    app.data_dir = opt.data_dir;
    app.interactive = isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
    app.graph = graph_create();
    app.registry = registry_create();
    app.roster = roster_create();
    app.output = strbuf_create();
    if (app.graph == NULL || app.registry == NULL || app.roster == NULL || app.output == NULL) {
        fprintf(stderr, "fatal: %s\n", tk_strerror(TK_ERR_NOMEM));
        app_destroy(&app);
        return EXIT_FAILURE;
    }

    double speed = SIMULATOR_DEFAULT_SPEED;
    TkStatus st = load_initial_state(&app, &opt, &speed);
    if (st != TK_OK) {
        /* Do NOT save on this path: we would overwrite good data on disk. */
        fprintf(stderr, "fatal: could not load state from '%s': %s\n", opt.data_dir, tk_strerror(st));
        app_destroy(&app);
        return EXIT_FAILURE;
    }
    if (opt.speed > 0.0) {
        speed = opt.speed;
    }

    app.sim = simulator_create(app.registry, speed);
    if (app.sim == NULL || simulator_start(app.sim) != TK_OK) {
        fprintf(stderr, "fatal: could not start simulation thread\n");
        app_destroy(&app);
        return EXIT_FAILURE;
    }

    cli_run(&app);

    simulator_stop(app.sim);
    int exit_code = EXIT_SUCCESS;
    if (!opt.no_save) {
        st = storage_save(opt.data_dir, app.graph, app.registry, app.roster, simulator_speed(app.sim));
        if (st != TK_OK) {
            fprintf(stderr, "error: could not save state to '%s': %s\n", opt.data_dir, tk_strerror(st));
            exit_code = EXIT_FAILURE;
        } else {
            fprintf(stderr, "State saved to %s/. Goodbye!\n", opt.data_dir);
        }
    }
    app_destroy(&app);
    return exit_code;
}
