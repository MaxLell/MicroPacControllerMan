# Training campaign

FR-037 asks for **4,600** points on the normal maze — the only maze the AI
may be handed control in (FR-040) — and for more than a uniform-random policy
on the same maze. Both figures are the mean of twenty runs on the same twenty
draws: the game's timings are jittered (FR-044), so a run is a draw rather than
a measurement. Every figure below comes out of `Training/evaluate.py`, which
measures both policies in one run so the comparison cannot drift.

| run | what | budget | score | vs. random | factor | nodes | conns | gen | FR-037 |
|---|---|---|---|---|---|---|---|---|---|
| `shipped` | the deterministic-game winner the firmware ships today — 4,980 on one fixed episode | — | 2827.5 | 518.0 | 5.5x | 43 | 432 | 2125 | not met |
| `arcade-danger` | the shipped recipe — 23-16-4, one life, ten points a dangerous decision | 5.70 h | 3531.0 | 518.0 | 6.8x | 43 | 432 | 4049 | not met |
| `arcade-danger-wide` | the same again with 32 hidden units, to see whether 16 was the ceiling | 5.70 h | 2634.0 | 518.0 | 5.1x | 59 | 864 | 4179 | not met |

## What each run reported

### shipped — the deterministic-game winner the firmware ships today — 4,980 on one fixed episode

```
stage 3, normal maze, 43 nodes (16 hidden), 432 connections, digest ec66ee57464204c2

trained
  scores  [3130, 3620, 3110, 2280, 3070, 2190, 1930, 3210, 3350, 2580, 2560, 2630, 2200, 3770, 2970, 2380, 2930, 2010, 2730, 3900]
  mean    2827.5   median 2830   min 1930   max 3900
  levels  reached up to 1   decisions 260..416   ghosts eaten 36

uniform random
  scores  [820, 420, 770, 380, 450, 670, 730, 200, 400, 750, 550, 570, 360, 280, 410, 480, 580, 810, 400, 330]
  mean    518.0   median 465   min 200   max 820
  levels  reached up to 1   decisions 152..274   ghosts eaten 2

FR-037: 2827.5 vs. required 4600 — NOT met
         2827.5 vs. random 518.0 (5.5x) — met
```

### arcade-danger — the shipped recipe — 23-16-4, one life, ten points a dangerous decision

```
stage 3, normal maze, 43 nodes (16 hidden), 432 connections, digest 41cc70f5ce88b97e

trained
  scores  [3760, 3570, 4160, 1100, 1120, 3270, 1500, 3430, 7690, 4480, 4170, 7580, 4160, 3260, 1430, 1100, 5400, 3350, 2600, 3490]
  mean    3531.0   median 3460   min 1100   max 7690
  levels  reached up to 1   decisions 129..532   ghosts eaten 77

uniform random
  scores  [820, 420, 770, 380, 450, 670, 730, 200, 400, 750, 550, 570, 360, 280, 410, 480, 580, 810, 400, 330]
  mean    518.0   median 465   min 200   max 820
  levels  reached up to 1   decisions 152..274   ghosts eaten 2

FR-037: 3531.0 vs. required 4600 — NOT met
         3531.0 vs. random 518.0 (6.8x) — met
```

### arcade-danger-wide — the same again with 32 hidden units, to see whether 16 was the ceiling

```
stage 3, normal maze, 59 nodes (32 hidden), 864 connections, digest e3f1b399b3890e57

trained
  scores  [3110, 1840, 2150, 2250, 2150, 2430, 1340, 4030, 3510, 2650, 1990, 3100, 3260, 2800, 3300, 3020, 2540, 3820, 900, 2490]
  mean    2634.0   median 2595   min 900   max 4030
  levels  reached up to 1   decisions 129..456   ghosts eaten 54

uniform random
  scores  [820, 420, 770, 380, 450, 670, 730, 200, 400, 750, 550, 570, 360, 280, 410, 480, 580, 810, 400, 330]
  mean    518.0   median 465   min 200   max 820
  levels  reached up to 1   decisions 152..274   ghosts eaten 2

FR-037: 2634.0 vs. required 4600 — NOT met
         2634.0 vs. random 518.0 (5.1x) — met
```

## Next

If the best of these is still short of 4,600, the order to look in is the one
[M6 §14](../../Docu/Design/M6-Pacman-AI.md) sets out: the 23 features first, then the
expectimax reference agent as a teacher. Not the threshold — that is the owner's to
move.
