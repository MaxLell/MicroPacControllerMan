# Mechanics

The 3D-printed enclosure, and the measured basis it is designed against: ST's own 3D models
of the two boards, the numbers read out of them, and the tools that read them.

**Only source is committed here.** The geometry — ST's board models, the enclosure's STEP and
STL — is either fetched or generated, and is shared over Syncthing rather than through git.
Two commands put all of it back:

```bash
cd Mechanics/Tools && ./fetch_st_cad.sh                      # -> Cad/*.step, from ST
cd ../Enclosure && "$FC" freecadcmd enclosure.py .           # -> both parts, STEP and STL
```

| Path | What it is |
|---|---|
| `Enclosure/enclosure.py` | The enclosure — parametric, builds both parts and exports them |
| `Enclosure/verify_fit.py` | Intersects the enclosure with every component of both boards, and the two parts with each other |
| `Tools/fetch_st_cad.sh` | Fetches ST's board models |
| `Tools/measure_step.py` | Reads part bounding boxes and PCB holes out of a STEP file |
| `Tools/export_stl.py` | Tessellates a STEP file for viewing |

`FC` below is FreeCAD 1.1.3 as an AppImage — there is no `freecad` package in Ubuntu 24.04:

```bash
FC=~/Applications/FreeCAD_1.1.3-x86_64.AppImage
```

## Where the board models come from, and the trap in getting them

ST does not put these on the product page's CAD Resources tab. They are inside the **board
manufacturing packs**, under `Mechanical/`, one per board revision, with every part carrying
its schematic designator:

- <https://www.st.com/resource/en/board_manufacturing_specification/mb1841-manufacturing.zip>
- <https://www.st.com/resource/en/board_manufacturing_specification/mb1642_manufacturing_v3-0.zip>

**`curl` cannot fetch anything from st.com from here** — Akamai drops the connection with no
status code, over HTTP/2 and HTTP/1.1 alike, whatever user agent is set. `wget` gets through.
`Tools/fetch_st_cad.sh` exists to hold onto that.

**The two LCD variants ST ships under MB1642D are mechanically identical** — module footprint,
module height and joystick agree to the hundredth. They differ only in how the display's flex
tail is drawn, so it does not matter which one the board carries.

The dimensioned drawings say the same thing in less detail and are worth having open:
**UM3062** figure 7 for the Nucleo, **UM2750** figure 5 for the shield.

## Coordinate system

Both models put the origin on the **top face of their own PCB**, with X and Y on the board
outline. So a positive Z is how far a component stands above the board it is soldered to,
which is exactly what an enclosure needs. The enclosure is built in the **Nucleo's**
coordinates, and the shield is shifted into them by (0, −16.80, +12.65).

## The display shield — X-NUCLEO-GFX01M2 (MB1642D)

| | X | Y | Top edge (Z) |
|---|---|---|---|
| PCB outline (70.36 × 75.18) | −0.13 … 70.23 | −0.13 … 75.06 | 1.60 thick, corners R2.13 |
| Notch in the top edge | 14.63 … 55.47 | 65.16 … 75.06 | 40.84 wide, 9.90 deep |
| `LCD1` display module | 15.67 … 54.25 | 7.89 … 63.23 | **+2.50** |
| — its glass | 16.54 … 53.38 | 14.04 … 62.16 | |
| — its **active area** | 18.04 … 51.88 | 15.54 … 60.66 | **33.84 × 45.12** |
| `B1` joystick | 57.67 … 68.43 | 4.91 … 15.67 | **+5.20** |
| `CN2` / `CN3` sockets | 2.10 … 7.05 / 63.06 … 68.01 | 20.32 … 69.09 | down to −10.11 |
| `CN1` flex connector (underside) | 21.68 … 48.28 | 29.13 … 35.53 | down to −6.40 |
| tallest other top-side part | | | **+1.40** |

The active area is 33.84 × 45.12 mm to the hundredth, which is 240 × 320 pixels at exactly
0.141 mm — a good sign that the model is the real one and not a placeholder.

The joystick's actuator is drawn simplified — a 10.76 mm square block 5.20 tall, where the
real part has a round cap. The lid's opening is sized to that keepout rather than to a cap
diameter the model does not state.

The shield has **no mounting holes**; it is held entirely by CN2/CN3. The two Ø3.00 holes at
y = 3.53 are the ends of the flex slot, not fixings.

**The joystick stands 2.70 mm above the display glass**, which is what settles whether it
needs a printed cap: with the display flush in the lid, the knob comes through by itself.

## The Nucleo — NUCLEO-U545RE-Q (MB1841D)

| | X | Y | Top edge (Z) |
|---|---|---|---|
| PCB outline | 0 … 70.00 | 0 … 82.50 | 1.52 thick |
| Mounting holes, **Ø3.20**, three of them | 10.87 / 59.13 / 43.89 | 54.57 / 53.30 / 2.50 | through |
| `CN7` / `CN10` morpho pins | 1.97 … 7.07 / 62.93 … 68.02 | 3.77 … 52.04 | +8.38, insulator to +2.54 |
| `CN5` `CN6` `CN8` `CN9` Arduino sockets | | | +8.51 |
| `CN1` **USB** (ST-LINK, Type-C) | 42.99 … 52.19 | 72.85 … **83.35** | −1.93 … **+3.25** |
| `B1` / `B2` buttons | 22.24 … 31.25 / 38.75 … 47.75 | 50.20 … 56.40 | +7.30 |
| `CN4` SWD header | 3.65 … 9.49 | 58.37 … 70.97 | +6.60 |
| `JP3` | 55.31 … 60.40 | 56.59 … 69.30 | +8.38 |

**The USB connector overhangs the board edge by 0.85 mm** (it reaches y = 83.35 on an 82.50 mm
board). Its body is 9.20 wide and stands 3.25 above the PCB. The enclosure's side opening is
sized for the plug's overmould, not for this.

## How the two stack — derived, not measured

The two STEP files are each referenced to their own PCB, so nothing in them states the
mated height. It follows from the connectors:

- the shield's `SSW-119-01-G-D` sockets have an 8.51 mm body below the shield's PCB;
- the Nucleo's `TSW-119-09-G-D` pins stand +8.38 above its PCB, with a 2.54 mm insulator
  under them;
- the socket cannot go lower than the insulator's top face without the two solids
  intersecting, so it lands at **+2.54**.

That puts the **shield's PCB top 12.65 mm above the Nucleo's PCB top** (2.54 + 8.51 + 1.60),
and therefore, above the Nucleo's PCB top:

| | |
|---|---|
| display glass, top | **15.15 mm** |
| joystick, top | **17.85 mm** |
| whole stack, from the Nucleo's underside | **19.37 mm** |

**This is a derivation and should be checked with one caliper reading before anything is
printed.** If the sockets in fact seat all the way down onto the Nucleo's PCB, every figure
above drops by 2.54 mm — and the display, which is flush *because of* this number, ends up
recessed by exactly that.

## Two things that will bite when modelling

**The display's flex tail is drawn unfolded.** In both shield models `LCD1` extends to
y ≈ −29, some 29 mm past the bottom edge of the board, because the model shows the tail
straight instead of folded under. It is a drawing, not an obstruction. Deriving the
enclosure's outline from raw bounding boxes puts 3 cm of nothing into the box.

**The connector pins are drawn full length.** Both boards' through-hole parts reach about
10 mm below their PCB, which is the pin before it is soldered and clipped. Stand the Nucleo
off its floor rather than trusting that number.

## The enclosure

**81.16 × 111.08 × 23.07 mm**, two printed parts, no fasteners: the lid **snaps** into the
base. Every dimension is a named constant at the top of `enclosure.py`, taken from the tables
above, so changing one number and re-running rebuilds both parts.

### The snap

**What flexes is a horizontal tongue cut out of the base's wall**, not a finger hanging off
the lid — and that is a layer-direction decision rather than a strength one. Printed
lid-face-down, a hanging finger grows in Z and bends *across* its layers, which is the one
direction PLA is weak in: about 30 MPa against 55 along them. The first version of this
design had exactly that, at 41 MPa nominal and near 75 with the sharp corner at its root. It
would have broken, and no amount of extra material behind it would have helped — peak strain
is `1.5 · thickness · deflection / length²`, so a beefier spring breaks *sooner*.

A tongue in a wall printed floor-down bends within its layers instead:

| | |
|---|---|
| tongue | 28 × 6 mm, the wall's own 2.4 mm thick, freed by a 1.2 mm slot |
| engagement | **0.75 mm** |
| peak strain | **0.56 %** — about 20 MPa against ~55 |

Six of them: two per long side, one per end. The one on the USB wall sits off to the side
because the socket's opening takes the middle.

The lid's half is a **rigid nub**, stepped so the tongue's nose has an upward-facing ledge to
sit on. Nothing on the lid flexes, and nothing on it is long enough to snap off in a pocket.

Both mating faces are at 45°, which is the steepest either can be and still print without
support, and which makes the lid come off with a firm pull. To open it deliberately, push a
tongue outward with a fingernail — it is part of the outer wall, so it is right there.

### What else decided its shape

**The display sits flush, and that is what makes the lid thin.** There are only 2.50 mm
between the shield's PCB and the display's glass, and the tallest component in that space —
apart from the display and the joystick — is 1.40 mm. So over the shield the lid is
**1.00 mm** and no more. Everywhere else it keeps its full **2.50 mm**, and the step between
the two is a pocket in its underside. That thin panel is a flange around a large opening,
supported on all sides, not a span.

**The rim left standing around that pocket rests on the shield's bare edge**, which is what
holds the board stack down — on three sides only. On the fourth the display's flex crosses
the board edge on its way to the slot and stands 1.30 mm proud, so the pocket runs past it.

**The walls stand 3.0 mm off the boards** — enough for the lid's nub and its sliding gap.
That is the whole reason the box is not tighter. The tongues need no room inside: they bulge
*outward* when the lid goes on.

**The Nucleo is located, not screwed.** Three standoffs 4 mm high, each with a Ø2.90 peg into
the board's Ø3.20 holes, and the lid's rim above.

### Print notes

PLA, **0.2 mm** clearance for one printed part inside another (`PRINT_CLEARANCE`). About
51 cm³ of filament for both parts together.

Print the **base floor-down** as exported — nothing overhangs except the top edge of the USB
opening and of the six snap windows, all short bridges. Print the **lid top-face-down**, so
the visible surface is against the bed and the hooks grow upward off it. Neither part needs
support.

**Layer direction is load-bearing here**, which is why the spring is in the base and not on
the lid. Printed floor-down, a tongue bends along its layers. Do not reorient either part to
save time: printing the base on its side would put every tongue back into the weak direction.

### What has been checked, and what has not

`verify_fit.py` places every solid of both board models where the assembly puts it and
intersects it with both parts, then intersects the two parts with each other:

```
checking 1077 component solids
BASE_LID_OVERLAP 0.0000 mm3
SNAP_HOLDS 11.62 mm3 of interference when lifted 0.5 mm
COLLISIONS 0
```

The lifted check is there because the other two only prove the parts go *together*. A snap
that does not get in the way of the lid coming off is not a snap, and that is invisible in
both a render and an overlap test.

It clips the two drawing artefacts above rather than skipping them, so a real part that
happens to be low or long still counts. It has earned its keep: it found the lid's first
locating lip coming down onto the shield's edge, a rim overlapping components by 0.08 mm, a
rim landing on the display's flex, and snap windows cut 1 mm short, which left a skin of wall
for the hooks to jam against — 34.76 mm³ of overlap, invisible in a render.

What it cannot check is **SHIELD_PCB_TOP**, the derived stack height. Every vertical
dimension hangs off it. **Measure it before printing.**

The tightest clearance in the design is **0.10 mm**, between the lid's pocket floor and that
1.40 mm component.
