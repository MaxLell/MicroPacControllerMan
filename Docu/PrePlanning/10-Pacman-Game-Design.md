# 10 Pacman Game Design

[← Back to Index](Index.md) · See also [02 Requirements](02-Requirements.md) · [03 Architecture](03-Architecture.md)

This document pins down the concrete game rules that [02 Requirements](02-Requirements.md) intentionally left open (it closes [A-005](05-Risks-Assumptions-and-Dependencies.md#52-assumptions) and fills in the tunable constants of [A-006](05-Risks-Assumptions-and-Dependencies.md#52-assumptions)). It is the input for **Milestone 3 (Pacman Development)** — enough detail to implement Model/Control and to write unit tests with expected values. Numbers marked *(tunable)* are starting defaults that may be adjusted for feel; the rules around them are fixed.

## 10.1 Movement & Tick Model

- **Grid-based.** The maze is a grid of square cells (§10.2). Pacman and each ghost occupy exactly one cell and face one of four directions (North / East / South / West). Movement is one whole cell at a time, which keeps collisions, pellet-eating and ghost targeting deterministic and unit-testable.
- **But the *view* moves in pixels.** A cell is 8 px on the 240 x 320 panel and Pacman crosses one in about 167 ms, so cell-granular drawing means an **8-pixel jump six times a second** — visibly choppy, and no display refresh rate can rescue it. This was measured rather than guessed: the frame-rate ladder of [M2 Board Bring-Up §3.2](../Design/M2-Board-Bring-Up.md) shows 9 px per frame already reads as stutter. So each moving entity additionally carries **how far into its current cell it has come from the one before it**, and the Game-View interpolates ([03 §3.6](03-Architecture.md#36-pacman-sub-application-architecture)). At 60 FPS that is well under a pixel per frame, the smooth end of the same ladder.
  - **Backwards, not forwards, and that is the whole of it.** The fraction measures the step already taken. Measuring the step to come instead means interpolating towards a cell that has not been chosen yet, and the guess is wrong exactly where it is most visible — at a corner, where the entity slides a cell the way it came from and then snaps sideways, or, with a wall ahead, stands still for a whole period and then jumps. Measured backwards a corner is two straight runs that meet on the corner cell, with nothing to guess.
  - An entity that did not move reports **arrived** rather than a running fraction. One stopped against a wall keeps its facing and its timer keeps running, and a fraction would have it slide in from behind, on the spot, once per period.
  - The rules do not see any of this. Interpolation is presentation: an entity *is* in its current cell for every purpose the game logic cares about, and the fraction is written by the movement timer and read only by the view.
  - The earlier wording here said there is no sub-cell position at all, justified by "tile rendering on the monochrome display". That display is gone, and with it the reason.
- **Simulation tick.** The game runs on a fixed, fine simulation tick. Each moving entity has a **movement period** and advances one cell when its period elapses; pellet-eating, collisions and mode timers are evaluated every tick. Rendering runs independently at ≥ 60 FPS (NFR-002), interpolating between steps as above.
- **Pacman movement.** Pacman moves one cell every movement period, which is **per level and not constant within one** (§10.9): he is quicker while the ghosts are frightened and slower on the step after eating a pellet. He keeps a current direction and a *queued* direction; `MSG_INPUT_DIRECTION` sets the queued one. When he is due to move: if the queued direction is not blocked by a wall it becomes current; then he moves one cell if that cell is open, otherwise he stays put (stopped against a wall until the direction changes).
- **Ghost movement.** A ghost moves one cell toward its target (§10.4) every ghost-movement-period. **Each ghost has its own**, because the period depends on more than the level (§10.9): a ghost in the tunnel crawls, a frightened one is slow, and Blinky speeds up as the maze empties. A ghost never reverses onto the cell it just left, except when its mode changes (scatter↔chase, or entering/leaving frightened).
- **Tunnels.** Leaving the maze through a tunnel mouth (§10.2) re-enters at the opposite mouth, on the same row (FR-012).

## 10.2 The Maze (Playfield)

There is **one maze and every level plays it** (FR-025): the arcade's own, a 28 × 31 grid at 8 × 8 px per cell (FR-022) — 224 × 248 px of a 240 × 320 panel, centred, with three rows above and below for the HUD.

This has been re-baselined twice, and both steps are worth recording. It began as an 11 × 9 reduction with five of them, one per level: the reduction was right when the panel was 128 px wide, and the five layouts were a way of making levels differ before §10.9 had a better one. It was then re-drawn at 28 × 31 by hand, to the right proportions — and 94 of its 868 cells still did not match the arcade. What settled it was wanting the arcade's wall tiles to draw with, because a tile map only fits the maze it was drawn for. The layout below is therefore transcribed, not authored.

Legend: `#` wall · `.` pellet · `o` power pellet · `P` Pacman start · `G` ghost start (pen) · `T` tunnel · space = open, nothing on it.

```
############################
#............##............#
#.####.#####.##.#####.####.#
#o####.#####.##.#####.####o#
#.####.#####.##.#####.####.#
#..........................#
#.####.##.########.##.####.#
#.####.##.########.##.####.#
#......##....##....##......#
######.##### ## #####.######
######.##### ## #####.######
######.##          ##.######
######.## ###  ### ##.######
######.## #      # ##.######
TTTTTT.   # GGGG #   .TTTTTT
######.## #      # ##.######
######.## ######## ##.######
######.##          ##.######
######.## ######## ##.######
######.## ######## ##.######
#............##............#
#.####.#####.##.#####.####.#
#.####.#####.##.#####.####.#
#o..##.......P .......##..o#
###.##.##.########.##.##.###
###.##.##.########.##.##.###
#......##....##....##......#
#.##########.##.##########.#
#.##########.##.##########.#
#..........................#
############################
```

- **Pellets** occupy every open path cell except the ghost house, Pacman's start cell and the tunnel. **244 in total — 240 plus four power pellets, exactly the arcade's count.** That matters rather than being a nicety: §10.9's Cruise Elroy thresholds are absolute pellet counts taken from the arcade, and they only mean what they meant there against this number. A unit test pins it, so re-drawing the maze cannot quietly change the difficulty.
- **Power pellets** (`o`) sit one row in from each corner, four of them.
- **Ghost pen** (`G`) is four cells of the ghost house, one per ghost. The house is open upward through a two-cell **gate**, which the rules treat as ordinary open cells — the arcade's one-way rule that keeps Pacman out is not modelled.
- **Tunnel** (`T`): row 14's left and right edges wrap to each other (FR-012). It is marked cell by cell rather than derived from the row, because a ghost is slowed *inside* the tunnel and not merely on its row (§10.9) — the junction the tunnel opens onto is ordinary maze. It is the only place in the maze where Pacman can reliably shake a ghost off.
- **No dead ends.** Every open cell has at least two open neighbours, so there is nowhere to be cornered by geometry alone. This is a property of the arcade layout rather than a rule of ours, and it is asserted rather than assumed.
- **Level clear** occurs when the last pellet and power pellet are eaten — see §10.7 / §10.9 for what happens next.

### How it is drawn

The map above is what the **rules** see. What the **panel** sees is a second map, in Game-View, holding one of the arcade's thirty-odd wall pieces per cell — the line and corner tiles that make the walls thin blue outlines rather than filled blocks, plus the pink bar across the ghost house gate.

The two cannot be derived from each other: a wall bitmap does not say which corner piece belongs in a cell, and a corner piece does not say whether a ghost may stand there. So the maze is written down twice, from the same source, and a unit test checks every cell agrees — no wall drawn where an entity may walk, and none missing where it may not. The gate is the single deliberate exception: drawn, but passable.

## 10.3 Entities

The Model (see [03 Architecture §3.1](03-Architecture.md#31-mvp-architecture-model--view--control)) holds:

- **Maze**: the static walls plus a dynamic map of which pellets/power pellets remain.
- **Pacman**: cell, current direction, queued direction, alive flag.
- **Four ghosts**, each: cell, direction, personality (Blinky / Pinky / Inky / Clyde), mode (scatter / chase), frightened flag + which "eaten" state, and its scatter-corner.
- **Score** (cumulative across levels), **lives** (starting count per FR-006, default 3), **current level** (1–21), **high score** (in-memory copy of NVM), the **frightened timer**, the **scatter/chase phase timer**, and **pellets remaining** — which is not only the level-clear condition but what wakes Cruise Elroy (§10.9).

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

**Scatter mode:** every ghost targets its own fixed corner (each ghost is assigned one of the four maze corners) — except a Blinky who has become Cruise Elroy, who keeps chasing (§10.9).

**Scatter/Chase schedule:** per level, in §10.9. A mode change lets a ghost reverse once.

## 10.5 Power Pellets & Frightened Mode (FR-017..FR-020)

- Eating a power pellet puts all ghosts into **Frightened** mode for a **per-level duration** (§10.9 — 6 s on level 1, down to 1 s by level 9, and 0 s at level 17 and from 19 on, where a power pellet then only scores points): they slow down and step *away* from Pacman (choose the valid non-reverse neighbour with the **largest** Manhattan distance to Pacman); they are drawn in a visibly frightened style (FR-018).
- Pacman entering a frightened ghost's cell **eats** it: the ghost returns to the pen and resumes normal behaviour from there; Pacman scores the ghost bonus (§10.6). Eating a ghost does not cost a life.
- Frightened mode ends when its timer expires (FR-020), and the ghosts **flash a warning** before it does — for `flashes × 2 × 250 ms`, per §10.9. Eating another power pellet restarts the timer and resets the ghost-bonus chain.

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
- **Level clear:** no pellets remain → if it is **not** the final level, advance to the next — restore the pellets, apply that level's difficulty (§10.9), reset entity positions, keep the accumulated score and remaining lives (FR-021); if it **is** the final (21st) level, the game is **won** (FR-027).

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

**Twenty-one levels on one maze** (§10.2), differing only in how they play. This is the arcade's own progression, transcribed rather than invented, and it replaces the five hand-authored mazes that used to carry the difficulty. Score is cumulative and lives (FR-006) carry across all levels; the run ends on game over (FR-007) or after clearing level 21 (FR-027).

**Why 21.** The arcade has no last level — its table simply stops changing after 21 and it then runs until the player is out of lives. So 21 is the honest finish line: clearing it means the whole curve has been walked, and level 22 would be level 21 with a different number on it.

### Speeds

Every speed is a **percentage of full speed**, which is one pixel per frame at 60 Hz across 8-pixel cells — 7.5 cells a second, or **133 ms per cell**. Keeping the table in percentages is deliberate: it is how the source states them, so the transcription can be checked line by line.

| Level | Pacman | Pacman eating | Pacman frightened | Ghost | Ghost tunnel | Ghost frightened | Elroy 1 at / speed | Elroy 2 at / speed | Frightened | Flashes |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 80 % | 71 % | 90 % | 75 % | 40 % | 50 % | 20 / 80 % | 10 / 85 % | 6 s | 5 |
| 2 | 90 % | 79 % | 95 % | 85 % | 45 % | 55 % | 30 / 90 % | 15 / 95 % | 5 s | 5 |
| 3 | 90 % | 79 % | 95 % | 85 % | 45 % | 55 % | 40 / 90 % | 20 / 95 % | 4 s | 5 |
| 4 | 90 % | 79 % | 95 % | 85 % | 45 % | 55 % | 40 / 90 % | 20 / 95 % | 3 s | 5 |
| 5 | 100 % | 87 % | 100 % | 95 % | 50 % | 60 % | 40 / 100 % | 20 / 105 % | 2 s | 5 |
| 6 | 100 % | 87 % | 100 % | 95 % | 50 % | 60 % | 50 / 100 % | 25 / 105 % | 5 s | 5 |
| 7–8 | 100 % | 87 % | 100 % | 95 % | 50 % | 60 % | 50 / 100 % | 25 / 105 % | 2 s | 5 |
| 9 | 100 % | 87 % | 100 % | 95 % | 50 % | 60 % | 60 / 100 % | 30 / 105 % | 1 s | 3 |
| 10 | 100 % | 87 % | 100 % | 95 % | 50 % | 60 % | 60 / 100 % | 30 / 105 % | 5 s | 5 |
| 11 | 100 % | 87 % | 100 % | 95 % | 50 % | 60 % | 60 / 100 % | 30 / 105 % | 2 s | 5 |
| 12–13 | 100 % | 87 % | 100 % | 95 % | 50 % | 60 % | 80 / 100 % | 40 / 105 % | 1 s | 3 |
| 14 | 100 % | 87 % | 100 % | 95 % | 50 % | 60 % | 80 / 100 % | 40 / 105 % | 3 s | 5 |
| 15–16 | 100 % | 87 % | 100 % | 95 % | 50 % | 60 % | 100 / 100 % | 50 / 105 % | 1 s | 3 |
| 17 | 100 % | 87 % | — | 95 % | 50 % | — | 100 / 100 % | 50 / 105 % | **0 s** | 0 |
| 18 | 100 % | 87 % | 100 % | 95 % | 50 % | 60 % | 100 / 100 % | 50 / 105 % | 1 s | 3 |
| 19–20 | 100 % | 87 % | — | 95 % | 50 % | — | 120 / 100 % | 60 / 105 % | **0 s** | 0 |
| 21 | **90 %** | 79 % | — | 95 % | 50 % | — | 120 / 100 % | 60 / 105 % | **0 s** | 0 |

Read as the shape of a run: levels 1–4 hand out speed, level 5 is where the frightened window collapses to two seconds, from 9 on it is mostly one second and Cruise Elroy wakes with a quarter of the maze still full, at 17 and from 19 on a power pellet stops frightening anyone at all, and 21 takes Pacman's own speed back down while leaving the ghosts where they are.

Four things in that table are worth stating outright, because each is easy to get wrong and none of them is visible from the numbers alone:

- **No ordinary ghost is ever faster than Pacman**, for twenty levels. They top out at 95 % and he reaches 100 %. What closes on him is the margin — comfortable on level 1, all but gone from level 5 — plus four of them at once. Level 21 is the single exception, and it gets there by slowing *him* down.
- **Cruise Elroy** is the other exception, and the heart of the progression. Blinky speeds up in two stages as the maze empties, at the pellet counts in the table, and at stage 2 from level 5 on he is **faster than Pacman**. He also **stops going home**: once awake he keeps chasing through the scatter phases the other three take off. That is what turns the last twenty pellets of a level from a mop-up into the dangerous part. The counts are absolute, and they mean what they meant in the arcade because the maze holds the same 244 pellets (§10.2).
- **The tunnel** halves a ghost's pace and is the one reliable escape. It applies to frightened ghosts too — it is a cap on that stretch, not a mode.
- **Pacman is slower while eating.** A full corridor costs him roughly a tenth of his speed; a cleared one does not. This is why clearing an escape route before going for a power pellet is a real tactic rather than a superstition.

The frightened window closes with a **warning**: the ghosts flash between their frightened colour and a white one for the last `flashes × 2 × 250 ms`. Which half of the flash it is on is decided by the game, not the view, so it is a fact a test can assert.

### Scatter / Chase

The plan alternates, always starting with scatter; when it runs out the ghosts chase for the rest of the level.

| Levels | Plan |
|---|---|
| 1 | Scatter 7 s, Chase 20 s, Scatter 7 s, Chase 20 s, Scatter 5 s, Chase 20 s, Scatter 5 s, then Chase |
| 2–4 | Scatter 7 s, Chase 20 s, Scatter 7 s, Chase 20 s, Scatter 5 s, Chase 1033 s, Scatter 1/60 s, then Chase |
| 5–21 | Scatter 5 s, Chase 20 s, Scatter 5 s, Chase 20 s, Scatter 5 s, Chase 1037 s, Scatter 1/60 s, then Chase |

The seventeen-minute chase and the single-frame scatter after it are quirks of the original and are transcribed as they are. Nobody will ever see that blip — a level is long over by then — but a tidied-up plan would be a different game, and the kind of difference that is impossible to notice later. A mode change lets a ghost reverse once; the frightened window freezes the plan rather than running it down in the background.

Clearing a level keeps the score and lives and applies the next row; clearing level 21 wins the game.
