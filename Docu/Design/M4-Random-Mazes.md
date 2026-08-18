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
| The four ghost starting cells | Blinky (14,11) outside, Inky (11,14), Pinky (13,14), Clyde (15,14) inside | Same coordinates as the arcade, so the dot-counter release order, revival after being eaten and the rule that Pacman may never enter keep the meaning they were verified with ([10 §10.4](../PrePlanning/10-Pacman-Game-Design.md)). **Two cells apart, which is exactly one ghost wide** — a ghost is drawn 16 pixels across, so that stands the three of them shoulder to shoulder, as the arcade does at its own 11.5, 13.5 and 15.5. Clyde used to be at 16, one cell too far right, which left an 8-pixel gap between him and Pinky; it was reported from the panel and was a transcription slip rather than a choice. The three are 48 pixels wide in a 64-pixel house, and the group sits 4 pixels left of the middle: a ghost's centre falls on a cell's centre, the house's on a cell boundary, and no integer cell is at both. |
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

### 2.5 Giving the outer wall its second cell, which removed all of this

Four complaints came back from the panel at once: pellets not centred, the joins where a wall
meets the frame changing thickness, the tunnel mouths still gappy, the ghost house too thin. They
turned out to be **one cause**.

Every wall inside the maze is two cells thick and draws a 6-pixel stroke set 5 pixels in — that
needs 16 pixels to sit in. The outer wall is **one** cell, 8 pixels, so its stroke had to move,
and once it moves nothing agrees with it: the corridor beside it is 14 pixels where every other
corridor is 18, its pellets sit 2 pixels off the middle, and a wall arriving at it lands beside
the branch rather than on it.

So the outer wall was given its missing cell, in the panel's own margin — 8 px either side of a
224 px maze in a 240 px panel, and the row above, with the lives moved one row down to make room
below. It is then an ordinary two-cell wall, and the whole frame apparatus goes away:

| | before | after |
|---|---|---|
| Frame tiles | 4 edges, 4 corners, 8 tees, 2 mouth tiles | **none** — the block rule draws it |
| Rules for the frame | ~80 lines | **none** |
| Worst pellet offset in the maze | 2.0 px | **0.0 px** |
| Tunnel mouth | 8 px, then 18 with two special tiles | **18 px**, no special tiles |
| Ghost house wall | 2 px | **6 px** |
| Flash | — | **272 B smaller** |

The ghost house is the one thing that cannot be fixed this way: it sits in the middle of the maze
with no margin to borrow, so its wall stays one cell. It wears the 6-pixel band centred in that
cell — matching every other wall's *weight* if not its inset. Nothing is lost by that, because the
arcade clears the pellets all round the house, so there is no pellet next to it whose centring the
inset would decide.

The field handover grows from 868 cells to 990, which is one more message and no measurable frame
cost.

**What this costs in verification**, stated rather than buried: the appearance test no longer
compares the outer wall against the arcade's map — 114 cells — nor the house's 40. Both are
skipped and counted. For the outer wall that comparison had already stopped meaning anything in
§2.3, where the tiles kept their ids and lost their pixels; this makes the loss visible instead of
leaving a check that only looks like one. The 596 interior cells are still held to the arcade's
own tile, exactly.

### 2.6 And then the tiles went away

Four rounds of the above — [DEC-031](../PrePlanning/11-Decisions-and-As-Built.md) to DEC-033 —
each fixed a visible fault by adding tiles or a special case, and each time another seam appeared
somewhere else. The owner asked why the tiles could not simply be generic sprites placed where
needed, which is what they already were, and then asked for shaunlebron's own approach instead:
draw the maze as geometry. He said the original tiles were not needed here.

So the whole alphabet is gone — 24 tiles, the letter map that named them, and the two lookups —
and one rule replaces it:

> A pixel of a wall cell is **ink** when its distance to the wall's nearest edge lies in
> `[inset, inset + 6)`, where `inset` is half the wall's spare depth, capped at 5.

Six is the stroke; five is the setback that leaves every corridor the same clear black. Both are
now *arithmetic*, so they are the same everywhere by construction — which is exactly what a fixed
alphabet could not give, because a tile's ink sits at a fixed place in its 8 pixels and therefore
only composes at the thickness it was drawn for.

Everything the alphabet had a piece for falls out of that one line: edges, outer and inner
corners, junctions, wall ends, the mouths where a tunnel breaks the wall. The pixels travel in
the display list — `DISPLAY_ITEM_WALL` carries an 8-byte bitmap where a background item carries a
drawing's name — so Render stays as ignorant of the maze as it always was.

The distance is measured to the nearest edge **in any direction, corners included** — Chebyshev,
so a stroke turns a corner squarely rather than rounding it. That was not the first attempt: the
first took the smallest of the four axis distances, which is the same answer along a straight edge
and the wrong one at a junction. Where a branch meets a wall the nearest edge is the *inside
corner* of the join, which is diagonal; to an axis it looks eleven pixels away when it is five, so
the branch's stroke stopped short of the one it was meant to run into. A T with a gap in its neck,
and the same wherever a box met the outer wall. One correction, and both closed.

Two known departures, both deliberate:

- The **ghost house's wall is given a second cell** — the first ring of its own inside — because
  one cell cannot carry this stroke. Measured on one maze: 964 of its strokes had their centre on a
  cell boundary and 32 did not, and the 32 were the house, half a cell off the grid the rest of the
  maze sits on. The corridor round it was 14 pixels where every other corridor is 18, for the same
  reason. With the second cell it is an ordinary two-cell wall and needs no exception at all.

  It costs the house's inside: 24 pixels of black become 10, and the waiting ghosts overlap the
  wall by 6 pixels instead of 3. **A one-cell ring can have a 6-pixel stroke, or the grid, or a
  roomy inside — any two.** The owner was shown the three ways round it, with the numbers, and chose
  the grid.

  The gate is part of that wall rather than a hole in it: **wall that happens to be pink**. Left
  out, it broke the house's top in two, and a broken stroke ends in caps — two blue blocks either
  side of the opening, which is what the first attempt looked like. Passable to a ghost either way;
  that is the map's business, not the picture's.
- A wall **exactly three cells thick** shows the 2-pixel hole the arithmetic gives it, rather than
  being drawn solid. DEC-032's solid tile went with the rest.

**What it costs is the arcade comparison**, entirely: there is no tile left to compare against the
1980 map, and §2.1's 764-of-764 goes with the alphabet. What replaces it is aimed at what was
actually going wrong. Three unit tests rebuild the picture from the display list and measure it:

| | asserted |
|---|---|
| Every pellet | within **1 px** of its corridor's centre line |
| Every tunnel mouth | exactly as wide as a corridor's own gap (**18 px**) |
| Every outside corner | cut back further on the diagonal than the stroke's setback along a straight face — §2.8 |

The first two are the faults the owner reported, and they are now caught by `ceedling` rather than
by looking at the panel.

One more fault was reported and it was not geometry at all: a wall was drawn straight across the
tunnel mouths. The border outside the maze mirrors the maze's edge cell — wall beside wall, open
beside a mouth — and the *geometry* used that rule while the code that decides what a cell **is**
still treated every border cell as wall. A portal Pacman cannot walk through is not a portal.

### 2.7 Two tiles the 1980 ROM does not contain

Nothing is ever attached to the arcade maze's bottom wall, so the ROM has no bottom-edge tee.
**62 % of generated mazes attach something** — the generator joins pieces to the boundary on
purpose, because a maze of free-standing blocks is far too easy. The two missing pieces are the
top tees **turned upside down, row for row**: a mirrored arcade tile still has the arcade's line
weight, its 2-pixel bar and its diagonal step, which is why that is a fair way to fill the gap
rather than new art in an old style.

### 2.8 The corner, which needed a case after all

[DEC-057](../PrePlanning/11-Decisions-and-As-Built.md). The owner asked for the maze to look like
the original again and gave two reference pictures: the arcade cabinet, whose walls are **hollow**,
and a wallpaper rendition, whose walls are **massive**. They disagree about that and agree that
**every corner is round**. He chose massive plus round.

Hollow against massive is free, and it is the clearest thing §2.6's arithmetic bought: it is the two
numbers, `6 / 5` against `2 / 1`, and either falls out of the one rule at every wall thickness. A
tile alphabet could not have given the second at all without a new family per thickness.

The corner is not free, and **the wrong turning is worth writing down** because §2.6 invites it.
That section says the distance is Chebyshev *so that a stroke turns a corner squarely* — which reads
as though a true Euclidean distance would round it. It does not. At a **convex** corner of a wall the
nearest pixel that is not wall lies straight out, left or up, and never diagonally; the distance
there is `min(x, y) + 1` under every metric, and the contour is an L. Measured, switching the walk to
Euclidean moved **106 of 15,396 ink pixels, all of them at *inside* corners**.

> A distance field offset from outside cannot round an outside corner. It can only displace it.

So the corner is cut on purpose, and the rule is the stroke's own geometry rather than a shape from
an alphabet:

> A convex corner is a **bend in the stroke's centre line**. The centre line runs
> `inset + width / 2` in from a face — 8 px here, exactly one cell, which is why the bend sits on a
> cell corner. The arc is the disc of `width / 2` around the bend.

`prv_is_inside_the_corner_arc` is given the pixel's distance to the **two faces that meet at the
corner**, so mirroring its inputs serves all four orientations from one function: there is no case
analysis by direction, only the question *is there a corner here*, answered from the 3×3 cell
neighbourhood the code already reads. `prv_is_kept_by_every_corner` intersects the arcs where a cell
has more than one corner — the end of a one-cell bar is two, a wall one cell square is four — and
that intersection is what makes a bar's end a round cap.

**The alternative was costed and rejected.** Rounding the wall *shape* morphologically —
erode by a disc, dilate by the same — is the textbook answer and gives concave fillets for free. It
needs a pixel-level distance field over the whole maze: two 240 × 264 bitmaps at 7.9 kB each against
about 28 kB of free main RAM at 89.4 %, and some 36 million comparisons per level build. For three
pixels a corner that is the wrong price.

| | |
|---|---|
| Flash | **+304 B** → 21.2 % |
| RAM | **unchanged** → 89.4 % |
| Frame | **unchanged** — wall pixels are built with a level's field, not per frame; worst slice 12 ms of 13 |
| Tests | 450 host green (one new), 11 of 11 on-target green |

**What it costs the spec** is §2.6's proudest claim, and only partly: the appearance needs no case
analysis *along a straight run*. There is exactly one case now, it is *is there a corner*, and it is
not the fault that killed the alphabet — that was a family of tiles per wall thickness, and this is
one formula that does not know how thick anything is.

**How the corner is tested** is a relationship and not a radius, so it survives the stroke being
re-tuned: along a straight face the stroke keeps a setback, and out of a corner along the diagonal it
must keep *more*. A mitre puts the two at the same distance. Checked against the rounding switched
off, where it fails with `Expected 5 to be greater than 5: the corner is mitred, not arced`.

One measurement had to change with it. The corridor gap was taken as the **widest** black run up to
three cells; an arc cuts its corner back, so the black at a bend is legitimately a few pixels wider
than along a straight run, and the widest run in the picture is now at a bend. It is the
**commonest** run wider than a cell instead — 18 px, unchanged — and the tunnel mouth is still
exactly 18, so the claim the test exists for is untouched.

## 3 What it costs

| | Before | After |
|---|---|---|
| Flash | 89,496 B (17.3 %) | 96,320 B (18.7 %) |
| RAM | 176,428 B (67.3 %) | 178,248 B (68.0 %) |
| Frame cost on the target | 8 ms of 16 | 8 ms of 16 |
| Achieved frame rate | 59 fps | 59 fps |

The RAM is the maze held as characters in `game_t` (899 B) plus the derived tile per cell in
`game_view_t` (868 B). Both are held rather than recomputed: the tiles are read for every cell
of every field handover and cannot change until the next maze.

**The frame cost is unchanged, and that was measured rather than assumed** — the same `ott pacman`
run was taken against `main` before the change and against the branch after it, and both report
300 frames in about 5.06 s. The derivation runs once per level, not per frame, and the number of
items a frame carries did not change.

That measurement turned up something the close-out had not stated: **the achieved rate is 59 fps,
not the 60 the design aimed at**, and it never was. The frame timer is armed at 16 ms and re-armed inside its
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
