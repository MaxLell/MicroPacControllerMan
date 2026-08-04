# M4 — Random Mazes

[← Back to Index](../PrePlanning/Index.md) · requirement: [FR-029](../PrePlanning/02-Requirements.md) ·
decisions: [DEC-029](../PrePlanning/11-Decisions-and-As-Built.md), DEC-030

The *how* for the milestone that replaced the arcade's one layout with a maze per level. The
requirement says only what a maze must be true of; this says which generator, why that one,
how the port was proved to be the same generator, and how a maze that nobody drew gets drawn.

## 1 The generator

The maze comes from **[shaunlebron/pacman-mazegen](https://github.com/shaunlebron/pacman-mazegen)**,
research from 2012 into generating mazes "aesthetically similar to those found in Pac-Man and
Ms. Pac-Man". That repository holds four attempts — a random fill, a Clingo answer-set
solver, a spanning tree, and **tetris-stacking**, which its own README calls the best. The
tetris one is what `App/maze_gen` implements, from `tetris/mapgen.js`.

It works in two phases:

1. **Stack pieces on a small grid.** A 9 × 5 grid — nine rows, five columns — is filled with
   tetris-like pieces, always growing from the leftmost empty column, each piece one to five
   cells, with heuristics that refuse the shapes that do not look like Pacman: no straight run
   of three, no pocket a later piece cannot fill, at most one long "L" or "T" per maze, one
   single-cell stub allowed against the top edge and one against the bottom.
2. **Upscale it by three and mirror it.** Each cell becomes 3 × 3 tiles, except that one row
   per column is made a tile taller and one column per row a tile narrower — which is what
   stops the result reading as a grid of identical boxes. The 9 × 5 grid becomes 31 rows of 14
   columns, mirrored to 28. **That is exactly `PLAYFIELD_HEIGHT` × `PLAYFIELD_WIDTH`**, and
   not by arrangement: the arcade maze is 28 × 31 because it was built the same way.

Between the two, the grid is judged and thrown away if it fails: a solid corner missing, two
stacked pairs that would upscale into a slab, no row that can be stretched, no column that can
be narrowed, no place to cut a tunnel mouth, or a tunnel that would run straight across the
maze. About three attempts are needed per maze; the worst of 2000 seeds needed 24. The port
caps the retries at 256 and asserts, because an unbounded loop on a target with no way out is
not a thing to ship — but reaching that cap would mean the generator can no longer satisfy its
own rules, which is a defect and not bad luck.

Mirroring is why only half a maze is ever generated, and it is also the reason the output reads
as *Pacman's* rather than as a maze: the arcade's are symmetric too.

### 1.1 Why the port is faithful rather than tidied

The port makes the same decisions in the same order and **draws the same random numbers**,
including where the original relies on a JavaScript accident. Three examples, all reproduced
and commented in `maze_gen.c` rather than corrected:

- `tallRows` and `narrowCols` are never reset between attempts, so a rejected grid leaves its
  choices behind and the next attempt inherits them. Cleared once per seed here, which is what
  makes one seed mean one maze, but **not** between attempts.
- Two of the energizer bounds are `subrows / 2` — 15.5. A range that starts on a half makes the
  original compute a fractional row and write nowhere, so no pellet is placed, but a random
  number is still consumed. Both halves of that matter and both are kept.
- The tile-cell array is indexed two to the left of its first column, which in JavaScript is a
  negative array index and works. Here the array carries the offset.

Correcting any of them would have produced a *different* generator whose output could no longer
be compared against anything. Which matters, because comparison is how the port was verified:

> A driver ran the original `mapgen.js` under node with `Math.random` replaced by the same
> xorshift32 the port uses, and printed the tile map for a seed. The C port printed its map for
> the same seed. **300 seeds, byte for byte identical.** The scripts are not in the repository
> — they need node and the upstream JavaScript — but the comparison is reproducible from this
> description in an afternoon, and the unit tests in `Test/Host/test_maze_gen.c` pin the
> properties that comparison was checking for.

### 1.2 What the game needs and the generator does not produce

`maze_gen` writes `playfield`'s own map legend, so the generator and the loader share a
contract (`PLAYFIELD_MAP_*` in `playfield.h`) instead of agreeing by eye. Three things are
stamped in afterwards, at the arcade's own coordinates:

| What | Where | Why it can be stamped |
|---|---|---|
| The ghost house interior and its gate | rows 13–15 × columns 11–16, gate at (13,12)/(14,12) | The generator's own grid puts its house **exactly** there — checked over 2000 seeds. To the generator the house is one more piece of wall, so its inside is blank; the game needs it to be a *place*. |
| The four ghost starting cells | Blinky (14,11) outside, Pinky (13,14), Inky (11,14), Clyde (16,14) inside | Same coordinates as the arcade, so the dot-counter release order, revival after being eaten and the rule that Pacman may never enter keep the meaning they were verified with ([10 §10.4](../PrePlanning/10-Pacman-Game-Design.md)). |
| Pacman's starting cell | (13,23) | The generator already clears the pellets there. |

**The tunnels are marked, not stamped**: the run of pellet-free corridor in from each edge,
which is exactly the stretch the generator clears and exactly what a ghost has to crawl
through. It is **shorter than the arcade's six cells** — usually one or two — because a
generated maze has no room for the arcade's side masses. The tunnel is still a way round and
still slows a ghost; it is a weaker escape than the arcade's.

The seed for a run is **the tick at the moment start was pressed**. Read as entropy and not as
a time — nothing is measured with it and no two values of it are compared — which is why it is
not the `millis()` the coding standard rules out. A player cannot press a key on a chosen
millisecond, so two runs differ; and a seed that is known replays exactly, which is why
`ott pacman` prints the one it used. Level *n* mixes the run seed with *n* rather than adding,
so consecutive levels are not neighbours in the generator's sequence.

## 2 Drawing a maze nobody drew

This is the harder half. The maze used to be written down **twice** — the rules in
`playfield.c`, the appearance as a hand-transcribed tile map in `game_view.c` — with a unit
test holding the two together, because a hand-drawn tile map genuinely cannot be derived from a
wall bitmap. Once the maze is generated there is nothing to hand-draw, so the appearance has to
be **derived**, and the duplication goes away with it.

The arcade's 33 wall tiles fall into two families, and the pixels say which is which:

- **The ring**: `MAZE_TOP`/`BOTTOM`/`LEFT`/`RIGHT`, four corners, six tees. Each draws a
  2-pixel bar hugging the tile's outer edge — the maze's boundary line.
- **A block**: `MAZE_BLOCK_*`. Twelve pieces — four convex corners, four edges, four concave
  corners — drawing a block's own outline **inset by five pixels**. That inset is why a
  two-cell-thick wall reads as Pacman's thin double line, and why the inside of a thick block is
  drawn as nothing at all.

So the rule is:

1. The ghost-house box (rows 12–16 × columns 10–17) is **stamped**: it is the same structure at
   the same place in every maze, and it owns the only tiles nothing else uses — the pink gate
   and four rounded corners.
2. A wall cell on the panel's edge is a **ring** cell. Its line runs straight, **ends** where a
   tunnel breaks the ring, or grows a branch inward where a block's line has to meet it. Which
   tee depends on whether the block starts or ends at that cell, because each of a two-cell
   wall's two parallel lines lands on its own tee.
3. A ring cell **beside a tunnel mouth** is where the boundary line stops, and it is drawn with
   the *block* family — see §2.3.
4. Any other wall cell carries part of its **block's** outline, chosen from which of its four
   neighbours are open — and, where none is, from which diagonal is, which is where the outline
   bends around an inside angle.
5. Everything else is corridor, and what is on it comes from the game state.

### 2.1 How that was checked

The arcade maze is the one case where the answer exists independently: its hand-drawn tile map,
transcribed from the 1980 game. So the derivation is run on the arcade's walls and compared
against it, cell by cell — now a unit test, with the old appearance map living in
`Test/Host/test_game_view.c` as the fixture:

> **764 of 764 cells outside the two tunnel masses match exactly**, including every concave
> corner and every tee.

The **64 cells inside the masses** (rows 9–19, six columns at each edge) do not, and the test
asserts that it is exactly 64 so the exclusion cannot quietly grow. There the arcade wall is
six cells thick and is drawn with the *ring* family, as a detour of the boundary rather than as
a block with an outline — a shape no generated maze has, since `maze_gen` builds a one-cell ring
with blocks inside it. The rule is right for every maze that will be played and wrong for those
64 cells of the fixture, which is the honest way round.

### 2.2 The tunnel mouths, which is where the first attempt looked wrong

The first version drew the cell above and below a tunnel mouth with a **frame corner** — the
tile that turns the boundary line inward. It looked wrong on the panel, and the owner said so:

> the "portals" — here the edges of the maze do not look tidy.

He was right, and the reason is worth keeping. A frame corner turns the line *away* from the
panel edge, and at a tunnel mouth there is corridor on that side, so the line turned inward and
**ended in mid-air** — a three-pixel stub pointing into the corridor, attached to nothing. The
arcade has no tile for the shape because it has no such shape: its tunnels are cut through a
six-cell-thick mass, never through a one-cell frame, so the boundary line there runs along the
mass's face and never has to stop.

What it needed is a line that stops *towards* the panel edge, and that is exactly what a block's
convex corner is. So a ring cell next to a tunnel mouth falls through to the block rule: the
boundary line comes down the frame and ends in a small cap at the edge, with the mouth as a clean
gap between two of them.

It also lands on the arcade's own tunnel proportions by accident, which is a good sign. A block
corner puts the line at the far side of its cell, a whole cell back from the corridor — and the
arcade pulls its tunnel walls back by exactly one cell too, drawing rows 13 and 15 with `MAZE_TOP`
and `MAZE_BOTTOM`. Both end up with a mouth that is wider than the corridor it serves.

### 2.3 The frame is six pixels thick, and that one is not the arcade's

Measured, because "looks thin" needs a number: the arcade's frame tile is a **2-pixel** line
inset 1 pixel from the panel edge, while a two-cell-thick wall inside the maze is two 3-pixel
block lines that meet — a **6-pixel** solid bar. The frame really is a third of the weight, and
the owner asked for them to match.

That is a departure from the ROM, and the ROM's reason is geometric: the frame is **one** cell
(8 px) thick where an inner wall is **two** (16 px). At 2 px a corner has room for the arcade's
diagonal step; at 6 px it has none, so the four screen corners become solid right angles. The
owner was shown that and chose 6 px anyway ([DEC-031](../PrePlanning/11-Decisions-and-As-Built.md)),
over a hollow double line (two 2-px lines, same span, curved corners) and over 3 px flush to the
edge (no new art, still thinner).

Sixteen tiles are affected — four edges, four corners, eight tees — and they are **generated from
the geometry** rather than drawn, so they are exactly symmetric and the tees line up with the
3-pixel branch an inner wall's line arrives on. The **ghost house keeps the arcade's 2-pixel
wall** and now has four tiles of its own for it; it used to share the frame's, and a house walled
like the panel border would have swallowed itself.

None of this touched the derivation: it picks the same tile *ids* it always did, which is why
§2.1's 764-of-764 comparison still holds — only those tiles' pixels are ours now. It did cost the
tunnel mouths a third attempt, though. §2.2's block cap is 3 pixels, and against a 6-pixel band
that leaves a visible step, so the band now simply **stops square** at the cell boundary where the
corridor begins. A 2-pixel line ending squarely looked like a stray line, which is what started
§2.2; a 6-pixel band ending squarely looks like a wall ending.

### 2.4 The stroke, and the width of a portal

Two more things the owner asked for from the panel, both **measured before being changed**:

| | before | after |
|---|---|---|
| Frame | 6 px | 6 px |
| Wall two cells thick | 6 px (two 3-px tiles meeting) | unchanged |
| Wall three or more cells thick | **3 px** | **6 px** |
| Tunnel mouth | **8 px** | **18 px** |

The stroke first. A two-cell wall looked right because its two 3-pixel tiles meet into 6; a
thicker wall showed each side's 3 pixels alone. The fix is not a thicker tile but a **second
ring**: run the very same rule one cell further in — treating the outer ring as if it were
corridor — and turn the answer through half a turn, so the inner tile's line completes the stroke
the outer one started. Every one of the twelve block tiles turns out to be the half-turn of
another (checked by flipping the bitmaps and matching, not assumed), so **this needed no new
art**.

The geometry is now uniform: a 6-pixel stroke set 5 pixels inside the wall's boundary, whatever
the wall's thickness. Which is also why the corridors did not narrow — the 5-pixel setback is what
sets the gap between two wall lines, and it did not move.

**It has a visible consequence and it should be said plainly:** 24 pixels of wall, less 5 either
side, leaves 14 for two 6-pixel strokes — a 2-pixel hole. So a wall exactly three cells thick is
drawn **solid**, and since the generator upscales by three, most generated boxes are solid. Only
walls four cells or thicker still read as outlines. On the arcade maze this changes 10 cells, all
of them inside walls where nothing can stand.

Then the mouth. The frame band used to stop flush with the cell boundary, giving an 8-pixel gap
against the 18 pixels of clear black a corridor leaves between two wall lines — which is why it
read as a notch. The band now stops **5 pixels short** either side: 5 + 8 + 5 = 18, the same gap
as everywhere else. Two tiles for that, plus one solid for the three-cell case.

### 2.5 Two tiles the 1980 ROM does not contain

Nothing is ever attached to the arcade maze's bottom wall, so the ROM has no bottom-edge tee.
**62 % of generated mazes attach something** — the generator joins pieces to the boundary on
purpose, because a maze of free-standing blocks is far too easy. The two missing pieces are the
top tees **turned upside down, row for row**: a mirrored arcade tile still has the arcade's line
weight, its 2-pixel bar and its diagonal step, which is why that is a fair way to fill the gap
rather than new art in an old style.

## 3 What it costs

| | Before | After |
|---|---|---|
| Flash | 89,496 B (17.3 %) | 98,256 B (19.0 %) |
| RAM | 176,428 B (67.3 %) | 178,216 B (68.0 %) |
| Frame cost on the target | 8 ms of 16 | 8 ms of 16 |
| Achieved frame rate | 59 fps | 59 fps |

The RAM is the maze held as characters in `game_t` (899 B) plus the derived tile per cell in
`game_view_t` (868 B). Both are held rather than recomputed: the tiles are read for every cell
of every field handover and cannot change until the next maze.

**The frame cost is unchanged, and that was measured rather than assumed** — the same `ott pacman`
run was taken against `main` before the change and against the branch after it, and both report
300 frames in about 5.06 s. The derivation runs once per level, not per frame, and the number of
items a frame carries did not change.

That measurement turned up something the close-out had not stated: **59 fps is not the 60 that
NFR-002 asks for**, and it never was. The frame timer is armed at 16 ms and re-armed inside its
own callback, so a period is 16 ms plus the callback path plus up to a millisecond of tick
granularity — about 16.9 ms, which is 59 fps. It predates this milestone and is not caused by
it; the unpaced ceiling of 175 fps that the earlier docs quote is a different measurement, of
how fast frames *could* come rather than how fast they do. A 15 ms period would give 66 fps and
is the obvious fix, and it is not made here: this milestone is the maze, and changing the frame
period would change what the frame-cost comparison above is comparing.

## 4 What is deliberately not done

- **The mazes are not colour-varied.** Ms. Pac-Man recolours each maze; here every maze is the
  arcade's blue. The palette is a `sprite_set` concern and no requirement asks for it.
- **The tunnels are short**, per §1.2. Making them arcade-length would mean generating the side
  masses, which is a change to the generator's own shape rather than a setting.
- **The generated maze is never validated at run time.** The unit tests check the properties
  over 100 seeds and the generator's own retries enforce them, so a maze that reaches the game
  is sound by construction. A run-time check would cost a flood fill per level to defend
  against a defect the tests already catch.
