/**
 * @file ui.c
 * @brief ANSI escape helpers and the dashboard layout.
 */
#include "ui.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define DASH_QUEUE_ROWS 8
#define DASH_TASK_ROWS  5
#define DASH_LEADERS    3
#define DASH_WIDTH      78

static bool g_ansi = false;

static const char *const STYLE_CODES[UI_STYLE_COUNT] = {
    [UI_RESET]   = "\033[0m",
    [UI_BOLD]    = "\033[1m",
    [UI_DIM]     = "\033[2m",
    [UI_RED]     = "\033[31m",
    [UI_GREEN]   = "\033[32m",
    [UI_YELLOW]  = "\033[33m",
    [UI_BLUE]    = "\033[34m",
    [UI_MAGENTA] = "\033[35m",
    [UI_CYAN]    = "\033[36m",
    [UI_INVERSE] = "\033[7m",
};

void ui_init(bool force_no_color)
{
    const char *no_color = getenv("NO_COLOR");
    g_ansi = !force_no_color && isatty(STDOUT_FILENO) &&
             (no_color == NULL || no_color[0] == '\0');
}

bool ui_ansi_enabled(void)
{
    return g_ansi;
}

const char *ui_style(UiStyle style)
{
    if (!g_ansi || (unsigned)style >= UI_STYLE_COUNT) {
        return "";
    }
    return STYLE_CODES[style];
}

UiStyle ui_status_style(TreeStatus status)
{
    switch (status) {
    case TREE_HEALTHY:  return UI_GREEN;
    case TREE_THIRSTY:  return UI_YELLOW;
    case TREE_CRITICAL: return UI_RED;
    case TREE_WILTED:   return UI_RED;
    case TREE_DISEASED: return UI_MAGENTA;
    default:            return UI_RESET;
    }
}

void ui_water_bar(StrBuf *sb, double percent)
{
    /* Unicode blocks on a terminal, plain ASCII when piped to a file. */
    const char *full = g_ansi ? "█" : "#";
    const char *empty = g_ansi ? "░" : ".";
    int cells = (int)lround(percent / 10.0);
    if (cells < 0) cells = 0;
    if (cells > 10) cells = 10;
    for (int i = 0; i < 10; i++) {
        strbuf_append(sb, i < cells ? full : empty);
    }
}

void ui_format_clock(char *buf, size_t cap, double hours)
{
    if (buf == NULL || cap == 0) {
        return;
    }
    long total_minutes = (long)floor(hours * 60.0);
    long day = total_minutes / (24 * 60) + 1;
    long hour = (total_minutes / 60) % 24;
    long minute = total_minutes % 60;
    snprintf(buf, cap, "Day %ld %02ld:%02ld", day, hour, minute);
}

void ui_clear_screen(StrBuf *sb)
{
    if (g_ansi) {
        strbuf_append(sb, "\033[H\033[2J");
    }
}

void ui_cursor_visible(bool visible)
{
    if (g_ansi) {
        fputs(visible ? "\033[?25h" : "\033[?25l", stdout);
        fflush(stdout);
    }
}

/* ---- Dashboard sections ---- */

static void rule(StrBuf *sb)
{
    strbuf_append(sb, ui_style(UI_DIM));
    for (int i = 0; i < DASH_WIDTH; i++) {
        strbuf_append(sb, g_ansi ? "─" : "-");
    }
    strbuf_printf(sb, "%s\n", ui_style(UI_RESET));
}

static void section_title(StrBuf *sb, const char *title, const char *subtitle)
{
    strbuf_printf(sb, " %s%s%s%s %s%s%s\n", ui_style(UI_BOLD), ui_style(UI_CYAN), title,
                  ui_style(UI_RESET), ui_style(UI_DIM), subtitle, ui_style(UI_RESET));
}

static void render_header(AppContext *app, StrBuf *sb)
{
    char clock[32];
    ui_format_clock(clock, sizeof clock, registry_clock(app->registry));
    bool paused = simulator_is_paused(app->sim);

    strbuf_printf(sb, "%s%s  TREEKEEPER  %s %s%-48s%s %s%s%s\n",
                  ui_style(UI_INVERSE), ui_style(UI_GREEN), ui_style(UI_RESET),
                  ui_style(UI_BOLD), "Afforestation & Volunteer Scheduling",
                  ui_style(UI_RESET), ui_style(UI_BOLD), clock, ui_style(UI_RESET));

    strbuf_printf(sb, " Simulation %s%s%s  x%.2f h/s   Zones %zu   Paths %zu   Volunteers %zu\n",
                  ui_style(paused ? UI_YELLOW : UI_GREEN),
                  paused ? "PAUSED " : "RUNNING", ui_style(UI_RESET),
                  simulator_speed(app->sim), graph_zone_count(app->graph),
                  graph_path_count(app->graph), roster_count(app->roster));
}

static void render_health(const RegistryStats *stats, StrBuf *sb)
{
    strbuf_printf(sb, " %sTrees %zu%s  ", ui_style(UI_BOLD), stats->total, ui_style(UI_RESET));
    for (int s = 0; s < TREE_STATUS_COUNT; s++) {
        strbuf_printf(sb, " %s%s %zu%s", ui_style(ui_status_style((TreeStatus)s)),
                      tree_status_name((TreeStatus)s), stats->by_status[s], ui_style(UI_RESET));
    }
    strbuf_printf(sb, "\n Queue    %zu waiting, %zu in progress\n", stats->queued, stats->assigned);

    size_t danger = stats->by_status[TREE_CRITICAL] + stats->by_status[TREE_WILTED];
    if (danger > 0) {
        strbuf_printf(sb, " %s%s! ALERT: %zu tree(s) critical or wilted - run 'assign <volunteer>'%s\n",
                      ui_style(UI_BOLD), ui_style(UI_RED), danger, ui_style(UI_RESET));
    }
}

/* One table row; shared by the queue and the in-progress list. */
static void render_tree_row(AppContext *app, StrBuf *sb, const TreeInfo *t, size_t rank)
{
    const char *zone = graph_zone_name(app->graph, t->zone_id);
    if (rank > 0) {
        strbuf_printf(sb, " %2zu ", rank);
    } else {
        strbuf_append(sb, "    ");
    }
    strbuf_printf(sb, "%5u  %-9s %-21.21s ", (unsigned)t->id, species_name(t->species),
                  zone != NULL ? zone : "?");
    strbuf_append(sb, ui_style(ui_status_style(t->status)));
    ui_water_bar(sb, t->water_level);
    strbuf_printf(sb, " %5.1f%%%s", t->water_level, ui_style(UI_RESET));

    if (t->hours_to_dry > 0.0) {
        strbuf_printf(sb, " %6.1fh", t->hours_to_dry);
    } else {
        strbuf_printf(sb, " %7s", "DRY");
    }
    strbuf_printf(sb, "  %s%-8s%s", ui_style(ui_status_style(t->status)),
                  tree_status_name(t->status), ui_style(UI_RESET));

    if (t->assigned_volunteer >= 0) {
        const char *who = roster_name(app->roster, t->assigned_volunteer);
        strbuf_printf(sb, " -> %s", who != NULL ? who : "?");
    }
    strbuf_append(sb, "\n");
}

static void render_queue(AppContext *app, StrBuf *sb)
{
    section_title(sb, "URGENT WATERING QUEUE", "(min-heap, earliest dry-out first)");
    strbuf_printf(sb, "%s  #    ID  Species   Zone                  Water              Dry in  Status%s\n",
                  ui_style(UI_DIM), ui_style(UI_RESET));

    TreeInfo rows[DASH_QUEUE_ROWS];
    size_t n = 0;
    if (registry_most_urgent(app->registry, DASH_QUEUE_ROWS, rows, &n) != TK_OK || n == 0) {
        strbuf_append(sb, "    (queue empty - every tree is cared for)\n");
        return;
    }
    for (size_t i = 0; i < n; i++) {
        render_tree_row(app, sb, &rows[i], i + 1);
    }
}

static void render_tasks(AppContext *app, StrBuf *sb, size_t assigned)
{
    if (assigned == 0) {
        return;
    }
    rule(sb);
    section_title(sb, "TASKS IN PROGRESS", "(scan with 'water <id>' when done)");

    TreeInfo *all = NULL;
    size_t n = 0;
    if (registry_snapshot(app->registry, &all, &n) != TK_OK) {
        return;
    }
    size_t shown = 0;
    for (size_t i = 0; i < n && shown < DASH_TASK_ROWS; i++) {
        if (all[i].assigned_volunteer >= 0) {
            render_tree_row(app, sb, &all[i], 0);
            shown++;
        }
    }
    if (assigned > shown) {
        strbuf_printf(sb, "    ... and %zu more\n", assigned - shown);
    }
    free(all);
}

static void render_leaderboard(AppContext *app, StrBuf *sb)
{
    VolunteerInfo top[DASH_LEADERS];
    size_t n = roster_leaderboard(app->roster, DASH_LEADERS, top);
    strbuf_printf(sb, " %s%sLEADERBOARD%s ", ui_style(UI_BOLD), ui_style(UI_CYAN), ui_style(UI_RESET));
    if (n == 0) {
        strbuf_append(sb, " (no volunteers yet)");
    }
    for (size_t i = 0; i < n; i++) {
        strbuf_printf(sb, "  %zu. %s%s%s (%u)", i + 1, ui_style(UI_BOLD), top[i].name,
                      ui_style(UI_RESET), top[i].tasks_completed);
    }
    strbuf_append(sb, "\n");
}

void ui_render_dashboard(AppContext *app, StrBuf *sb)
{
    if (app == NULL || sb == NULL) {
        return;
    }
    RegistryStats stats;
    registry_stats(app->registry, &stats);

    render_header(app, sb);
    rule(sb);
    render_health(&stats, sb);
    rule(sb);
    render_queue(app, sb);
    render_tasks(app, sb, stats.assigned);
    rule(sb);
    render_leaderboard(app, sb);
    rule(sb);
}
