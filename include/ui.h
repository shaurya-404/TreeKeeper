/**
 * @file ui.h
 * @brief ANSI terminal helpers and the live dashboard renderer.
 *
 * Colours are emitted only when stdout is a terminal and the NO_COLOR
 * environment variable is unset (see https://no-color.org), so piping the
 * program's output into a file produces clean text.
 */
#ifndef UI_H
#define UI_H

#include <stdbool.h>

#include "app.h"
#include "registry.h"
#include "strbuf.h"

typedef enum {
    UI_RESET = 0,
    UI_BOLD,
    UI_DIM,
    UI_RED,
    UI_GREEN,
    UI_YELLOW,
    UI_BLUE,
    UI_MAGENTA,
    UI_CYAN,
    UI_INVERSE,
    UI_STYLE_COUNT
} UiStyle;

/** Decides whether colour / screen control may be used. Call once. */
void ui_init(bool force_no_color);

/** True if ANSI escape sequences are enabled. */
bool ui_ansi_enabled(void);

/** Escape sequence for @p style, or "" when colours are disabled. */
const char *ui_style(UiStyle style);

/** Colour associated with a tree status. */
UiStyle ui_status_style(TreeStatus status);

/** Appends a 10-cell water gauge such as "███████░░░". */
void ui_water_bar(StrBuf *sb, double percent);

/** Formats simulated hours as "Day 3 14:05". */
void ui_format_clock(char *buf, size_t cap, double hours);

/** Appends the full dashboard (header, health, queue, tasks, leaderboard). */
void ui_render_dashboard(AppContext *app, StrBuf *sb);

/** Screen control helpers (no-ops when ANSI is disabled). */
void ui_clear_screen(StrBuf *sb);
void ui_cursor_visible(bool visible);

#endif /* UI_H */
