# TreeKeeper – User Manual

## Starting

Run `make` once, then `./a.out`. The dashboard is shown first, followed by the menu. Type the number of an option and press Enter.

## Dashboard (option 1)

- Simulated time (Day, hour), speed, and whether the timer is running
- Number of trees in each state: Healthy (60% water or more), Thirsty (30–60%), Critical (below 30%), Wilted (0%), Diseased
- The 5 most urgent trees from the min heap
- Volunteer leaderboard (trees watered)

## Menu options

| No. | Option | What to enter |
|---|---|---|
| 1 | Dashboard | – |
| 2 | Plant a tree | species, zone name, water level (0–100). You get a tree ID back for the QR code |
| 3 | Water a tree (QR) | tree ID. Water goes back to 100% and the assigned volunteer gets credit |
| 4 | Search tree by ID | tree ID |
| 5 | Mark diseased/healthy | tree ID, then 1 (diseased) or 0 (healthy) |
| 6 | Remove a tree | tree ID |
| 7 | Show all trees | – |
| 8 | Add a zone | zone name (no spaces) |
| 9 | Add a path | two zone names and the distance in metres |
| 10 | Show campus map | – |
| 11 | Shortest route | two zone names |
| 12 | Add volunteer | name and starting zone |
| 13 | Show volunteers | – |
| 14 | Assign urgent trees | volunteer ID and number of trees. Shows the walking route |
| 15 | Pause/resume timer | – |
| 16 | Change speed | simulated hours per real second |
| 17 | Save now | – |
| 0 | Save and exit | – |

Species: Neem, Banyan, Peepal, Mango, Teak, Bamboo, Gulmohar, Arjuna.
Names must not contain spaces or commas (use `_`, for example `Main_Gate`).

## Data

Everything is saved in the `data/` folder as CSV files when you exit (option 0) or choose option 17. To start again with the sample campus, delete the `data/` folder.
