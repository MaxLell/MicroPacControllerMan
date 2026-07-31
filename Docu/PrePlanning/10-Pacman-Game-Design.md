# 10 Pacman Game Design

[← Back to Index](Index.md) · See also [02 Requirements](02-Requirements.md) · [03 Architecture](03-Architecture.md)

This document pins down the concrete game rules that [02 Requirements](02-Requirements.md) intentionally left open (it closes [A-005](05-Risks-Assumptions-and-Dependencies.md#52-assumptions) and fills in the tunable constants of [A-006](05-Risks-Assumptions-and-Dependencies.md#52-assumptions)). It is the input for **Milestone 3 (Pacman Development)** — enough detail to implement Model/Control and to write unit tests with expected values. Numbers marked *(tunable)* are starting defaults that may be adjusted for feel; the rules around them are fixed.

## 10.1 Movement & Tick Model

- **Grid-based.** The maze is a grid of square cells (§10.2). Pacman and each ghost occupy exactly one cell and face one of four directions (North / East / South / West). Movement is one whole cell at a time, which keeps collisions, pellet-eating and ghost targeting deterministic and unit-testable.
- **But the *view* moves in pixels.** A cell is about 21 px on the 240 x 320 panel and Pacman crosses one every 150 ms, so cell-granular drawing means a **21-pixel jump 6.7 times a second** — visibly choppy, and no display refresh rate can rescue it. This was measured rather than guessed: the frame-rate ladder of [M2 Board Bring-Up §3.2](../Design/M2-Board-Bring-Up.md) shows 9 px per frame already reads as stutter. So each moving entity additionally carries **how far it is between its current cell and the next**, and the Game-View interpolates ([03 §3.6](03-Architecture.md#36-pacman-sub-application-architecture)). At 60 FPS that is ~2.3 px per frame, the smooth end of the same ladder.
  - The rules do not see this. Interpolation is presentation: an entity *is* in its current cell for every purpose the game logic cares about, and the fraction is written by the movement timer and read only by the view.
  - The earlier wording here said there is no sub-cell position at all, justified by "tile rendering on the monochrome display". That display is gone, and with it the reason.
- **Simulation tick.** The game runs on a fixed, fine simulation tick. Each moving entity has a **movement period** and advances one cell when its period elapses; pellet-eating, collisions and mode timers are evaluated every tick. Rendering runs independently at ≥ 60 FPS (NFR-002), interpolating between steps as above.
- **Pacman movement.** Pacman moves one cell every **150 ms** *(tunable)*, constant across all levels. He keeps a current direction and a *queued* direction; `MSG_INPUT_DIRECTION` sets the queued one. When he is due to move: if the queued direction is not blocked by a wall it becomes current; then he moves one cell if that cell is open, otherwise he stays put (stopped against a wall until the direction changes).
- **Ghost movement.** A ghost moves one cell toward its target (§10.4) every ghost-movement-period, which is **per level** (§10.9) — slower than Pacman on level 1, faster than him on level 5. A ghost never reverses onto the cell it just left, except when its mode changes (scatter↔chase, or entering/leaving frightened). Frightened ghosts move at half their current speed.
- **Tunnels.** Leaving the maze through a tunnel mouth (§10.2) re-enters at the opposite mouth — on the same row for the horizontal tunnel, the same column for the vertical one (FR-012).

## 10.2 Mazes (Playfield)

The game has **five mazes, one per level** (FR-025), each a reduced, display-fit grid (FR-022) — left-right symmetric, fully connected, with at least one tunnel and a central ghost pen. **Level 1's maze** is the 11 × 9 reference below (tile ≈ 10 × 10 px → ≈ 110 × 90 px, centred, HUD in the remaining space). The mazes for **levels 2–5** are distinct layouts of increasing difficulty (more dead-ends, longer corridors, fewer safe corners, possibly fewer power pellets), authored to the same format and constraints during M3.

Legend: `#` wall · `.` pellet · `o` power pellet · `P` Pacman start · `G` ghost start (pen) · space = open, no pellet (tunnel mouths).

```
#####.#####
#o..#.#..o#
#.#.#.#.#.#
#.........#
...#GGG#...
#.........#
#.#.#.#.#.#
#o..#P#..o#
#####.#####
```

- **Pellets** occupy every open path cell except the pen cells, Pacman's start cell, and the tunnel-mouth cells.
- **Power pellets** (`o`) sit in the four corners.
- **Ghost pen** (`G`) is the three central cells; ghosts start here and return here when eaten. It is open above and below (no door in the reference layout).
- **Tunnels** (two): the **horizontal** tunnel opens the middle row's left and right edges, which wrap to each other; the **vertical** tunnel opens the top and bottom edges of Pacman's column, which wrap to each other — so Pacman's start pocket always has an escape rather than being a dead-end trap.
- **Level clear** occurs when the last pellet and power pellet of the current maze are eaten — see §10.7 / §10.9 for what happens next.

## 10.3 Entities

The Model (see [03 Architecture §3.1](03-Architecture.md#31-mvp-architecture-model--view--control)) holds:

- **Maze**: the static walls plus a dynamic map of which pellets/power pellets remain.
- **Pacman**: cell, current direction, queued direction, alive flag.
- **Four ghosts**, each: cell, direction, personality (Blinky / Pinky / Inky / Clyde), mode (scatter / chase), frightened flag + which "eaten" state, and its scatter-corner.
- **Score** (cumulative across levels), **lives** (starting count per FR-006, default 3), **current level** (1–5), **high score** (in-memory copy of NVM), the **frightened timer**, the **scatter/chase phase timer**, and **pellets remaining** in the current maze.

Per [10.1] all entities are Agents (the movable-entity base, [03 Architecture §3.6](03-Architecture.md#36-pacman-sub-application-architecture)): Pacman and the ghosts share the "occupy a cell, face a direction, try to step" behaviour and specialise only how they choose their next direction.

## 10.4 Ghost Behaviour (FR-014)

At each move, a ghost looks at the non-wall neighbours of its cell (excluding the reverse direction) and steps to the one whose **Manhattan distance to its target cell** is smallest. Ties are broken in the fixed order **Up, Left, Down, Right**. Only the *target cell* differs per ghost and per mode.

**Chase-mode targets:**

| Ghost | Target |
|---|---|
| **Blinky** (direct) | Pacman's current cell. |
| **Pinky** (ambush) | The cell 2 ahead of Pacman in his current direction. |
| **Inky** (flank) | Take the cell 1 ahead of Pacman, then double the vector from Blinky to that cell (target = point + (point − Blinky)). |
| **Clyde** (shy) | Pacman's cell when farther than 4 cells away; otherwise his own scatter corner. |

**Scatter mode:** every ghost targets its own fixed corner (each ghost is assigned one of the four maze corners).

**Scatter/Chase schedule** *(tunable)*: the ghosts alternate **Scatter 5 s → Chase 20 s**, repeated **twice**, then **Chase permanently**. A mode change lets a ghost reverse once.

## 10.5 Power Pellets & Frightened Mode (FR-017..FR-020)

- Eating a power pellet puts all ghosts into **Frightened** mode for a **per-level duration** (§10.9 — 6 s on level 1, shrinking to 0 s on level 5, where a power pellet then only scores points): they move at half speed (§10.1) and step *away* from Pacman (choose the valid non-reverse neighbour with the **largest** Manhattan distance to Pacman); they are drawn in a visibly frightened style (FR-018).
- Pacman entering a frightened ghost's cell **eats** it: the ghost returns to the pen and resumes normal behaviour from there; Pacman scores the ghost bonus (§10.6). Eating a ghost does not cost a life.
- Frightened mode ends when its timer expires (FR-020); the last ~1 s may blink as a warning *(optional)*. Eating another power pellet restarts the timer and resets the ghost-bonus chain.

## 10.6 Scoring (fills A-006)

| Event | Points |
|---|---|
| Pellet | 10 |
| Power pellet | 50 |
| Ghost eaten (1st / 2nd / 3rd / 4th in one frightened period) | 200 / 400 / 800 / 1600 |

No bonus fruit (out of scope, [01 §1.2](01-System-Overview-and-Context.md#12-system-boundary)). No level-clear bonus. All values *(tunable)*.

## 10.7 Collisions & End of Game

Resolved each tick, after movement:

- **Caught:** a **non-frightened** ghost sharing Pacman's cell — or swapping cells with him in the same tick (passing through each other) — catches Pacman. This costs one life (FR-024). If lives remain, Pacman and the ghosts reset to their level start positions and the level continues; if lives reach zero, it is **game over** (FR-007).
- **Eaten ghost:** a **frightened** ghost in the same situation is eaten (§10.5).
- **Level clear:** no pellets remain in the current maze → if it is **not** the final level, advance to the next level — load its maze and difficulty (§10.9), reset entity positions, keep the accumulated score and remaining lives (FR-021); if it **is** the final (5th) level, the game is **won** (FR-027).

When the run ends — either game over (FR-007) or the final level cleared (FR-027) — the final score is shown on its own screen for **2 s** *(tunable)* before returning to the menu (FR-023) and is offered for the high-score check (FR-008).

## 10.8 Tunable Constants (defaults)

Collected for convenience — these realise [A-006](05-Risks-Assumptions-and-Dependencies.md#52-assumptions):

| Constant | Default |
|---|---|
| Pacman movement period | 150 ms (all levels) |
| Starting lives | 3 |
| Number of levels | 5 |
| Ghost speed / frightened duration / scatter schedule | per level — see §10.9 |
| Frightened ghost speed | half of the ghost's current speed |
| Pellet / power-pellet points | 10 / 50 |
| Ghost-eaten points | 200 / 400 / 800 / 1600 |
| Render rate | ≥ 60 FPS (NFR-002) |

## 10.9 Levels & Difficulty (FR-025 / FR-026)

Five levels, each with its own maze (§10.2) and its own difficulty. Pacman's speed stays constant (150 ms/cell); the ghosts get faster, the frightened window shrinks, and scatter time falls away — so level 1 is comfortable and level 5 is almost unwinnable. Score is cumulative and lives (FR-006) carry across all levels; the run ends only on game over (all lives lost, FR-007) or after clearing level 5 (FR-027).

| Level | Maze | Ghost move period | Frightened duration | Scatter / Chase |
|---|---|---|---|---|
| 1 | maze 1 | 200 ms (slower than Pacman) | 6 s | (Scatter 5 s, Chase 20 s) × 2, then Chase |
| 2 | maze 2 | 170 ms | 5 s | (Scatter 4 s, Chase 20 s) × 2, then Chase |
| 3 | maze 3 | 150 ms (= Pacman) | 4 s | (Scatter 3 s, Chase 20 s), then Chase |
| 4 | maze 4 | 130 ms (faster) | 2 s | Scatter 2 s once, then Chase |
| 5 | maze 5 | 110 ms (faster) | 0 s (no frightened) | Chase only (no scatter) |

All values *(tunable)*. Clearing a level keeps the score and lives and loads the next row; clearing level 5 wins the game.
