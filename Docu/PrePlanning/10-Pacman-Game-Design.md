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
- **Simulation tick.** The game runs on a fixed, fine simulation tick. Each moving entity has a **movement period** and advances one cell when its period elapses; pellet-eating, collisions and mode timers are evaluated every tick. Rendering runs independently at ≥ 60 FPS, interpolating between steps as above.
- **Pacman movement.** Pacman moves one cell every movement period, which is **per level and not constant within one** (§10.9): he is quicker while the ghosts are frightened and slower on the step after eating a pellet. He keeps a current direction and a *queued* direction; `MSG_INPUT_DIRECTION` sets the queued one. When he is due to move: if the queued direction is not blocked by a wall it becomes current; then he moves one cell if that cell is open, otherwise he stays put (stopped against a wall until the direction changes).
- **Ghost movement.** A ghost moves one cell toward its target (§10.4) every ghost-movement-period. **Each ghost has its own**, because the period depends on more than the level (§10.9): a ghost in the tunnel crawls, a frightened one is slow, and Blinky speeds up as the maze empties. A ghost never reverses onto the cell it just left, except when its mode changes (scatter↔chase, or entering/leaving frightened).
- **Tunnels.** Leaving the maze through a tunnel mouth (§10.2) re-enters at the opposite mouth, on the same row (FR-012).

## 10.2 The Maze (Playfield)

**Every level plays its own generated maze** (FR-029), on a 28 × 31 grid at 8 × 8 px per cell (FR-022) — 224 × 248 px of a 240 × 320 panel, centred, with three rows above and below for the HUD. How one is generated is [M4 Random Mazes](../Design/M4-Random-Mazes.md); what one is guaranteed to be true of is FR-029.

The arcade's own layout is below and is **still in the code**, as the reference: it is the one maze whose properties are known from outside this codebase — 244 pellets, the corridors the Dossier's ghost behaviour is described against, the hand-drawn tile map the appearance rules are checked against. It is what the generated mazes are judged by, and what a unit test plays when it needs a corridor it can name.

What every maze shares, generated or not, is the furniture: the ghost house and its gate, the four ghost starting cells and Pacman's, all at the coordinates below. That is what lets §10.4's release order, §10.5's revival and the scatter targets mean the same thing in a maze nobody has seen.

This has been re-baselined three times, and each step is worth recording. It began as an 11 × 9 reduction with five of them, one per level: the reduction was right when the panel was 128 px wide, and the five layouts were a way of making levels differ before §10.9 had a better one. It was then re-drawn at 28 × 31 by hand, to the right proportions — and 94 of its 868 cells still did not match the arcade. What settled it was wanting the arcade's wall tiles to draw with, because a tile map only fits the maze it was drawn for. The layout below is therefore transcribed, not authored. And then it stopped being what a level plays at all, because the owner asked for mazes that are generated ([DEC-029](11-Decisions-and-As-Built.md)).

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

At each move a ghost takes the first step of the **shortest route to the reachable cell nearest its target**, never turning round (§10.1). Distance is the **straight line**, compared as a square because that is what the arcade compares and it keeps the ordering exact. Ties — equal routes to equally near cells — are broken in the fixed order **Up, Left, Down, Right**. Only the *target* differs per ghost and per mode.

The arcade itself looks only one cell ahead and takes the neighbour nearest the target. This searches the route instead, at the owner's request; where the target is reachable the two agree, and where it is not — which is most of the time for Pinky and Inky, whose targets land in walls or off the board — the search heads for the nearest cell that exists. Fleeing stays one-cell greedy: there is no destination to plan a route to.

**Chase-mode targets:**

| Ghost | Target |
|---|---|
| **Blinky** (direct) | Pacman's current cell. |
| **Pinky** (ambush) | The cell 4 ahead of Pacman in his current direction. |
| **Inky** (flank) | Take the cell 2 ahead of Pacman, then double the vector from Blinky to that cell (target = point + (point − Blinky)). |
| **Clyde** (shy) | Pacman's cell when eight cells away or more; otherwise his own scatter tile. |

**Scatter mode:** every ghost aims at its own fixed tile — **Blinky top-right, Pinky top-left, Inky bottom-right, Clyde bottom-left** — except a Blinky who has become Cruise Elroy, who keeps chasing (§10.9).

Those tiles sit in the **dead space above and below the maze and cannot be reached**, and that is the mechanism rather than an oversight. A ghost walks to the corner nearest its target and then, forbidden from turning round, circles it until the mode changes. As the Dossier puts it: *"the only reason a ghost has a favourite corner of the maze at all is due to the location of a fixed target tile it will never reach."* Aim it at a real cell instead and it arrives, and stops being interesting.

Clyde's shy rule reuses the same tile: when Pacman is within eight cells, that is what he heads for.

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
| Render rate | ≥ 60 FPS *(a design figure; the requirement that asked for it is withdrawn — [DEC-036](11-Decisions-and-As-Built.md))* |

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

## 10.10 The HUD (FR-022)

The maze is 224 × 248 px of a 240 × 320 panel, which leaves three rows of cells above it and six below (§10.2). The HUD lives there, arranged as the arcade arranges it: a label on the first row and its value on the second, the score at the left, and the lives as little Pacmans along the bottom.

```
   1UP                LEVEL
      1240                7
  ┌────────────────────────┐
  │                        │
  │         the maze       │
  │                        │
  └────────────────────────┘
  ᗧ ᗧ ᗧ
```

- **Score**, seven digits, right-aligned so the units sit in a fixed place and the number grows leftward. Seven because a full run can pass a million. Leading zeros are drawn as blanks — but a score of nothing still shows a single `0`, or the row would read as a HUD that has not come up yet.
- **Level**, two digits, right-aligned at the other end. The arcade puts a fruit here; this game has none ([01 §1.2](01-System-Overview-and-Context.md#12-system-boundary)), so it puts the number.
- **Lives**, one Pacman per remaining life. The arcade shows the *reserve* men, one fewer than the count it is holding; this shows the count itself, so that what is on the panel and what `game_get_lives()` says can never disagree.
- **The alphabet** is the arcade's own font, from the same tile ROM as the walls. All of it is present although the HUD spells two words with it, because the screens FR-001, FR-002 and FR-023 still want are all text.

**How it reaches the panel.** The HUD is a *fixed-length list of slots* — three for `1UP`, seven for the score, five for `LEVEL`, two for the level, three for lives — and what each slot should show is a pure function of the game state. Asking the same question of the state last drawn says what is already on the panel, and the difference is exactly what has to be sent.

That matters because the score changes on almost every pellet. Re-sending seven digits each time would cost more of a frame than the five actors do; sending the one digit that moved costs nothing. A spent life is *painted over* rather than skipped — a slot that simply stopped being drawn would leave the last Pacman on the panel for the rest of the run.

## 10.11 The High Scores (FR-009)

Three scores, not one. A single number tells a player whether they were the best; a table tells them how close they came, and the cabinets this game is copied from all showed a list. Best first, and a score equal to one already there does not displace it — first to get there keeps the place.

A finished run is offered to the table **once, when it ends**. That is not tidiness: storing a score erases a flash page and stalls the CPU for the milliseconds it takes, so offering the score as it climbed would be a visible stutter every few pellets. Measured on the board, an erase is about a millisecond and three writes about ten; a frame is sixteen.

**Every stored byte is suspect.** A page can be erased, half-programmed by a power cut mid-write, or left over from a different build of this firmware, and all three read back as numbers. A magic word says *this is a high-score table*, a version says *of this shape*, and a CRC-32 says *and it is intact*. Anything that fails those starts from an empty table, because showing a score nobody scored is worse than showing none.

Clearing the table is a **console command**, `highscore reset`, rather than a menu entry. The reason to clear it is almost always a developer's — a score set while testing sits in the top three for good, and there is no way to play a worse game than one that has already happened. `highscore` on its own prints the three.

**Where they live.** `Bsp/flash_bsp` owns one 8 KB page, and the *linker* reserves it: the `FLASH` region is one page shorter than the part and the page beyond it is its own region, so a firmware that grew into it fails to link rather than erasing part of itself the first time a score is saved. The driver offers read and **replace** rather than write, because flash erases whole pages and can only clear bits — offering a partial write would be a promise the hardware cannot keep.

The host build has the same store behind a file next to the executable, so scores survive between runs there too. That is not a convenience: the property being stood in for is *surviving a power cycle*, and an array in a process that exits stands in for nothing.
