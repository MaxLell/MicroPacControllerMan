# Training campaign

FR-037 asks for **4,600** points on the normal maze — the only maze the AI
may be handed control in (FR-040) — and for more than a uniform-random policy
on the same maze. Both figures are the mean of twenty runs on the same twenty
draws: the game's timings are jittered (FR-044), so a run is a draw rather than
a measurement. Every figure below comes out of `Training/evaluate.py`, which
measures both policies in one run so the comparison cannot drift.

| run | what | score | vs. random | factor | nodes | conns | gen | FR-037 |
|---|---|---|---|---|---|---|---|---|
| `shipped` | the deterministic-game winner the firmware ships today — 4,980 on one fixed episode | 2197.0 | 424.5 | 5.2x | 35 | 19 | 276 | not met |
| `normal-seed1` | the jittered game, one life per episode, a bonus per ghost (FR-036/FR-044) | 1746.5 | 424.5 | 4.1x | 29 | 6 | 317 | not met |

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

### normal-seed1 — the jittered game, one life per episode, a bonus per ghost (FR-036/FR-044)

```
stage 3, normal maze, 29 nodes (2 hidden), 6 connections, digest 1450c949b06a81c3

trained
  scores  [2080, 2890, 1640, 1300, 1690, 2090, 1070, 2710, 2080, 1600, 1690, 2020, 1650, 2040, 850, 1080, 2090, 840, 2090, 1430]
  mean    1746.5   median 1690   min 840   max 2890
  levels  reached up to 1   decisions 176..346   ghosts eaten 25

uniform random
  scores  [640, 540, 550, 460, 410, 360, 240, 140, 350, 510, 280, 700, 360, 440, 470, 290, 300, 340, 440, 670]
  mean    424.5   median 425   min 140   max 700
  levels  reached up to 1   decisions 75..295   ghosts eaten 0

FR-037: 1746.5 vs. required 4600 — NOT met
         1746.5 vs. random 424.5 (4.1x) — met
```

## Next

If the best of these is still short of 4,600, the order to look in is the one
[M6 §14](../../Docu/Design/M6-Pacman-AI.md) sets out: the 23 features first, then the
expectimax reference agent as a teacher. Not the threshold — that is the owner's to
move.
