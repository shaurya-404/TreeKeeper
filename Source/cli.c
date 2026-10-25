/**
 * @file cli.c
 * @brief Command table, argument validation and command handlers.
 *
 * Every handler writes its result into app->output instead of printing
 * directly. The main loop then decides how to present it: below the
 * dashboard on a terminal, or as plain text when input is piped.
 */
#include "cli.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/select.h>
#include <unistd.h>

#include "storage.h"
#include "ui.h"

#define CLI_MAX_ARGS        8
#define CLI_DEFAULT_ASSIGN  3
#define CLI_MAX_ASSIGN      50
#define CLI_DEFAULT_URGENT  10
#define WALK_METERS_PER_MIN 84.0 /* ~1.4 m/s average walking pace */

volatile sig_atomic_t g_cli_stop_requested = 0;

typedef void (*CommandFn)(AppContext *app, int argc, char **argv);

typedef struct {
    const char *name;
    const char *alias;
    const char *usage;
    const char *summary;
    int         min_args; /* excluding the command word */
    int         max_args;
    CommandFn   run;
} Command;

/* ======================================================================
 *  Output helpers
 * ==================================================================== */

static void emit(AppContext *app, UiStyle style, const char *tag,
                 const char *fmt, va_list args) TK_PRINTF(4, 0);
static void emit(AppContext *app, UiStyle style, const char *tag,
                 const char *fmt, va_list args)
{
    char message[512];
    vsnprintf(message, sizeof message, fmt, args);
    strbuf_printf(app->output, "%s%s%s %s\n", ui_style(style), tag, ui_style(UI_RESET), message);
}

static void say_ok(AppContext *app, const char *fmt, ...) TK_PRINTF(2, 3);
static void say_ok(AppContext *app, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    emit(app, UI_GREEN, ui_ansi_enabled() ? "✔" : "[ok]", fmt, args);
    va_end(args);
}

static void say_err(AppContext *app, const char *fmt, ...) TK_PRINTF(2, 3);
static void say_err(AppContext *app, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    emit(app, UI_RED, ui_ansi_enabled() ? "✘" : "[error]", fmt, args);
    va_end(args);
}

static void say_warn(AppContext *app, const char *fmt, ...) TK_PRINTF(2, 3);
static void say_warn(AppContext *app, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    emit(app, UI_YELLOW, "!", fmt, args);
    va_end(args);
}

/* ======================================================================
 *  Argument resolution
 * ==================================================================== */

static bool parse_tree_id(AppContext *app, const char *text, uint32_t *out)
{
    long id = 0;
    if (tk_parse_long(text, 1, (long)UINT32_MAX - 1, &id) != TK_OK) {
        say_err(app, "'%s' is not a valid tree id", text);
        return false;
    }
    *out = (uint32_t)id;
    return true;
}

/* Accepts a zone name (case-insensitive) or its numeric id. */
static bool resolve_zone(AppContext *app, const char *text, int *out)
{
    int id = graph_find_zone(app->graph, text);
    long numeric = 0;
    if (id < 0 && tk_parse_long(text, 0, INT32_MAX, &numeric) == TK_OK &&
        graph_zone_valid(app->graph, (int)numeric)) {
        id = (int)numeric;
    }
    if (id < 0) {
        say_err(app, "unknown zone '%s' (see 'zones')", text);
        return false;
    }
    *out = id;
    return true;
}

/* Accepts a volunteer name (case-insensitive) or numeric id. */
static bool resolve_volunteer(AppContext *app, const char *text, VolunteerInfo *out)
{
    long numeric = 0;
    if (tk_parse_long(text, 1, INT32_MAX, &numeric) == TK_OK &&
        roster_get(app->roster, (int)numeric, out) == TK_OK) {
        return true;
    }
    for (size_t i = 0; i < roster_count(app->roster); i++) {
        if (roster_at(app->roster, i, out) == TK_OK && strcasecmp(out->name, text) == 0) {
            return true;
        }
    }
    say_err(app, "unknown volunteer '%s' (see 'volunteers')", text);
    return false;
}

static const char *zone_label(const AppContext *app, int zone_id)
{
    const char *name = graph_zone_name(app->graph, zone_id);
    return name != NULL ? name : "?";
}

/* Appends "A -> B -> [C]" (stops in brackets) and the distance summary. */
static void print_path(AppContext *app, const Path *path)
{
    strbuf_append(app->output, "   ");
    for (size_t i = 0; i < path_length(path); i++) {
        const char *name = zone_label(app, path_zone_at(path, i));
        if (i > 0) {
            strbuf_printf(app->output, " %s->%s ", ui_style(UI_DIM), ui_style(UI_RESET));
        }
        if (path_is_stop(path, i)) {
            strbuf_printf(app->output, "%s[%s]%s", ui_style(UI_BOLD), name, ui_style(UI_RESET));
        } else {
            strbuf_append(app->output, name);
        }
    }
    double meters = path_distance(path);
    strbuf_printf(app->output, "\n   Total: %.0f m  (~%.0f min walk, %zu zones)\n",
                  meters, meters / WALK_METERS_PER_MIN, path_length(path));
}

/* ======================================================================
 *  Command handlers
 * ==================================================================== */

static void cmd_help(AppContext *app, int argc, char **argv);

static void cmd_dash(AppContext *app, int argc, char **argv)
{
    (void)argc;
    (void)argv;
    /* On a terminal the dashboard is redrawn anyway; when piped, print it. */
    if (!app->interactive) {
        ui_render_dashboard(app, app->output);
    }
}

static void cmd_monitor(AppContext *app, int argc, char **argv)
{
    (void)argc;
    (void)argv;
    if (!app->interactive) {
        ui_render_dashboard(app, app->output);
        return;
    }

    StrBuf *frame = strbuf_create();
    if (frame == NULL) {
        say_err(app, "%s", tk_strerror(TK_ERR_NOMEM));
        return;
    }
    ui_cursor_visible(false);
    while (!g_cli_stop_requested) {
        strbuf_clear(frame);
        ui_clear_screen(frame);
        ui_render_dashboard(app, frame);
        strbuf_printf(frame, " %s%sLIVE%s refreshing every second - press %sEnter%s to return\n",
                      ui_style(UI_INVERSE), ui_style(UI_RED), ui_style(UI_RESET),
                      ui_style(UI_BOLD), ui_style(UI_RESET));
        fwrite(strbuf_cstr(frame), 1, strbuf_len(frame), stdout);
        fflush(stdout);

        /* Wait up to 1 s for a key press without blocking the refresh. */
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        struct timeval timeout = { .tv_sec = 1, .tv_usec = 0 };
        int ready = select(STDIN_FILENO + 1, &fds, NULL, NULL, &timeout);
        if (ready > 0) {
            char *discard = NULL;
            size_t cap = 0;
            if (getline(&discard, &cap, stdin) < 0) {
                clearerr(stdin);
            }
            free(discard);
            break;
        }
        if (ready < 0 && errno != EINTR) {
            break;
        }
    }
    ui_cursor_visible(true);
    strbuf_destroy(frame);
}

static void cmd_plant(AppContext *app, int argc, char **argv)
{
    Species species;
    if (species_parse(argv[1], &species) != TK_OK) {
        say_err(app, "unknown species '%s'. Choose one of:", argv[1]);
        for (int i = 0; i < SPECIES_COUNT; i++) {
            strbuf_printf(app->output, "   %-9s (loses %.1f%%/h)\n",
                          species_name((Species)i), species_base_decay((Species)i));
        }
        return;
    }
    int zone = -1;
    if (!resolve_zone(app, argv[2], &zone)) {
        return;
    }
    double water = TREE_WATER_FULL;
    if (argc > 3 && tk_parse_double(argv[3], 0.0, TREE_WATER_FULL, &water) != TK_OK) {
        say_err(app, "water level must be a number between 0 and 100");
        return;
    }

    uint32_t id = 0;
    TkStatus st = registry_plant(app->registry, species, zone, water, &id);
    if (st != TK_OK) {
        say_err(app, "could not plant tree: %s", tk_strerror(st));
        return;
    }
    say_ok(app, "Planted %s #%u in %s at %.0f%% water. Print QR code: TK-%u",
           species_name(species), (unsigned)id, zone_label(app, zone), water, (unsigned)id);
}

static void cmd_water(AppContext *app, int argc, char **argv)
{
    uint32_t id = 0;
    if (!parse_tree_id(app, argv[1], &id)) {
        return;
    }
    VolunteerInfo scanner;
    bool explicit_volunteer = argc > 2;
    if (explicit_volunteer && !resolve_volunteer(app, argv[2], &scanner)) {
        return;
    }

    int previous = -1;
    TkStatus st = registry_water(app->registry, id, &previous);
    if (st != TK_OK) {
        say_err(app, "tree #%u: %s", (unsigned)id, tk_strerror(st));
        return;
    }

    int credited = explicit_volunteer ? scanner.id : previous;
    if (credited >= 0 && roster_credit(app->roster, credited) == TK_OK) {
        say_ok(app, "Tree #%u watered to 100%%. Task credited to %s.",
               (unsigned)id, roster_name(app->roster, credited));
    } else {
        say_ok(app, "Tree #%u watered to 100%% and re-queued.", (unsigned)id);
    }
}

static void cmd_status(AppContext *app, int argc, char **argv)
{
    (void)argc;
    uint32_t id = 0;
    if (!parse_tree_id(app, argv[1], &id)) {
        return;
    }
    bool diseased;
    if (strcasecmp(argv[2], "diseased") == 0) {
        diseased = true;
    } else if (strcasecmp(argv[2], "healthy") == 0 || strcasecmp(argv[2], "cured") == 0) {
        diseased = false;
    } else {
        say_err(app, "status must be 'diseased' or 'healthy'");
        return;
    }
    TkStatus st = registry_set_diseased(app->registry, id, diseased);
    if (st != TK_OK) {
        say_err(app, "tree #%u: %s", (unsigned)id, tk_strerror(st));
        return;
    }
    say_ok(app, "Tree #%u marked %s.", (unsigned)id, diseased ? "DISEASED" : "disease-free");
}

static void cmd_remove(AppContext *app, int argc, char **argv)
{
    (void)argc;
    uint32_t id = 0;
    if (!parse_tree_id(app, argv[1], &id)) {
        return;
    }
    TkStatus st = registry_remove(app->registry, id);
    if (st != TK_OK) {
        say_err(app, "tree #%u: %s", (unsigned)id, tk_strerror(st));
        return;
    }
    say_ok(app, "Tree #%u removed from the registry.", (unsigned)id);
}

static void cmd_info(AppContext *app, int argc, char **argv)
{
    (void)argc;
    uint32_t id = 0;
    TreeInfo t;
    if (!parse_tree_id(app, argv[1], &id)) {
        return;
    }
    if (registry_get(app->registry, id, &t) != TK_OK) {
        say_err(app, "no tree with id %u", (unsigned)id);
        return;
    }

    char planted[32], watered[32];
    ui_format_clock(planted, sizeof planted, t.planted_at);
    ui_format_clock(watered, sizeof watered, t.last_watered);
    const char *volunteer = roster_name(app->roster, t.assigned_volunteer);

    StrBuf *out = app->output;
    strbuf_printf(out, " %sTree #%u%s  (QR TK-%u)\n", ui_style(UI_BOLD), (unsigned)t.id,
                  ui_style(UI_RESET), (unsigned)t.id);
    strbuf_printf(out, "   Species      %s\n", species_name(t.species));
    strbuf_printf(out, "   Zone         %s\n", zone_label(app, t.zone_id));
    strbuf_printf(out, "   Status       %s%s%s\n", ui_style(ui_status_style(t.status)),
                  tree_status_name(t.status), ui_style(UI_RESET));
    strbuf_append(out, "   Water        ");
    strbuf_append(out, ui_style(ui_status_style(t.status)));
    ui_water_bar(out, t.water_level);
    strbuf_printf(out, " %.1f%%%s\n", t.water_level, ui_style(UI_RESET));
    strbuf_printf(out, "   Decay rate   %.2f%% per hour\n", t.decay_rate);
    strbuf_printf(out, "   Dries out in %.1f hours\n", t.hours_to_dry);
    strbuf_printf(out, "   Planted      %s\n", planted);
    strbuf_printf(out, "   Last watered %s\n", watered);
    strbuf_printf(out, "   Assigned to  %s\n", volunteer != NULL ? volunteer : "-");
}

static int compare_urgency(const void *a, const void *b)
{
    double da = ((const TreeInfo *)a)->hours_to_dry;
    double db = ((const TreeInfo *)b)->hours_to_dry;
    return (da > db) - (da < db);
}

static void cmd_list(AppContext *app, int argc, char **argv)
{
    int zone_filter = -1;
    if (argc > 1 && !resolve_zone(app, argv[1], &zone_filter)) {
        return;
    }
    TreeInfo *trees = NULL;
    size_t n = 0;
    TkStatus st = registry_snapshot(app->registry, &trees, &n);
    if (st != TK_OK) {
        say_err(app, "%s", tk_strerror(st));
        return;
    }
    if (n > 1) {
        qsort(trees, n, sizeof *trees, compare_urgency);
    }

    strbuf_printf(app->output, "%s     ID  Species   Zone                  Water              Dry in  Status%s\n",
                  ui_style(UI_DIM), ui_style(UI_RESET));
    size_t shown = 0;
    for (size_t i = 0; i < n; i++) {
        const TreeInfo *t = &trees[i];
        if (zone_filter >= 0 && t->zone_id != zone_filter) {
            continue;
        }
        strbuf_printf(app->output, "  %5u  %-9s %-21.21s ", (unsigned)t->id,
                      species_name(t->species), zone_label(app, t->zone_id));
        strbuf_append(app->output, ui_style(ui_status_style(t->status)));
        ui_water_bar(app->output, t->water_level);
        strbuf_printf(app->output, " %5.1f%%%s %6.1fh  %s%s%s\n", t->water_level,
                      ui_style(UI_RESET), t->hours_to_dry, ui_style(ui_status_style(t->status)),
                      tree_status_name(t->status), ui_style(UI_RESET));
        shown++;
    }
    strbuf_printf(app->output, "  %zu tree(s)\n", shown);
    free(trees);
}

static void cmd_urgent(AppContext *app, int argc, char **argv)
{
    long k = CLI_DEFAULT_URGENT;
    if (argc > 1 && tk_parse_long(argv[1], 1, 1000, &k) != TK_OK) {
        say_err(app, "k must be between 1 and 1000");
        return;
    }
    TreeInfo *rows = tk_reallocarray(NULL, (size_t)k, sizeof *rows);
    if (rows == NULL) {
        say_err(app, "%s", tk_strerror(TK_ERR_NOMEM));
        return;
    }
    size_t n = 0;
    TkStatus st = registry_most_urgent(app->registry, (size_t)k, rows, &n);
    if (st != TK_OK) {
        say_err(app, "%s", tk_strerror(st));
    } else if (n == 0) {
        say_ok(app, "The watering queue is empty.");
    } else {
        strbuf_printf(app->output, " Top %zu of the min-heap (peeked, not removed):\n", n);
        for (size_t i = 0; i < n; i++) {
            strbuf_printf(app->output, "  %2zu. #%u %-9s %-21s %5.1f%%  dry in %.1fh\n", i + 1,
                          (unsigned)rows[i].id, species_name(rows[i].species),
                          zone_label(app, rows[i].zone_id), rows[i].water_level,
                          rows[i].hours_to_dry);
        }
    }
    free(rows);
}

static void cmd_zones(AppContext *app, int argc, char **argv)
{
    (void)argc;
    (void)argv;
    size_t n = graph_zone_count(app->graph);
    strbuf_printf(app->output, " %zu zone(s):\n", n);
    for (size_t i = 0; i < n; i++) {
        strbuf_printf(app->output, "  %3zu  %-22s %zu path(s)\n", i,
                      graph_zone_name(app->graph, (int)i), graph_degree(app->graph, (int)i));
    }
}

static void cmd_add_zone(AppContext *app, int argc, char **argv)
{
    (void)argc;
    int id = -1;
    TkStatus st = graph_add_zone(app->graph, argv[1], &id);
    if (st == TK_ERR_INVALID) {
        say_err(app, "zone names are 1-%d characters of letters, digits, '_', '-' or '.'",
                TK_NAME_MAX - 1);
    } else if (st != TK_OK) {
        say_err(app, "zone '%s': %s", argv[1], tk_strerror(st));
    } else {
        say_ok(app, "Zone '%s' added with id %d. Connect it with 'path'.", argv[1], id);
    }
}

static void cmd_path(AppContext *app, int argc, char **argv)
{
    (void)argc;
    int a = -1, b = -1;
    double meters = 0.0;
    if (!resolve_zone(app, argv[1], &a) || !resolve_zone(app, argv[2], &b)) {
        return;
    }
    if (tk_parse_double(argv[3], 0.01, 1e7, &meters) != TK_OK) {
        say_err(app, "distance must be a positive number of metres");
        return;
    }
    TkStatus st = graph_add_path(app->graph, a, b, meters);
    if (st != TK_OK) {
        say_err(app, "path: %s", a == b ? "a path needs two different zones" : tk_strerror(st));
        return;
    }
    say_ok(app, "Path %s <-> %s set to %.0f m.", zone_label(app, a), zone_label(app, b), meters);
}

static void cmd_unpath(AppContext *app, int argc, char **argv)
{
    (void)argc;
    int a = -1, b = -1;
    if (!resolve_zone(app, argv[1], &a) || !resolve_zone(app, argv[2], &b)) {
        return;
    }
    TkStatus st = graph_remove_path(app->graph, a, b);
    if (st != TK_OK) {
        say_err(app, "no path between %s and %s", zone_label(app, a), zone_label(app, b));
        return;
    }
    say_ok(app, "Path %s <-> %s removed.", zone_label(app, a), zone_label(app, b));
}

typedef struct {
    AppContext *app;
    bool        first;
} NeighborCtx;

static void print_neighbor(int from, int to, double meters, void *ctx)
{
    (void)from;
    NeighborCtx *nc = ctx;
    strbuf_printf(nc->app->output, "%s%s (%.0fm)", nc->first ? "" : ", ",
                  zone_label(nc->app, to), meters);
    nc->first = false;
}

static void cmd_map(AppContext *app, int argc, char **argv)
{
    (void)argc;
    (void)argv;
    size_t n = graph_zone_count(app->graph);
    strbuf_printf(app->output, " Adjacency list (%zu zones, %zu paths):\n", n,
                  graph_path_count(app->graph));
    for (size_t i = 0; i < n; i++) {
        strbuf_printf(app->output, "  %s%-22s%s -> ", ui_style(UI_BOLD),
                      graph_zone_name(app->graph, (int)i), ui_style(UI_RESET));
        NeighborCtx ctx = { .app = app, .first = true };
        graph_foreach_neighbor(app->graph, (int)i, print_neighbor, &ctx);
        strbuf_append(app->output, ctx.first ? "(isolated)\n" : "\n");
    }
}

static void cmd_route(AppContext *app, int argc, char **argv)
{
    (void)argc;
    int src = -1, dst = -1;
    if (!resolve_zone(app, argv[1], &src) || !resolve_zone(app, argv[2], &dst)) {
        return;
    }
    Path *path = NULL;
    TkStatus st = graph_shortest_path(app->graph, src, dst, &path);
    if (st == TK_ERR_UNREACHABLE) {
        say_err(app, "%s cannot be reached from %s (no connecting paths)",
                zone_label(app, dst), zone_label(app, src));
        return;
    }
    if (st != TK_OK) {
        say_err(app, "route: %s", tk_strerror(st));
        return;
    }
    say_ok(app, "Shortest path (Dijkstra) from %s to %s:", zone_label(app, src), zone_label(app, dst));
    print_path(app, path);
    path_destroy(path);
}

static void cmd_add_volunteer(AppContext *app, int argc, char **argv)
{
    (void)argc;
    int zone = -1, id = -1;
    if (!resolve_zone(app, argv[2], &zone)) {
        return;
    }
    TkStatus st = roster_add(app->roster, argv[1], zone, &id);
    if (st == TK_ERR_INVALID) {
        say_err(app, "volunteer names are 1-%d characters of letters, digits, '_', '-' or '.'",
                TK_NAME_MAX - 1);
    } else if (st != TK_OK) {
        say_err(app, "volunteer '%s': %s", argv[1], tk_strerror(st));
    } else {
        say_ok(app, "Volunteer %s registered with id %d, based at %s.", argv[1], id,
               zone_label(app, zone));
    }
}

static void cmd_volunteers(AppContext *app, int argc, char **argv)
{
    (void)argc;
    (void)argv;
    size_t n = roster_count(app->roster);
    VolunteerInfo *board = tk_reallocarray(NULL, n > 0 ? n : 1, sizeof *board);
    if (board == NULL) {
        say_err(app, "%s", tk_strerror(TK_ERR_NOMEM));
        return;
    }
    n = roster_leaderboard(app->roster, n, board);
    strbuf_printf(app->output, " %zu volunteer(s), ranked by trees watered:\n", n);
    for (size_t i = 0; i < n; i++) {
        strbuf_printf(app->output, "  %2zu. [id %d] %-16s base: %-22s %u task(s)\n", i + 1,
                      board[i].id, board[i].name, zone_label(app, board[i].zone_id),
                      board[i].tasks_completed);
    }
    free(board);
}

static void cmd_assign(AppContext *app, int argc, char **argv)
{
    VolunteerInfo vol;
    if (!resolve_volunteer(app, argv[1], &vol)) {
        return;
    }
    long k = CLI_DEFAULT_ASSIGN;
    if (argc > 2 && tk_parse_long(argv[2], 1, CLI_MAX_ASSIGN, &k) != TK_OK) {
        say_err(app, "number of trees must be between 1 and %d", CLI_MAX_ASSIGN);
        return;
    }

    int stops[CLI_MAX_ASSIGN];
    size_t taken = 0;
    TreeInfo t;
    /* Extract-min k times: the k trees that will dry out soonest. */
    while (taken < (size_t)k && registry_assign_next(app->registry, vol.id, &t) == TK_OK) {
        if (taken == 0) {
            say_ok(app, "Assigned to %s (starting at %s):", vol.name, zone_label(app, vol.zone_id));
        }
        strbuf_printf(app->output, "   #%u %-9s in %-21s %5.1f%% water, dry in %.1fh\n",
                      (unsigned)t.id, species_name(t.species), zone_label(app, t.zone_id),
                      t.water_level, t.hours_to_dry);
        stops[taken++] = t.zone_id;
    }
    if (taken == 0) {
        say_warn(app, "No trees are waiting in the queue.");
        return;
    }

    Path *route = NULL;
    TkStatus st = graph_plan_route(app->graph, vol.zone_id, stops, taken, &route);
    if (st == TK_OK) {
        strbuf_append(app->output, " Suggested walking route (nearest stop first, [stop]):\n");
        print_path(app, route);
        path_destroy(route);
    } else {
        say_warn(app, "Could not plan a full route: %s. Check 'map' for missing paths.",
                 tk_strerror(st));
    }
}

static void cmd_release(AppContext *app, int argc, char **argv)
{
    (void)argc;
    uint32_t id = 0;
    if (!parse_tree_id(app, argv[1], &id)) {
        return;
    }
    TkStatus st = registry_release(app->registry, id);
    if (st == TK_ERR_INVALID) {
        say_err(app, "tree #%u is not assigned to anyone", (unsigned)id);
    } else if (st != TK_OK) {
        say_err(app, "tree #%u: %s", (unsigned)id, tk_strerror(st));
    } else {
        say_ok(app, "Tree #%u returned to the watering queue.", (unsigned)id);
    }
}

static void cmd_pause(AppContext *app, int argc, char **argv)
{
    (void)argc;
    (void)argv;
    simulator_set_paused(app->sim, true);
    say_ok(app, "Simulation paused.");
}

static void cmd_resume(AppContext *app, int argc, char **argv)
{
    (void)argc;
    (void)argv;
    simulator_set_paused(app->sim, false);
    say_ok(app, "Simulation resumed.");
}

static void cmd_speed(AppContext *app, int argc, char **argv)
{
    (void)argc;
    double speed = 0.0;
    if (tk_parse_double(argv[1], 0.001, SIMULATOR_MAX_SPEED, &speed) != TK_OK ||
        simulator_set_speed(app->sim, speed) != TK_OK) {
        say_err(app, "speed must be between 0.001 and %.0f simulated hours per second",
                SIMULATOR_MAX_SPEED);
        return;
    }
    say_ok(app, "1 real second now equals %.3g simulated hour(s).", speed);
}

static void cmd_tick(AppContext *app, int argc, char **argv)
{
    (void)argc;
    double hours = 0.0;
    if (tk_parse_double(argv[1], 0.01, 24.0 * 365.0, &hours) != TK_OK) {
        say_err(app, "hours must be between 0.01 and 8760");
        return;
    }
    registry_advance(app->registry, hours);
    say_ok(app, "Fast-forwarded %.2f simulated hour(s).", hours);
}

static void cmd_save(AppContext *app, int argc, char **argv)
{
    (void)argc;
    (void)argv;
    TkStatus st = storage_save(app->data_dir, app->graph, app->registry, app->roster,
                               simulator_speed(app->sim));
    if (st != TK_OK) {
        say_err(app, "save to '%s' failed: %s", app->data_dir, tk_strerror(st));
        return;
    }
    say_ok(app, "State saved to %s/", app->data_dir);
}

static void cmd_quit(AppContext *app, int argc, char **argv)
{
    (void)argc;
    (void)argv;
    app->quit = true;
}

static const Command COMMANDS[] = {
    { "help",       "?",    "help",                          "show this list",                                0, 0, cmd_help },
    { "dash",       NULL,   "dash",                          "redraw the dashboard",                          0, 0, cmd_dash },
    { "monitor",    "live", "monitor",                       "live auto-refreshing dashboard",                0, 0, cmd_monitor },
    { "plant",      NULL,   "plant <species> <zone> [water]", "log a new sapling",                            2, 3, cmd_plant },
    { "water",      "scan", "water <tree_id> [volunteer]",   "QR scan: tree watered, credit volunteer",       1, 2, cmd_water },
    { "status",     NULL,   "status <tree_id> diseased|healthy", "flag or clear disease",                     2, 2, cmd_status },
    { "info",       NULL,   "info <tree_id>",                "O(1) hash-map lookup of one tree",              1, 1, cmd_info },
    { "list",       NULL,   "list [zone]",                   "all trees, most urgent first",                  0, 1, cmd_list },
    { "urgent",     NULL,   "urgent [k]",                    "peek top-k of the min-heap",                    0, 1, cmd_urgent },
    { "remove",     NULL,   "remove <tree_id>",              "delete a tree",                                 1, 1, cmd_remove },
    { "zones",      NULL,   "zones",                         "list zones",                                    0, 0, cmd_zones },
    { "zone",       NULL,   "zone <name>",                   "add a zone (graph vertex)",                     1, 1, cmd_add_zone },
    { "path",       NULL,   "path <zone> <zone> <meters>",   "add/update a walkway (graph edge)",             3, 3, cmd_path },
    { "unpath",     NULL,   "unpath <zone> <zone>",          "remove a walkway",                              2, 2, cmd_unpath },
    { "map",        "graph", "map",                          "print the adjacency list",                      0, 0, cmd_map },
    { "route",      NULL,   "route <from> <to>",             "shortest walk (Dijkstra)",                      2, 2, cmd_route },
    { "volunteer",  NULL,   "volunteer <name> <zone>",       "register a volunteer",                          2, 2, cmd_add_volunteer },
    { "volunteers", NULL,   "volunteers",                    "roster and leaderboard",                        0, 0, cmd_volunteers },
    { "assign",     NULL,   "assign <volunteer> [k]",        "give k most urgent trees + walking route",      1, 2, cmd_assign },
    { "release",    NULL,   "release <tree_id>",             "return an assigned tree to the queue",          1, 1, cmd_release },
    { "pause",      NULL,   "pause",                         "pause the simulation clock",                    0, 0, cmd_pause },
    { "resume",     NULL,   "resume",                        "resume the simulation clock",                   0, 0, cmd_resume },
    { "speed",      NULL,   "speed <hours_per_sec>",         "set simulation speed",                          1, 1, cmd_speed },
    { "tick",       NULL,   "tick <hours>",                  "fast-forward the clock instantly",              1, 1, cmd_tick },
    { "save",       NULL,   "save",                          "write state to disk now",                       0, 0, cmd_save },
    { "quit",       "exit", "quit",                          "save and exit",                                 0, 0, cmd_quit },
};

static void cmd_help(AppContext *app, int argc, char **argv)
{
    (void)argc;
    (void)argv;
    strbuf_printf(app->output, " %sCommands%s (zones/volunteers accept a name or an id):\n",
                  ui_style(UI_BOLD), ui_style(UI_RESET));
    for (size_t i = 0; i < TK_ARRAY_LEN(COMMANDS); i++) {
        strbuf_printf(app->output, "  %s%-36s%s %s\n", ui_style(UI_CYAN), COMMANDS[i].usage,
                      ui_style(UI_RESET), COMMANDS[i].summary);
    }
}

/* ======================================================================
 *  Dispatcher and main loop
 * ==================================================================== */

static const Command *find_command(const char *word)
{
    for (size_t i = 0; i < TK_ARRAY_LEN(COMMANDS); i++) {
        const Command *c = &COMMANDS[i];
        if (strcasecmp(word, c->name) == 0 || (c->alias != NULL && strcasecmp(word, c->alias) == 0)) {
            return c;
        }
    }
    return NULL;
}

void cli_execute(AppContext *app, char *line)
{
    if (app == NULL || line == NULL) {
        return;
    }
    line += strspn(line, " \t");
    if (*line == '#') {
        return; /* comment line (used in test scripts) */
    }
    char *argv[CLI_MAX_ARGS + 1];
    int argc = 0;
    char *save = NULL;
    for (char *tok = strtok_r(line, " \t\r\n", &save); tok != NULL;
         tok = strtok_r(NULL, " \t\r\n", &save)) {
        if (argc == CLI_MAX_ARGS) {
            say_err(app, "too many arguments");
            return;
        }
        argv[argc++] = tok;
    }
    if (argc == 0) {
        return;
    }
    argv[argc] = NULL;

    const Command *cmd = find_command(argv[0]);
    if (cmd == NULL) {
        say_err(app, "unknown command '%s' - type 'help'", argv[0]);
        return;
    }
    int given = argc - 1;
    if (given < cmd->min_args || given > cmd->max_args) {
        say_err(app, "usage: %s", cmd->usage);
        return;
    }
    cmd->run(app, argc, argv);
}

/* Draws one screen: dashboard + last command output + prompt. */
static void present(AppContext *app)
{
    if (!app->interactive) {
        fputs(strbuf_cstr(app->output), stdout);
        fflush(stdout);
        return;
    }
    StrBuf *screen = strbuf_create();
    if (screen == NULL) {
        fputs(strbuf_cstr(app->output), stdout);
        return;
    }
    ui_clear_screen(screen);
    ui_render_dashboard(app, screen);
    strbuf_append(screen, strbuf_cstr(app->output));
    strbuf_printf(screen, "%s%streekeeper>%s ", ui_style(UI_BOLD), ui_style(UI_GREEN), ui_style(UI_RESET));
    fwrite(strbuf_cstr(screen), 1, strbuf_len(screen), stdout);
    fflush(stdout);
    strbuf_destroy(screen);
}

void cli_run(AppContext *app)
{
    if (app == NULL) {
        return;
    }
    char *line = NULL;
    size_t cap = 0;

    if (app->interactive) {
        strbuf_printf(app->output, " Type %shelp%s for commands, %smonitor%s for live view, %squit%s to save & exit.\n",
                      ui_style(UI_BOLD), ui_style(UI_RESET), ui_style(UI_BOLD), ui_style(UI_RESET),
                      ui_style(UI_BOLD), ui_style(UI_RESET));
    }

    while (!app->quit && !g_cli_stop_requested) {
        present(app);
        strbuf_clear(app->output);

        errno = 0;
        if (getline(&line, &cap, stdin) < 0) {
            break; /* EOF, read error, or interrupted by Ctrl+C */
        }
        if (!app->interactive) {
            /* Echo scripted commands so transcripts read like a session. */
            char *end = line + strcspn(line, "\r\n");
            if (end != line && line[0] != '#') {
                printf("treekeeper> %.*s\n", (int)(end - line), line);
            }
        }
        cli_execute(app, line);
    }
    if (strbuf_len(app->output) > 0) {
        fputs(strbuf_cstr(app->output), stdout);
    }
    if (app->interactive) {
        fputs("\n", stdout);
    }
    fflush(stdout);
    free(line);
}
