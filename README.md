# TreeKeeper

**A Data-Driven Afforestation and Maintenance Scheduling System**
Team **PROTOCOL 0** · CSLR31 Data Structures Laboratory · NIT Tiruchirappalli
Aligned with **UN SDG 15 – Life on Land**

| Member | Reg. No. |
|---|---|
| Shaurya Bharat Makwana | 106125109 |
| Shravanth S Sennimalai | 106125111 |
| Shreya Ashok Mhetre    | 106125113 |

TreeKeeper tracks planted saplings and works out which ones need water first. It hands those trees to volunteers and plans the shortest walk between them. A background thread runs the simulated clock, so water levels keep dropping while the program is open. All state is saved to CSV files on exit and loaded again on the next start.

---

## Quick start

```sh
make            # optimised build -> ./a.out
./a.out         # first run loads a demo campus; later runs reload data/
```

## SETUP START (LINUX)
```sh
ls -la
rm -rf cc
sudo apt update
sudo apt install build-essential
make
./a.out
```

## WSL SETUP START
```sh
bash
ls -la
rm -rf cc
sudo apt update
sudo apt install build-essential
make
./a.out
```


Inside the program, type `help`. Try `monitor` (live view), `urgent`, `assign Shreya 3`, `route Library Orion`, `water 1014`, then `quit`.

Requirements: a C11 compiler (gcc or clang) and a POSIX system (Linux, macOS, or WSL). No external libraries are needed.

## Directory layout

```
.
├── Makefile            build system (a.out is produced here)
├── README.md           this file
├── include/            all .h files (module interfaces)
├── Source/             all .c files (module implementations)
├── object/             all .o files (created by make, per build mode)
├── Tests/              unit tests + scripted CLI demo
├── Document/           user manual
├── Video/              project experience video (.mp4)
└── data/               saved state (created on first exit)
```

## Modules

| Module | Files | Responsibility |
|---|---|---|
| Common | `tk_common.h/.c` | Status codes, overflow-safe allocation, strict number parsing, name validation |
| String builder | `strbuf.h/.c` | Growable string used to build each screen before printing it in one go |
| Hash map | `hashmap.h/.c` | Open addressing, linear probing, tombstones, resize at 70% load |
| Min-heap | `minheap.h/.c` | Indexed binary heap with decrease-key, arbitrary remove, and non-destructive top-k |
| Graph | `graph.h/.c` | Adjacency list, **Dijkstra**, multi-stop route planning |
| Registry | `registry.h/.c` | Tree records indexed by hash map + min-heap, guarded by a mutex |
| Volunteers | `volunteer.h/.c` | Dynamic-array roster and leaderboard |
| Simulator | `simulator.h/.c` | pthread worker that advances simulated time |
| Storage | `storage.h/.c` | CSV save/load with atomic replace and row validation |
| UI / CLI | `ui.h/.c`, `cli.h/.c` | ANSI dashboard, live monitor, command table |
| Entry | `main.c`, `seed.c` | Option parsing, start-up and shutdown order, demo data |

Every data structure is an **opaque pointer**: the header has only `typedef struct X X;`, and the fields are visible only inside the `.c` file. Every container grows on demand. There are no fixed array limits.

## Data structures and complexity

| Operation | Structure | Cost |
|---|---|---|
| Find / update a tree by ID (QR scan) | Hash map | O(1) expected |
| Plant / water / remove a tree | Hash map + heap | O(log n) |
| Most urgent tree | Min-heap peek | O(1) |
| Top-k urgent trees for the dashboard | Heap best-first search | O(k log k), heap unchanged |
| Assign k trees to a volunteer | k × extract-min | O(k log n) |
| Shortest path between zones | Dijkstra + indexed heap | O((V+E) log V) |
| Walking route through k zones | Repeated Dijkstra, nearest stop first | O(k (V+E) log V) |
| One simulation tick | Linear pass over all trees | O(n), **no heap reordering** |

### Key design decisions (useful for the viva)

1. **The heap key is the time a tree will dry out, not its water level.** Each tree loses water at a constant rate *r*, so it runs dry at `t_dry = now + water / r`. As time passes, water drops but `t_dry` stays the same. The heap never has to be reordered on a tick. Ordering by water alone would also be wrong: a Bamboo at 28% dries out before a Teak at 22% (see the demo output).
2. **The heap keeps a hash map from ID to array index.** That makes decrease-key and removal O(log n) instead of O(n). Dijkstra uses the same heap.
3. **Dijkstra uses decrease-key.** Each zone enters the frontier at most once, and its distance is final when it is popped (all weights are > 0).
4. **The route planner picks the nearest unvisited stop each time.** Visiting several zones in the best possible order is the Travelling Salesman Problem, which is NP-hard. Picking the nearest stop is a standard heuristic. The distances it compares are real shortest paths.
5. **Thread safety.** Only the registry is shared with the simulation thread, and every registry call locks a mutex. Callers only ever get *copies* of tree data (`TreeInfo`), never pointers into the registry. The simulator never holds its own lock while it takes the registry's, so the two locks cannot deadlock. The worker thread blocks all signals, so Ctrl+C always reaches `main`, which stops the thread, saves, and frees memory.
6. **Crash-safe saving.** Each CSV file is written to `file.tmp` and then `rename()`d over the old one, which is atomic on POSIX. When loading, bad rows are skipped and counted. If the load fails, the program exits **without** saving, so it cannot overwrite good data.

## Build targets

| Command | Purpose |
|---|---|
| `make` | Release build (`-O2`) |
| `make debug` | `-O0 -g3` build for gdb and Valgrind |
| `make asan` | AddressSanitizer + UBSan build (best on Linux) |
| `make test` | Build and run the unit tests |
| `make smoke` | Run `Tests/demo_script.txt`, then reload the saved state |
| `make valgrind` | Run the tests and the CLI demo under Valgrind (Linux) |
| `make leaks` | Same checks with macOS `leaks` |
| `make clean` / `make distclean` | Remove build output / also remove `data/` |

All code compiles with no warnings under `-Wall -Wextra -Wpedantic -Wshadow -Wformat=2 -Wstrict-prototypes -Wmissing-prototypes`.

## Testing

* `Tests/test_main.c` holds the unit tests, about 235,000 checks in all. They cover hash map resizing and tombstone churn, heap sort order, decrease/increase-key, `peek_k`, Dijkstra on a known graph (including unreachable zones and removed edges), route planning with duplicate stops, registry urgency ordering, draining over time, assignment, watering and disease, the roster leaderboard, and a full save→load round trip with corrupted rows.
* `Tests/demo_script.txt` is a scripted session. It mixes valid and invalid commands (unknown species or zone, unreachable zone, missing tree) to show the error handling.
* Memory: `make leaks` reports **0 leaks** for both the tests and the CLI session. Run `make valgrind` on Linux to get the matching Valgrind report.

> Note: on recent macOS versions, Apple's AddressSanitizer runtime can hang during start-up. Use `make leaks` on a Mac and `make asan` / `make valgrind` on Linux.

## References

* T. H. Cormen et al., *Introduction to Algorithms*, 3rd ed., MIT Press — Ch. 6 (Heaps), Ch. 11 (Hash Tables), Ch. 22–24 (Graphs, Dijkstra).
* E. W. Dijkstra, "A note on two problems in connexion with graphs", *Numerische Mathematik* 1, 1959.
* A. Appleby, MurmurHash3 finaliser (`fmix32`), public domain.
* IEEE Std 1003.1-2017 (POSIX): pthreads, `getline`, `rename`, `select`.
* NO_COLOR convention — https://no-color.org
