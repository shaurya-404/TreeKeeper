/**
 * @file storage.c
 * @brief CSV save/load with atomic replace and row-level validation.
 */
#include "storage.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "hashmap.h"

#define FILE_META       "meta.csv"
#define FILE_ZONES      "zones.csv"
#define FILE_PATHS      "paths.csv"
#define FILE_VOLUNTEERS "volunteers.csv"
#define FILE_TREES      "trees.csv"

#define CSV_MAX_FIELDS 16

/* ======================================================================
 *  Path helpers
 * ==================================================================== */

static TkStatus join_path(char *buf, size_t cap, const char *dir,
                          const char *file, const char *suffix)
{
    int n = snprintf(buf, cap, "%s/%s%s", dir, file, suffix);
    return (n < 0 || (size_t)n >= cap) ? TK_ERR_INVALID : TK_OK;
}

static TkStatus ensure_directory(const char *dir)
{
    struct stat st;
    if (stat(dir, &st) == 0) {
        return S_ISDIR(st.st_mode) ? TK_OK : TK_ERR_IO;
    }
    if (mkdir(dir, 0755) != 0 && errno != EEXIST) {
        return TK_ERR_IO;
    }
    return TK_OK;
}

bool storage_exists(const char *dir)
{
    char path[TK_PATH_MAX];
    if (dir == NULL || join_path(path, sizeof path, dir, FILE_ZONES, "") != TK_OK) {
        return false;
    }
    return access(path, R_OK) == 0;
}

/* ======================================================================
 *  Saving
 * ==================================================================== */

typedef struct {
    const Graph  *graph;
    TreeRegistry *registry;
    const Roster *roster;
    double        speed;
} SaveCtx;

typedef TkStatus (*CsvWriter)(FILE *fp, const SaveCtx *ctx);

/* Writes one file via "<file>.tmp" + rename() so readers never see half a file. */
static TkStatus write_atomically(const char *dir, const char *file,
                                 CsvWriter writer, const SaveCtx *ctx)
{
    char final_path[TK_PATH_MAX];
    char tmp_path[TK_PATH_MAX];
    if (join_path(final_path, sizeof final_path, dir, file, "") != TK_OK ||
        join_path(tmp_path, sizeof tmp_path, dir, file, ".tmp") != TK_OK) {
        return TK_ERR_INVALID;
    }

    FILE *fp = fopen(tmp_path, "w");
    if (fp == NULL) {
        return TK_ERR_IO;
    }
    TkStatus st = writer(fp, ctx);
    if (fflush(fp) != 0 || ferror(fp)) {
        st = TK_ERR_IO;
    }
    if (fclose(fp) != 0 && st == TK_OK) {
        st = TK_ERR_IO;
    }
    if (st == TK_OK && rename(tmp_path, final_path) != 0) {
        st = TK_ERR_IO;
    }
    if (st != TK_OK) {
        remove(tmp_path);
    }
    return st;
}

static TkStatus write_meta(FILE *fp, const SaveCtx *ctx)
{
    fprintf(fp, "key,value\n");
    fprintf(fp, "version,%d\n", STORAGE_FORMAT_VERSION);
    fprintf(fp, "clock,%.6f\n", registry_clock(ctx->registry));
    fprintf(fp, "next_tree_id,%u\n", (unsigned)registry_next_id(ctx->registry));
    fprintf(fp, "speed,%.4f\n", ctx->speed);
    return TK_OK;
}

static TkStatus write_zones(FILE *fp, const SaveCtx *ctx)
{
    fprintf(fp, "id,name\n");
    size_t n = graph_zone_count(ctx->graph);
    for (size_t i = 0; i < n; i++) {
        fprintf(fp, "%zu,%s\n", i, graph_zone_name(ctx->graph, (int)i));
    }
    return TK_OK;
}

static void write_edge(int from, int to, double meters, void *ctx)
{
    fprintf((FILE *)ctx, "%d,%d,%.2f\n", from, to, meters);
}

static TkStatus write_paths(FILE *fp, const SaveCtx *ctx)
{
    fprintf(fp, "from,to,meters\n");
    graph_foreach_edge(ctx->graph, write_edge, fp);
    return TK_OK;
}

static TkStatus write_volunteers(FILE *fp, const SaveCtx *ctx)
{
    fprintf(fp, "id,name,zone,tasks_completed\n");
    size_t n = roster_count(ctx->roster);
    for (size_t i = 0; i < n; i++) {
        VolunteerInfo v;
        if (roster_at(ctx->roster, i, &v) == TK_OK) {
            fprintf(fp, "%d,%s,%d,%u\n", v.id, v.name, v.zone_id, v.tasks_completed);
        }
    }
    return TK_OK;
}

static TkStatus write_trees(FILE *fp, const SaveCtx *ctx)
{
    TreeInfo *trees = NULL;
    size_t n = 0;
    TkStatus st = registry_snapshot(ctx->registry, &trees, &n);
    if (st != TK_OK) {
        return st;
    }
    fprintf(fp, "id,species,zone,water,decay_rate,planted_at,last_watered,diseased,assigned_volunteer\n");
    for (size_t i = 0; i < n; i++) {
        const TreeInfo *t = &trees[i];
        fprintf(fp, "%u,%s,%d,%.4f,%.6f,%.4f,%.4f,%d,%d\n",
                (unsigned)t->id, species_name(t->species), t->zone_id,
                t->water_level, t->decay_rate, t->planted_at, t->last_watered,
                t->diseased ? 1 : 0, t->assigned_volunteer);
    }
    free(trees);
    return TK_OK;
}

TkStatus storage_save(const char *dir, const Graph *graph, TreeRegistry *reg,
                      const Roster *roster, double speed)
{
    if (dir == NULL || graph == NULL || reg == NULL || roster == NULL) {
        return TK_ERR_INVALID;
    }
    TkStatus st = ensure_directory(dir);
    if (st != TK_OK) {
        return st;
    }

    const SaveCtx ctx = { .graph = graph, .registry = reg, .roster = roster, .speed = speed };
    /* Meta goes last: its presence marks a complete save. */
    static const struct {
        const char *file;
        CsvWriter   writer;
    } steps[] = {
        { FILE_ZONES,      write_zones },
        { FILE_PATHS,      write_paths },
        { FILE_VOLUNTEERS, write_volunteers },
        { FILE_TREES,      write_trees },
        { FILE_META,       write_meta },
    };
    for (size_t i = 0; i < TK_ARRAY_LEN(steps) && st == TK_OK; i++) {
        st = write_atomically(dir, steps[i].file, steps[i].writer, &ctx);
    }
    return st;
}

/* ======================================================================
 *  Loading
 * ==================================================================== */

typedef struct {
    Graph         *graph;
    TreeRegistry  *registry;
    Roster        *roster;
    HashMap       *zone_map;  /* zone id in file -> zone id in graph */
    StorageReport *report;
    uint32_t       next_tree_id;
} LoadCtx;

/*
 * Row handler: TK_OK keeps the row, TK_ERR_PARSE / TK_ERR_INVALID /
 * TK_ERR_EXISTS / TK_ERR_NOT_FOUND mark it as skipped, anything else
 * (e.g. TK_ERR_NOMEM) aborts the load.
 */
typedef TkStatus (*CsvRowHandler)(char **fields, size_t count, LoadCtx *ctx);

static bool is_row_error(TkStatus st)
{
    return st == TK_ERR_PARSE || st == TK_ERR_INVALID ||
           st == TK_ERR_EXISTS || st == TK_ERR_NOT_FOUND;
}

/* Splits @p line in place on commas. Returns field count, or SIZE_MAX if too many. */
static size_t split_csv(char *line, char **fields, size_t max_fields)
{
    size_t count = 0;
    char *cursor = line;
    for (;;) {
        if (count == max_fields) {
            return SIZE_MAX;
        }
        fields[count++] = cursor;
        char *comma = strchr(cursor, ',');
        if (comma == NULL) {
            break;
        }
        *comma = '\0';
        cursor = comma + 1;
    }
    return count;
}

static void strip_newline(char *line)
{
    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
        line[--len] = '\0';
    }
}

static TkStatus read_csv(const char *dir, const char *file, bool required,
                         CsvRowHandler handler, LoadCtx *ctx)
{
    char path[TK_PATH_MAX];
    if (join_path(path, sizeof path, dir, file, "") != TK_OK) {
        return TK_ERR_INVALID;
    }
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        return (errno == ENOENT && !required) ? TK_OK : TK_ERR_IO;
    }

    char *line = NULL;
    size_t line_cap = 0;
    bool header_seen = false;
    TkStatus st = TK_OK;

    while (getline(&line, &line_cap, fp) != -1) {
        strip_newline(line);
        if (line[0] == '\0' || line[0] == '#') {
            continue; /* blank lines and comments are allowed */
        }
        if (!header_seen) {
            header_seen = true;
            continue;
        }
        char *fields[CSV_MAX_FIELDS];
        size_t count = split_csv(line, fields, CSV_MAX_FIELDS);
        TkStatus row = (count == SIZE_MAX) ? TK_ERR_PARSE : handler(fields, count, ctx);
        if (is_row_error(row)) {
            ctx->report->skipped_rows++;
        } else if (row != TK_OK) {
            st = row;
            break;
        }
    }
    if (st == TK_OK && ferror(fp)) {
        st = TK_ERR_IO;
    }
    free(line);
    fclose(fp);
    return st;
}

static TkStatus parse_int(const char *text, long min, long max, int *out)
{
    long value = 0;
    TkStatus st = tk_parse_long(text, min, max, &value);
    if (st == TK_OK) {
        *out = (int)value;
    }
    return st;
}

/* Translates a zone id as written in the file into the graph's id. */
static TkStatus map_zone(const LoadCtx *ctx, const char *text, int *out)
{
    int file_id = 0;
    TkStatus st = parse_int(text, 0, INT32_MAX, &file_id);
    if (st != TK_OK) {
        return st;
    }
    void *value = NULL;
    if (!hashmap_lookup(ctx->zone_map, (uint32_t)file_id, &value)) {
        return TK_ERR_NOT_FOUND;
    }
    *out = (int)(uintptr_t)value;
    return TK_OK;
}

static TkStatus load_meta_row(char **f, size_t n, LoadCtx *ctx)
{
    if (n != 2) {
        return TK_ERR_PARSE;
    }
    double value = 0.0;
    TkStatus st = tk_parse_double(f[1], 0.0, 1e12, &value);
    if (st != TK_OK) {
        return st;
    }
    if (strcmp(f[0], "version") == 0) {
        /* Refuse files written by a newer, incompatible version. */
        return (value > STORAGE_FORMAT_VERSION) ? TK_ERR_IO : TK_OK;
    }
    if (strcmp(f[0], "clock") == 0) {
        registry_set_clock(ctx->registry, value);
    } else if (strcmp(f[0], "next_tree_id") == 0 && value < (double)UINT32_MAX) {
        ctx->next_tree_id = (uint32_t)value;
    } else if (strcmp(f[0], "speed") == 0) {
        ctx->report->speed = value;
    }
    return TK_OK; /* unknown keys are ignored for forward compatibility */
}

static TkStatus load_zone_row(char **f, size_t n, LoadCtx *ctx)
{
    int file_id = 0;
    if (n != 2 || parse_int(f[0], 0, INT32_MAX, &file_id) != TK_OK) {
        return TK_ERR_PARSE;
    }
    if (hashmap_contains(ctx->zone_map, (uint32_t)file_id)) {
        return TK_ERR_EXISTS;
    }
    int graph_id = -1;
    TkStatus st = graph_add_zone(ctx->graph, f[1], &graph_id);
    if (st != TK_OK) {
        return st;
    }
    st = hashmap_put(ctx->zone_map, (uint32_t)file_id, (void *)(uintptr_t)graph_id);
    if (st == TK_OK) {
        ctx->report->zones++;
    }
    return st;
}

static TkStatus load_path_row(char **f, size_t n, LoadCtx *ctx)
{
    int a = 0, b = 0;
    double meters = 0.0;
    if (n != 3) {
        return TK_ERR_PARSE;
    }
    TkStatus st = map_zone(ctx, f[0], &a);
    if (st == TK_OK) st = map_zone(ctx, f[1], &b);
    if (st == TK_OK) st = tk_parse_double(f[2], 0.01, 1e7, &meters);
    if (st == TK_OK) st = graph_add_path(ctx->graph, a, b, meters);
    if (st == TK_OK) ctx->report->paths++;
    return st;
}

static TkStatus load_volunteer_row(char **f, size_t n, LoadCtx *ctx)
{
    VolunteerInfo v;
    memset(&v, 0, sizeof v);
    long tasks = 0;
    if (n != 4 || tk_validate_name(f[1]) != TK_OK) {
        return TK_ERR_PARSE;
    }
    TkStatus st = parse_int(f[0], 1, INT32_MAX, &v.id);
    if (st == TK_OK) st = map_zone(ctx, f[2], &v.zone_id);
    if (st == TK_OK) st = tk_parse_long(f[3], 0, UINT32_MAX, &tasks);
    if (st != TK_OK) {
        return st;
    }
    memcpy(v.name, f[1], strlen(f[1]) + 1);
    v.tasks_completed = (unsigned)tasks;
    st = roster_restore(ctx->roster, &v);
    if (st == TK_OK) {
        ctx->report->volunteers++;
    }
    return st;
}

static TkStatus load_tree_row(char **f, size_t n, LoadCtx *ctx)
{
    if (n != 9) {
        return TK_ERR_PARSE;
    }
    TreeInfo t;
    memset(&t, 0, sizeof t);
    long id = 0, diseased = 0;

    TkStatus st = tk_parse_long(f[0], 1, (long)UINT32_MAX - 1, &id);
    if (st == TK_OK) st = species_parse(f[1], &t.species);
    if (st == TK_OK) st = map_zone(ctx, f[2], &t.zone_id);
    if (st == TK_OK) st = tk_parse_double(f[3], 0.0, TREE_WATER_FULL, &t.water_level);
    if (st == TK_OK) st = tk_parse_double(f[4], 1e-6, 100.0, &t.decay_rate);
    if (st == TK_OK) st = tk_parse_double(f[5], 0.0, 1e12, &t.planted_at);
    if (st == TK_OK) st = tk_parse_double(f[6], 0.0, 1e12, &t.last_watered);
    if (st == TK_OK) st = tk_parse_long(f[7], 0, 1, &diseased);
    if (st == TK_OK) st = parse_int(f[8], -1, INT32_MAX, &t.assigned_volunteer);
    if (st != TK_OK) {
        return st;
    }
    t.id = (uint32_t)id;
    t.diseased = (diseased == 1);

    /* An assignment to a volunteer who no longer exists is dropped. */
    if (t.assigned_volunteer >= 0 && roster_name(ctx->roster, t.assigned_volunteer) == NULL) {
        t.assigned_volunteer = -1;
    }
    st = registry_restore(ctx->registry, &t);
    if (st == TK_OK) {
        ctx->report->trees++;
    }
    return st;
}

TkStatus storage_load(const char *dir, Graph *graph, TreeRegistry *reg,
                      Roster *roster, StorageReport *report)
{
    if (dir == NULL || graph == NULL || reg == NULL || roster == NULL) {
        return TK_ERR_INVALID;
    }
    if (graph_zone_count(graph) != 0 || registry_count(reg) != 0 || roster_count(roster) != 0) {
        return TK_ERR_INVALID; /* loading merges nothing: start empty */
    }

    StorageReport local;
    memset(&local, 0, sizeof local);
    LoadCtx ctx = {
        .graph = graph,
        .registry = reg,
        .roster = roster,
        .zone_map = hashmap_create(64),
        .report = &local,
        .next_tree_id = 0,
    };
    if (ctx.zone_map == NULL) {
        return TK_ERR_NOMEM;
    }

    /* Order matters: the clock must be set before trees compute their keys,
     * and zones must exist before anything refers to them. */
    TkStatus st = read_csv(dir, FILE_META, false, load_meta_row, &ctx);
    if (st == TK_OK) st = read_csv(dir, FILE_ZONES, true, load_zone_row, &ctx);
    if (st == TK_OK) st = read_csv(dir, FILE_PATHS, false, load_path_row, &ctx);
    if (st == TK_OK) st = read_csv(dir, FILE_VOLUNTEERS, false, load_volunteer_row, &ctx);
    if (st == TK_OK) st = read_csv(dir, FILE_TREES, false, load_tree_row, &ctx);
    if (st == TK_OK) {
        registry_reserve_id(reg, ctx.next_tree_id);
    }

    hashmap_destroy(ctx.zone_map);
    if (report != NULL) {
        *report = local;
    }
    return st;
}
