# TreeKeeper — User Manual

## 1. Starting the program

```sh
make                 # build once
./a.out              # start (loads data/ if it exists, otherwise a demo campus)
```

| Option | Meaning |
|---|---|
| `--data DIR` | Use another folder for saved data |
| `--speed H` | Simulated hours per real second (default 1) |
| `--fresh` | Ignore saved data and load the demo campus |
| `--empty` | Start with no zones, trees or volunteers |
| `--no-save` | Do not save on exit |
| `--no-color` | Plain output without colours |

## 2. The dashboard

The dashboard is redrawn after every command:

* **Header:** the simulated clock (`Day 3 14:05`), whether the simulation is running or paused, and its speed.
* **Health:** how many trees are HEALTHY (≥60%), THIRSTY (30–60%), CRITICAL (<30%), WILTED (0%) or DISEASED. A red **ALERT** line appears when any tree is critical or wilted.
* **Urgent watering queue:** the 8 trees that will dry out soonest.
* **Tasks in progress:** trees currently assigned to a volunteer.
* **Leaderboard:** volunteers ranked by trees watered.

Type `monitor` to get a live view that refreshes every second. Press **Enter** to go back.

## 3. Everyday workflow

1. **Log a sapling:** `plant Neem Library 80` gives back an ID such as `1017`. Print it as a QR code (`TK-1017`) on the tree guard.
2. **Hand out work:** `assign Shreya 3` gives Shreya the 3 most urgent trees and prints the shortest walking route from her base zone. Stops are shown in `[brackets]`.
3. **Confirm the watering:** when a volunteer scans the QR code, run `water 1017`. The tree is refilled to 100%, put back in the queue, and the volunteer gets the credit. Use `water 1017 Shaurya` to credit someone else.
4. **Report disease:** `status 1017 diseased`, and later `status 1017 healthy`.

## 4. Command reference

| Command | Description |
|---|---|
| `plant <species> <zone> [water%]` | Species: Neem, Banyan, Peepal, Mango, Teak, Bamboo, Gulmohar, Arjuna |
| `water <id> [volunteer]` (alias `scan`) | Record a watering |
| `info <id>` | All details of one tree |
| `list [zone]` | All trees, most urgent first |
| `urgent [k]` | The k most urgent trees |
| `status <id> diseased\|healthy` | Set or clear the disease flag |
| `remove <id>` | Delete a tree |
| `zones` · `zone <name>` | List zones / add a zone |
| `path <a> <b> <metres>` · `unpath <a> <b>` | Add, update or remove a walkway |
| `map` | Show every zone and its walkways |
| `route <from> <to>` | Shortest walk between two zones |
| `volunteers` · `volunteer <name> <zone>` | Show the roster / register a volunteer |
| `assign <volunteer> [k]` | Give out the k most urgent trees and plan a route |
| `release <id>` | Return an assigned tree to the queue |
| `pause` · `resume` · `speed <h/s>` · `tick <hours>` | Control simulated time |
| `save` · `quit` | Save now / save and exit |

You can type zones and volunteers by name (not case-sensitive) or by number. Names may contain letters, digits, `_`, `-` and `.`, with no spaces.

## 5. Saved data

All state lives in `data/` as plain CSV files (`zones`, `paths`, `trees`, `volunteers`, `meta`). You can open them in a spreadsheet. If you edit one by hand and a row is invalid, that row is skipped and reported at the next start. **Ctrl+C** also saves before exiting.
