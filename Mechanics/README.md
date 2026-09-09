# Mechanics

The 3D-printed enclosure, and the measured basis it is designed against: the vendor's own 3D
models of the two boards, the numbers read out of them, and the tools that read them.

The enclosure is a plain box that covers the Nucleo and the electronics, with the display
flush in the lid, the joystick reachable from outside, and the device powered through the
ST-LINK's USB connector.

## What is here

| Path | What it is |
|---|---|
| `Cad/MB1841-U545REQ-D01.step` | The NUCLEO-U545RE-Q, board revision D01 — ST's own 3D model |
| `Cad/MB1642-TCXD022IB5-D01.step` | The X-NUCLEO-GFX01M2 display shield, LCD variant TCXD022IB5 |
| `Cad/MB1642-DT022CTFT-D01.step` | The same shield with ST's other LCD variant |
| `Stl/*.stl` | The same three boards tessellated, for looking at in any viewer or slicer |
| `Enclosure/enclosure.py` | The enclosure itself — parametric, builds both parts and exports them |
| `Enclosure/verify_fit.py` | Intersects the enclosure with every component of both boards |
| `Enclosure/enclosure-base.step` / `.stl` | The base — floor, walls, standoffs, screw bosses, USB opening |
| `Enclosure/enclosure-lid.step` / `.stl` | The lid — display opening, joystick opening, four screws |
| `Tools/measure_step.py` | Reads part bounding boxes and PCB holes out of a STEP file, via FreeCAD |
| `Tools/export_stl.py` | Tessellates a STEP file into one of those STLs |
| `Tools/fetch_st_cad.sh` | Re-fetches the STEP files from ST |

**The two LCD variants are mechanically identical** where it matters — module footprint,
module height and joystick all agree to the hundredth of a millimetre. They differ only in
how the display's flex tail is drawn. So it does not matter which one the board carries,
and both are kept because ST ships both under the same MB1642D.

## Where the models come from, and the trap in getting them

ST does not put these on the product page's CAD Resources tab. They are inside the **board
manufacturing packs**, under `Mechanical/`:

- <https://www.st.com/resource/en/board_manufacturing_specification/mb1841-manufacturing.zip>
- <https://www.st.com/resource/en/board_manufacturing_specification/mb1642_manufacturing_v3-0.zip>

**`curl` cannot fetch anything from st.com from here** — Akamai drops the connection with no
status code, over HTTP/2 and HTTP/1.1 alike, whatever user agent is set. `wget` gets through.
`Tools/fetch_st_cad.sh` exists to hold onto that fact.

The dimensioned drawings say the same thing in less detail and are worth having open:
**UM3062** figure 7 for the Nucleo, **UM2750** figure 5 for the shield.

## Coordinate system

Both models put the origin on the **top face of their own PCB**, with X and Y on the board
outline. So a positive Z is how far a component stands above the board it is soldered to,
which is exactly what an enclosure needs.

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

The joystick's actuator is drawn simplified — a 10.76 mm square block 5.20 tall, where the real
part has a round cap. So the lid's opening should clear that 10.76 mm envelope plus the cap's
travel, rather than a cap diameter this model does not state.

The active area is 33.84 × 45.12 mm to the hundredth, which is 240 × 320 pixels at exactly
0.141 mm — a good sign that the model is the real one and not a placeholder.

The shield has **no mounting holes**; it is held entirely by CN2/CN3. The two Ø3.00 holes at
y = 3.53 are the ends of the flex slot, not fixings.

**The joystick stands 2.70 mm above the display glass.** That is what settles the design
question of whether the joystick needs a printed cap: with the display flush in the lid, the
knob comes through the lid by itself.

## The Nucleo — NUCLEO-U545RE-Q (MB1841D)

| | X | Y | Top edge (Z) |
|---|---|---|---|
| PCB outline | 0 … 70.00 | 0 … 82.50 | 1.52 thick |
| Mounting holes, **Ø3.20**, three of them | 10.87 / 59.13 / 43.89 | 54.57 / 53.30 / 2.50 | through |
| `CN7` / `CN10` morpho pins | 1.97 … 7.07 / 62.93 … 68.02 | 3.77 … 52.04 | +8.38, insulator to +2.54 |
| `CN5` `CN6` `CN8` `CN9` Arduino sockets | | | +8.51 |
| `CN1` **USB** (ST-LINK) | 42.99 … 52.19 | 72.85 … **83.35** | −1.93 … **+3.25** |
| `B1` / `B2` buttons | 22.24 … 31.25 / 38.75 … 47.75 | 50.20 … 56.40 | +7.30 |
| `CN4` SWD header | 3.65 … 9.49 | 58.37 … 70.97 | +6.60 |
| `JP3` | 55.31 … 60.40 | 56.59 … 69.30 | +8.38 |

Ø3.20 holes take an M3 screw directly.

**The USB connector overhangs the board edge by 0.85 mm** (it reaches y = 83.35 on an 82.50 mm
board). Its body is 9.20 wide and stands 3.25 above the PCB — 9.20 × 5.18 counting the part
below the board. The enclosure's side opening has to clear the plug body, not just this.

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
above drops by 2.54 mm.

## Two things that will bite when modelling

**The display's flex tail is drawn unfolded.** In both shield models `LCD1` extends to
y ≈ −29, some 29 mm past the bottom edge of the board, because the model shows the tail
straight instead of folded under. It is a drawing, not an obstruction. Deriving the
enclosure's outline from raw bounding boxes puts 3 cm of nothing into the box.

**The connector pins are drawn full length.** Both boards' through-hole parts reach about
10 mm below their PCB, which is the pin before it is soldered and clipped. Stand the Nucleo
off its floor rather than trusting that number.

## What this means for the enclosure

Aligning the two boards on their morpho connectors puts the shield **16.80 mm** past the
Nucleo's lower edge, and leaves the Nucleo's top **24.24 mm** — the ST-LINK, the USB socket
and CN4 — sticking out beyond the shield. The assembly's footprint is therefore about
**70.4 × 99.3 mm**, which is comfortable on a Prusa Mini's 180 mm bed.

The shield's top-edge notch sits exactly over the Nucleo's `B1` and `B2`, which is what it
is there for. Both are 2.8 mm below the shield's top surface, so they are reachable through
the notch but recessed. The design does not use `B1` — the joystick's left direction is the
same "back" action (`shell_press_back`, DEC-056).

## The enclosure

**76.36 × 120.28 × 23.07 mm**, two printed parts, four M3 screws into heat-set inserts.
`Enclosure/enclosure.py` is the model: every dimension is a named constant at the top, taken
from the tables above, so changing one number and re-running rebuilds both parts.

```bash
cd Mechanics/Enclosure
FC=~/Applications/FreeCAD_1.1.3-x86_64.AppImage
"$FC" freecadcmd enclosure.py .        # -> both parts, STEP and STL
"$FC" freecadcmd verify_fit.py .       # -> COLLISIONS 0
```

### What decided its shape

**The display sits flush, and that is what makes the lid thin.** There are only 2.50 mm
between the shield's PCB and the display's glass, and the tallest component in that space —
apart from the display and the joystick — is 1.40 mm. So over the shield the lid can be
**1.00 mm** and no more. Everywhere else it keeps its full **2.50 mm**, and the step between
the two is a pocket in its underside. That thin panel is a flange around a large opening,
supported on all sides, not a span.

**The joystick needs no cap.** It stands 2.70 mm above the glass, so once the lid is flush
with the glass the knob comes through by itself.

**The rim left standing around that pocket rests on the shield's bare edge**, which is what
holds the board stack down — on three sides only. On the fourth the display's flex crosses
the board edge on its way to the slot and stands 1.30 mm proud, so the pocket runs past it.

**The screws are at the two short ends.** The boards are full width, so there is nowhere
along the long sides to put a boss without making the box wider. At the ends there is
7 mm of margin, which is where the four bosses and the lid's locating lip live. The lip
runs at the ends only for the same reason: along the long sides it would come down onto
the shield's edge.

**The Nucleo is located, not screwed.** Three standoffs 4 mm high, each with a Ø2.90 peg
into the board's Ø3.20 holes, and the lid's rim above. Screwing the board down would mean
screws that can only be fitted before the shield goes on.

### Print notes

Assumed **0.2 mm** clearance for one printed part sitting inside another; it is
`PRINT_CLEARANCE` in the model. About 54 cm³ of filament for both parts together.

Print the **base floor-down** as exported — nothing overhangs except the top edge of the USB
opening, which is a 14 mm bridge. Print the **lid top-face-down**, so the visible surface is
against the bed and the countersinks taper inward as they rise. Neither part needs support.

### What has been checked, and what has not

`verify_fit.py` places every solid of both board models where the assembly puts it and
intersects it with both parts: **1077 components, 0 collisions**. It clips the two known
drawing artefacts rather than skipping them, so a real part that happens to be low or long
still counts. That check found three genuine mistakes in the first version — the lip coming
down on the shield's edge, the rim overlapping components by 0.08 mm, and the rim landing on
the display's flex.

What it cannot check is **SHIELD_PCB_TOP**, the derived stack height. Every vertical
dimension hangs off it, and the display is flush *because of* it. If the sockets in fact seat
2.54 mm lower, the display ends up recessed by exactly that. **Measure it before printing.**

The tightest clearance in the design is **0.10 mm**, between the lid's pocket floor and that
1.40 mm component. Recessing the display half a millimetre would buy a thicker lid, if the
thin panel turns out to be a problem in the hand.

## The STL copies

`Stl/` holds the same geometry tessellated, so the boards can be opened in a slicer or any
STL viewer without FreeCAD. They are **for looking at, not for printing** — they are the
boards, not the enclosure, and a mesh has thrown away the exact surfaces the enclosure has
to be dimensioned against. Take dimensions from the STEP files.

| | Facets | Size |
|---|---|---|
| `MB1841-U545REQ-D01.stl` | 345,612 | 17 MB |
| `MB1642-TCXD022IB5-D01.stl` | 36,120 | 1.8 MB |
| `MB1642-DT022CTFT-D01.stl` | 36,448 | 1.8 MB |

Both are at 0.1 mm linear deflection. `Tools/export_stl.py` takes the deflection as an
optional argument if a smaller file is worth more than round holes.

## Re-running the measurements

FreeCAD 1.1.3 as an AppImage; there is no `freecad` package in Ubuntu 24.04. Headless:

```bash
cd Mechanics/Tools
FC=~/Applications/FreeCAD_1.1.3-x86_64.AppImage
"$FC" freecadcmd measure_step.py bbox  ../Cad/MB1642-TCXD022IB5-D01.step
"$FC" freecadcmd measure_step.py holes ../Cad/MB1841-U545REQ-D01.step
"$FC" freecadcmd export_stl.py ../Cad/MB1841-U545REQ-D01.step ../Stl/MB1841-U545REQ-D01.stl 0.1
```

The Nucleo model is 24 MB and takes a few minutes to import; the shield takes about one.
A Qt run needs `libxcb-cursor0` installed, which `freecadcmd` alone does not.

## Not kept here

The full ST packs (48 MB and 15 MB) and the Altium board design projects are not vendored —
`Tools/fetch_st_cad.sh` gets the models back from them. The models themselves are ST's, taken
unmodified from those packs and kept here so the enclosure has a fixed reference to build on.
