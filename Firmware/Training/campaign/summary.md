# Training campaign

FR-037 asks for **4,600** points on the normal maze — the only maze the AI
may be handed control in (FR-040) — and for more than a uniform-random policy
on the same maze. Both figures are the mean of twenty runs on the same twenty
draws: the game's timings are jittered (FR-044), so a run is a draw rather than
a measurement. Every figure below comes out of `Training/evaluate.py`, which
measures both policies in one run so the comparison cannot drift.

| run | what | budget | score | vs. random | factor | nodes | conns | gen | FR-037 |
|---|---|---|---|---|---|---|---|---|---|
| `shipped` | the deterministic-game winner the firmware ships today — 4,980 on one fixed episode | — | 2197.0 | 424.5 | 5.2x | 35 | 19 | 276 | not met |
| `neat-no-deletion` | NEAT, but forbidden to remove nodes or connections | — | 2360.0 | 424.5 | 5.6x | 40 | 22 | 360 | not met |
| `es-one-life` | fixed 23-16-4 network, an episode ends at the first death | 0.95 h | 2353.0 | 424.5 | 5.5x | 43 | 432 | 1748 | not met |

A budget of **1.00 h** could not pay for 2 of the configured runs, so they were not started: `es-whole-run` (needs 0.75 h), `es-whole-run-wide` (needs 0.75 h). A run too short to reach stage 3 answers a different question,
so it is dropped rather than shortened.

## What each run reported

### shipped — the deterministic-game winner the firmware ships today — 4,980 on one fixed episode

```
stage 3, normal maze, 35 nodes (8 hidden), 19 connections, digest a082e6ea61e8f6fa

trained
  scores  [1850, 2240, 2250, 1960, 2480, 2550, 1680, 1810, 2500, 2290, 2650, 1860, 3130, 1810, 2260, 1840, 2010, 2210, 2490, 2070]
  mean    2197.0   median 2225   min 1680   max 3130
  levels  reached up to 1   decisions 214..366   ghosts eaten 6

uniform random
  scores  [640, 540, 550, 460, 410, 360, 240, 140, 350, 510, 280, 700, 360, 440, 470, 290, 300, 340, 440, 670]
  mean    424.5   median 425   min 140   max 700
  levels  reached up to 1   decisions 75..295   ghosts eaten 0

FR-037: 2197.0 vs. required 4600 — NOT met
         2197.0 vs. random 424.5 (5.2x) — met
```

### neat-no-deletion — NEAT, but forbidden to remove nodes or connections

```
stage 3, normal maze, 40 nodes (13 hidden), 22 connections, digest 417cdaed53f62e16

trained
  scores  [2030, 2480, 2820, 2420, 1740, 1510, 2410, 2220, 2480, 2760, 2560, 2480, 2630, 2480, 2270, 2260, 2470, 2230, 2480, 2470]
  mean    2360.0   median 2470   min 1510   max 2820
  levels  reached up to 1   decisions 230..413   ghosts eaten 19

uniform random
  scores  [640, 540, 550, 460, 410, 360, 240, 140, 350, 510, 280, 700, 360, 440, 470, 290, 300, 340, 440, 670]
  mean    424.5   median 425   min 140   max 700
  levels  reached up to 1   decisions 75..295   ghosts eaten 0

FR-037: 2360.0 vs. required 4600 — NOT met
         2360.0 vs. random 424.5 (5.6x) — met
```

### es-one-life — fixed 23-16-4 network, an episode ends at the first death

```
stage 3, normal maze, 43 nodes (16 hidden), 432 connections, digest d362304b2970ea24

trained
  scores  [3130, 2690, 2620, 2210, 2020, 1970, 1690, 1690, 4050, 1970, 1970, 3130, 1970, 2620, 1950, 820, 3620, 1670, 3270, 2000]
  mean    2353.0   median 2010   min 820   max 4050
  levels  reached up to 1   decisions 199..428   ghosts eaten 47

uniform random
  scores  [640, 540, 550, 460, 410, 360, 240, 140, 350, 510, 280, 700, 360, 440, 470, 290, 300, 340, 440, 670]
  mean    424.5   median 425   min 140   max 700
  levels  reached up to 1   decisions 75..295   ghosts eaten 0

FR-037: 2353.0 vs. required 4600 — NOT met
         2353.0 vs. random 424.5 (5.5x) — met
```

## Next

If the best of these is still short of 4,600, the order to look in is the one
[M6 §14](../../Docu/Design/M6-Pacman-AI.md) sets out: the 23 features first, then the
expectimax reference agent as a teacher. Not the threshold — that is the owner's to
move.
