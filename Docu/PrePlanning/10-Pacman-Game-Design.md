# 10 Pacman Game Design

[← Back to Index](Index.md) · See also [02 Requirements](02-Requirements.md) · [03 Architecture](03-Architecture.md)

This document pins down the concrete game rules that [02 Requirements](02-Requirements.md) intentionally left open (it closes [A-005](05-Risks-Assumptions-and-Dependencies.md#52-assumptions) and fills in the tunable constants of [A-006](05-Risks-Assumptions-and-Dependencies.md#52-assumptions)). It is the input for **Milestone 3 (Pacman Development)** — enough detail to implement Model/Control and to write unit tests with expected values. Numbers marked *(tunable)* are starting defaults that may be adjusted for feel; the rules around them are fixed.

## 10.1 Movement & Tick Model

- **Grid-based.** The maze is a grid of square cells (§10.2). Pacman and each ghost occupy exactly one cell and face one of four directions (North / East / South / West). There is no sub-cell/pixel position — movement is one whole cell at a time. This keeps the logic deterministic and unit-testable, and matches tile rendering on the monochrome display.
- **Game step.** The game advances in discrete **steps** at a fixed cadence of **150 ms** *(tunable)*. Everything below (input application, movement, eating, collisions, timers) happens once per step. Rendering runs independently at ≥ 30 FPS (NFR-002) and simply redraws the current Model — a step just changes what the next frames show.
- **Pacman movement.** Pacman keeps a current direction and a *queued* direction. `MSG_INPUT_DIRECTION` sets the queued direction. Each step: if the queued direction is not blocked by a wall, it becomes the current direction; then Pacman moves one cell in the current direction if that cell is not a wall, otherwise he stays put (stopped against a wall until the direction changes).
- **Ghost movement.** Each step a ghost moves one cell toward its target cell (§10.4). A ghost never reverses onto the cell it just came from, except at the instant its mode changes (scatter↔chase, or entering/leaving frightened). Frightened ghosts move once every **2 steps** (half speed) *(tunable)*.
- **Tunnel.** Leaving the maze through a tunnel mouth (§10.2) re-enters at the opposite mouth on the same row (FR-012).

## 10.2 The Maze (Playfield)

The reference maze is an **11 × 9** grid (11 columns, 9 rows), rendered at a tile size of **10 × 10 px** (≈ 110 × 90 px), centred, with the remaining space used for the HUD (score + high score). It is left-right symmetric, fully connected, with one horizontal tunnel and a central ghost pen. Exact pixel geometry is finalised during M3; the grid below is fixed (FR-010, FR-022).

Legend: `#` wall · `.` pellet · `o` power pellet · `P` Pacman start · `G` ghost start (pen) · space = open, no pellet (tunnel mouths).

```
###########
#o..#.#..o#
#.#.#.#.#.#
#.........#
...#GGG#...
#.........#
#.#.#.#.#.#
#o..#P#..o#
###########
```

- **Pellets** occupy every open path cell except the pen cells, Pacman's start cell, and the two tunnel-mouth cells (row 5, far left/right).
- **Power pellets** (`o`) sit in the four corners.
- **Ghost pen** (`G`) is the three central cells; ghosts start here and return here when eaten. It is open above and below (no door in the reference layout).
- **Tunnel**: the middle row's left and right edges are open and wrap to each other.
- **Level clear (FR-021)** occurs when the last pellet and power pellet are eaten.

## 10.3 Entities

The Model (see [03 Architecture §3.1](03-Architecture.md#31-mvp-architecture-model--view--control)) holds:

- **Maze**: the static walls plus a dynamic map of which pellets/power pellets remain.
- **Pacman**: cell, current direction, queued direction, alive flag.
- **Four ghosts**, each: cell, direction, personality (Blinky / Pinky / Inky / Clyde), mode (scatter / chase), frightened flag + which "eaten" state, and its scatter-corner.
- **Score**, **lives** (always starts at 1, FR-006), **high score** (in-memory copy of NVM), the **frightened timer**, the **scatter/chase phase timer**, and **pellets remaining**.

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

- Eating a power pellet puts all ghosts into **Frightened** mode for **6 s** *(tunable)*: they move at half speed (§10.1) and step *away* from Pacman (choose the valid non-reverse neighbour with the **largest** Manhattan distance to Pacman); they are drawn in a visibly frightened style (FR-018).
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

Resolved once per step, after movement:

- **Caught (FR-007):** a **non-frightened** ghost sharing Pacman's cell — or swapping cells with him in the same step (passing through each other) — catches Pacman. With a single life (FR-006) the game ends and returns to the menu.
- **Eaten ghost:** a **frightened** ghost in the same situation is eaten (§10.5).
- **Level clear (FR-021):** no pellets remain → the game ends as won and returns to the menu.

In both end cases the final score is offered for the high-score check (FR-008).

## 10.8 Tunable Constants (defaults)

Collected for convenience — these realise [A-006](05-Risks-Assumptions-and-Dependencies.md#52-assumptions):

| Constant | Default |
|---|---|
| Game step period | 150 ms |
| Frightened duration | 6 s |
| Frightened ghost speed | 1 cell / 2 steps |
| Scatter / Chase schedule | (Scatter 5 s, Chase 20 s) × 2, then Chase |
| Pellet / power-pellet points | 10 / 50 |
| Ghost-eaten points | 200 / 400 / 800 / 1600 |
| Render rate | ≥ 30 FPS (NFR-002) |
