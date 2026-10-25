/**
 * @file test_main.c
 * @brief Unit tests for every TreeKeeper module (run with `make test`).
 *
 * A tiny self-contained framework: CHECK() records a failure with its
 * file/line and keeps going, so one run reports every broken assertion.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "graph.h"
#include "hashmap.h"
#include "minheap.h"
#include "registry.h"
#include "storage.h"
#include "strbuf.h"
#include "volunteer.h"

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        g_checks++;                                                        \
        if (!(cond)) {                                                     \
            g_failures++;                                                  \
            fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                  \
    } while (0)

#define RUN(test)                          \
    do {                                   \
        printf("  %-40s", #test);          \
        int before = g_failures;           \
        test();                            \
        puts(g_failures == before ? "ok" : "FAILED"); \
    } while (0)

/* ---------------------------------------------------------------- */

static void test_hashmap_basic(void)
{
    HashMap *map = hashmap_create(0);
    int a = 1, b = 2;
    CHECK(map != NULL);
    CHECK(hashmap_put(map, 10, &a) == TK_OK);
    CHECK(hashmap_put(map, 20, &b) == TK_OK);
    CHECK(hashmap_get(map, 10) == &a);
    CHECK(hashmap_put(map, 10, &b) == TK_OK); /* replace */
    CHECK(hashmap_get(map, 10) == &b);
    CHECK(hashmap_size(map) == 2);
    CHECK(hashmap_remove(map, 10, NULL) == TK_OK);
    CHECK(hashmap_remove(map, 10, NULL) == TK_ERR_NOT_FOUND);
    CHECK(!hashmap_contains(map, 10));
    CHECK(hashmap_contains(map, 20));
    CHECK(hashmap_put(NULL, 1, NULL) == TK_ERR_INVALID);
    hashmap_destroy(map);
}

static void test_hashmap_resize_and_tombstones(void)
{
    HashMap *map = hashmap_create(4);
    const uint32_t n = 20000;
    for (uint32_t i = 0; i < n; i++) {
        CHECK(hashmap_put(map, i * 7u, (void *)(uintptr_t)(i + 1)) == TK_OK);
    }
    CHECK(hashmap_size(map) == n);
    /* Remove the even keys, then make sure the odd keys are still reachable
     * through the tombstones left behind. */
    for (uint32_t i = 0; i < n; i += 2) {
        CHECK(hashmap_remove(map, i * 7u, NULL) == TK_OK);
    }
    bool all_ok = true;
    for (uint32_t i = 1; i < n; i += 2) {
        all_ok &= (hashmap_get(map, i * 7u) == (void *)(uintptr_t)(i + 1));
    }
    CHECK(all_ok);
    CHECK(hashmap_size(map) == n / 2);
    /* Churn: many insert/remove cycles must not exhaust free slots. */
    for (uint32_t i = 0; i < 100000; i++) {
        CHECK(hashmap_put(map, 1000000u + i, NULL) == TK_OK);
        CHECK(hashmap_remove(map, 1000000u + i, NULL) == TK_OK);
    }
    CHECK(hashmap_size(map) == n / 2);
    hashmap_destroy(map);
}

/* ---------------------------------------------------------------- */

static void test_minheap_order(void)
{
    MinHeap *heap = minheap_create(1);
    /* Pseudo-random keys; popping must return them sorted (heap sort). */
    unsigned seed = 12345;
    for (uint32_t i = 0; i < 5000; i++) {
        seed = seed * 1103515245u + 12345u;
        CHECK(minheap_push(heap, i, (double)(seed % 100000)) == TK_OK);
    }
    CHECK(minheap_push(heap, 0, 1.0) == TK_ERR_EXISTS);
    double prev = -1.0, key = 0.0;
    uint32_t id = 0;
    bool sorted = true;
    size_t popped = 0;
    while (minheap_pop(heap, &id, &key) == TK_OK) {
        sorted &= (key >= prev);
        prev = key;
        popped++;
    }
    CHECK(sorted);
    CHECK(popped == 5000);
    CHECK(minheap_pop(heap, &id, &key) == TK_ERR_EMPTY);
    minheap_destroy(heap);
}

static void test_minheap_update_remove(void)
{
    MinHeap *heap = minheap_create(4);
    for (uint32_t i = 1; i <= 10; i++) {
        minheap_push(heap, i, (double)i * 10.0);
    }
    uint32_t id = 0;
    CHECK(minheap_update(heap, 7, 1.0) == TK_OK); /* decrease-key */
    CHECK(minheap_peek(heap, &id, NULL) == TK_OK && id == 7);
    CHECK(minheap_update(heap, 7, 1000.0) == TK_OK); /* increase-key */
    CHECK(minheap_peek(heap, &id, NULL) == TK_OK && id == 1);
    CHECK(minheap_remove(heap, 1) == TK_OK);
    CHECK(!minheap_contains(heap, 1));
    CHECK(minheap_peek(heap, &id, NULL) == TK_OK && id == 2);
    CHECK(minheap_update(heap, 99, 0.0) == TK_ERR_NOT_FOUND);
    CHECK(minheap_push(heap, 50, NAN) == TK_ERR_INVALID);

    uint32_t ids[4];
    double keys[4];
    size_t n = 0;
    CHECK(minheap_peek_k(heap, 4, ids, keys, &n) == TK_OK && n == 4);
    CHECK(ids[0] == 2 && ids[1] == 3 && ids[2] == 4 && ids[3] == 5);
    CHECK(minheap_size(heap) == 9); /* peek_k must not remove anything */
    double key = 0.0;
    CHECK(minheap_get_key(heap, 7, &key) == TK_OK && key == 1000.0);
    minheap_destroy(heap);
}

/* ---------------------------------------------------------------- */

/*      A --4-- B --1-- C
 *       \             /
 *        ----10------      D (isolated)                              */
static Graph *build_test_graph(void)
{
    Graph *g = graph_create();
    graph_add_zone(g, "A", NULL);
    graph_add_zone(g, "B", NULL);
    graph_add_zone(g, "C", NULL);
    graph_add_zone(g, "D", NULL);
    graph_add_path(g, 0, 1, 4.0);
    graph_add_path(g, 1, 2, 1.0);
    graph_add_path(g, 0, 2, 10.0);
    return g;
}

static void test_graph_dijkstra(void)
{
    Graph *g = build_test_graph();
    CHECK(graph_zone_count(g) == 4 && graph_path_count(g) == 3);
    CHECK(graph_add_zone(g, "a", NULL) == TK_ERR_EXISTS); /* case-insensitive */
    CHECK(graph_add_zone(g, "bad name", NULL) == TK_ERR_INVALID);
    CHECK(graph_add_path(g, 0, 0, 1.0) == TK_ERR_INVALID);
    CHECK(graph_add_path(g, 0, 1, -3.0) == TK_ERR_INVALID);

    Path *p = NULL;
    CHECK(graph_shortest_path(g, 0, 2, &p) == TK_OK);
    CHECK(path_length(p) == 3);
    CHECK(path_zone_at(p, 0) == 0 && path_zone_at(p, 1) == 1 && path_zone_at(p, 2) == 2);
    CHECK(fabs(path_distance(p) - 5.0) < 1e-9); /* A-B-C beats A-C */
    path_destroy(p);

    CHECK(graph_shortest_path(g, 0, 3, &p) == TK_ERR_UNREACHABLE && p == NULL);
    CHECK(graph_shortest_path(g, 2, 2, &p) == TK_OK && path_length(p) == 1);
    path_destroy(p);

    /* Removing B-C forces the direct A-C edge. */
    CHECK(graph_remove_path(g, 1, 2) == TK_OK);
    CHECK(graph_remove_path(g, 1, 2) == TK_ERR_NOT_FOUND);
    CHECK(graph_shortest_path(g, 0, 2, &p) == TK_OK && fabs(path_distance(p) - 10.0) < 1e-9);
    path_destroy(p);
    graph_destroy(g);
}

static void test_graph_plan_route(void)
{
    Graph *g = build_test_graph();
    int stops[] = { 2, 1, 1 };
    Path *p = NULL;
    /* From A: nearest stop is B (4), then C (1). Duplicate B is merged. */
    CHECK(graph_plan_route(g, 0, stops, 3, &p) == TK_OK);
    CHECK(path_length(p) == 3);
    CHECK(path_zone_at(p, 1) == 1 && path_is_stop(p, 1));
    CHECK(path_zone_at(p, 2) == 2 && path_is_stop(p, 2));
    CHECK(!path_is_stop(p, 0));
    CHECK(fabs(path_distance(p) - 5.0) < 1e-9);
    path_destroy(p);

    int unreachable[] = { 3 };
    CHECK(graph_plan_route(g, 0, unreachable, 1, &p) == TK_ERR_UNREACHABLE && p == NULL);
    graph_destroy(g);
}

/* ---------------------------------------------------------------- */

static void test_registry_urgency(void)
{
    TreeRegistry *reg = registry_create();
    uint32_t neem = 0, bamboo = 0, mango = 0;
    /* Bamboo at 40% dries before Neem at 30%: ordering is by dry-out time. */
    CHECK(registry_plant(reg, SPECIES_NEEM, 0, 30.0, &neem) == TK_OK);
    CHECK(registry_plant(reg, SPECIES_BAMBOO, 0, 40.0, &bamboo) == TK_OK);
    CHECK(registry_plant(reg, SPECIES_MANGO, 1, 90.0, &mango) == TK_OK);
    CHECK(registry_plant(reg, SPECIES_COUNT, 0, 50.0, NULL) == TK_ERR_INVALID);
    CHECK(registry_plant(reg, SPECIES_NEEM, 0, 150.0, NULL) == TK_ERR_INVALID);

    TreeInfo top[3];
    size_t n = 0;
    CHECK(registry_most_urgent(reg, 3, top, &n) == TK_OK && n == 3);
    CHECK(top[0].id == bamboo && top[1].id == neem && top[2].id == mango);

    /* Time passes: water drains and statuses change. */
    registry_advance(reg, 10.0);
    TreeInfo t;
    CHECK(registry_get(reg, bamboo, &t) == TK_OK);
    CHECK(fabs(t.water_level - (40.0 - 2.8 * TREE_YOUNG_FACTOR * 10.0)) < 1e-9);
    CHECK(t.status == TREE_CRITICAL);
    registry_advance(reg, 100.0);
    CHECK(registry_get(reg, bamboo, &t) == TK_OK && t.water_level == 0.0 && t.status == TREE_WILTED);

    /* Assign pops the most urgent; it leaves the queue until watered. */
    CHECK(registry_assign_next(reg, 1, &t) == TK_OK && t.id == bamboo);
    RegistryStats stats;
    registry_stats(reg, &stats);
    CHECK(stats.total == 3 && stats.queued == 2 && stats.assigned == 1);

    int previous = -1;
    CHECK(registry_water(reg, bamboo, &previous) == TK_OK && previous == 1);
    CHECK(registry_get(reg, bamboo, &t) == TK_OK && t.water_level == 100.0 && t.assigned_volunteer == -1);
    registry_stats(reg, &stats);
    CHECK(stats.queued == 3 && stats.assigned == 0);

    CHECK(registry_set_diseased(reg, mango, true) == TK_OK);
    CHECK(registry_get(reg, mango, &t) == TK_OK && t.status == TREE_DISEASED);
    CHECK(registry_release(reg, mango) == TK_ERR_INVALID);
    CHECK(registry_remove(reg, mango) == TK_OK);
    CHECK(registry_get(reg, mango, &t) == TK_ERR_NOT_FOUND);
    CHECK(registry_water(reg, 424242, NULL) == TK_ERR_NOT_FOUND);
    registry_destroy(reg);
}

/* ---------------------------------------------------------------- */

static void test_roster(void)
{
    Roster *r = roster_create();
    int a = 0, b = 0;
    CHECK(roster_add(r, "Asha", 0, &a) == TK_OK);
    CHECK(roster_add(r, "Bala", 1, &b) == TK_OK);
    CHECK(roster_add(r, "asha", 0, NULL) == TK_ERR_EXISTS);
    CHECK(roster_add(r, "", 0, NULL) == TK_ERR_INVALID);
    CHECK(roster_credit(r, b) == TK_OK && roster_credit(r, b) == TK_OK);
    VolunteerInfo top[2];
    CHECK(roster_leaderboard(r, 2, top) == 2 && top[0].id == b && top[0].tasks_completed == 2);
    CHECK(roster_credit(r, 99) == TK_ERR_NOT_FOUND);
    roster_destroy(r);
}

static void test_strbuf(void)
{
    StrBuf *sb = strbuf_create();
    for (int i = 0; i < 1000; i++) {
        strbuf_printf(sb, "%04d,", i);
    }
    CHECK(strbuf_len(sb) == 5000);
    CHECK(strncmp(strbuf_cstr(sb), "0000,0001,", 10) == 0);
    strbuf_clear(sb);
    CHECK(strbuf_len(sb) == 0 && strbuf_cstr(sb)[0] == '\0');
    strbuf_destroy(sb);
}

/* ---------------------------------------------------------------- */

static void remove_saved_files(const char *dir)
{
    static const char *const files[] = { "meta.csv", "zones.csv", "paths.csv",
                                         "volunteers.csv", "trees.csv" };
    char path[TK_PATH_MAX];
    for (size_t i = 0; i < TK_ARRAY_LEN(files); i++) {
        snprintf(path, sizeof path, "%s/%s", dir, files[i]);
        remove(path);
    }
    rmdir(dir);
}

static void test_storage_roundtrip(void)
{
    /* storage_save() creates the directory itself. */
    char dir[64];
    snprintf(dir, sizeof dir, "/tmp/treekeeper_test_%ld", (long)getpid());

    Graph *g = build_test_graph();
    TreeRegistry *reg = registry_create();
    Roster *r = roster_create();
    uint32_t id = 0;
    roster_add(r, "Asha", 2, NULL);
    registry_plant(reg, SPECIES_TEAK, 1, 55.0, &id);
    registry_plant(reg, SPECIES_PEEPAL, 2, 10.0, NULL);
    registry_set_diseased(reg, id, true);
    registry_advance(reg, 5.0);
    CHECK(storage_save(dir, g, reg, r, 2.5) == TK_OK);
    CHECK(storage_exists(dir));

    Graph *g2 = graph_create();
    TreeRegistry *reg2 = registry_create();
    Roster *r2 = roster_create();
    StorageReport rep;
    CHECK(storage_load(dir, g2, reg2, r2, &rep) == TK_OK);
    CHECK(rep.zones == 4 && rep.paths == 3 && rep.trees == 2 && rep.volunteers == 1);
    CHECK(rep.skipped_rows == 0 && fabs(rep.speed - 2.5) < 1e-9);
    CHECK(fabs(registry_clock(reg2) - 5.0) < 1e-6);
    CHECK(registry_next_id(reg2) == registry_next_id(reg));

    TreeInfo a, b;
    CHECK(registry_get(reg, id, &a) == TK_OK && registry_get(reg2, id, &b) == TK_OK);
    CHECK(fabs(a.water_level - b.water_level) < 1e-3 && b.diseased && b.zone_id == 1);

    Path *p = NULL;
    CHECK(graph_shortest_path(g2, 0, 2, &p) == TK_OK && fabs(path_distance(p) - 5.0) < 1e-9);
    path_destroy(p);

    /* Loading into non-empty containers is refused. */
    CHECK(storage_load(dir, g2, reg2, r2, NULL) == TK_ERR_INVALID);

    /* A corrupted row is skipped, not fatal. */
    char path[TK_PATH_MAX];
    snprintf(path, sizeof path, "%s/trees.csv", dir);
    FILE *fp = fopen(path, "a");
    CHECK(fp != NULL);
    if (fp != NULL) {
        fputs("abc,Neem,0,50,1,0,0,0,-1\n7777,Neem,99,50,1,0,0,0,-1\n", fp);
        fclose(fp);
    }
    Graph *g3 = graph_create();
    TreeRegistry *reg3 = registry_create();
    Roster *r3 = roster_create();
    CHECK(storage_load(dir, g3, reg3, r3, &rep) == TK_OK);
    CHECK(rep.trees == 2 && rep.skipped_rows == 2);

    graph_destroy(g);  registry_destroy(reg);  roster_destroy(r);
    graph_destroy(g2); registry_destroy(reg2); roster_destroy(r2);
    graph_destroy(g3); registry_destroy(reg3); roster_destroy(r3);
    remove_saved_files(dir);
}

int main(void)
{
    puts("TreeKeeper unit tests");
    RUN(test_hashmap_basic);
    RUN(test_hashmap_resize_and_tombstones);
    RUN(test_minheap_order);
    RUN(test_minheap_update_remove);
    RUN(test_graph_dijkstra);
    RUN(test_graph_plan_route);
    RUN(test_registry_urgency);
    RUN(test_roster);
    RUN(test_strbuf);
    RUN(test_storage_roundtrip);
    printf("%d checks, %d failure(s)\n", g_checks, g_failures);
    return g_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
