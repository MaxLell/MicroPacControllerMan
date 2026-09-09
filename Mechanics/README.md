# Mechanics

Everything the 3D-printed enclosure is designed against. The enclosure itself is not here
yet — this is the measured basis for it: the vendor's own 3D models of the two boards, the
numbers read out of them, and the tools that read them.

The enclosure is a plain box that covers the Nucleo and the electronics, with the display
flush in the lid, the joystick reachable from outside, and the device powered through the
ST-LINK's USB connector.

## What is here

| Path | What it is |
|---|---|
| `Cad/MB1841-U545REQ-D01.step` | The NUCLEO-U545RE-Q, board revision D01 — ST's own 3D model |
| `Cad/MB1642-TCXD022IB5-D01.step` | The X-NUCLEO-GFX01M2 display shield, LCD variant TCXD022IB5 |
| `Cad/MB1642-DT022CTFT-D01.step` | The same shield with ST's other LCD variant |
| `Tools/measure_step.py` | Reads part bounding boxes and PCB holes out of a STEP file, via FreeCAD |
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

## Re-running the measurements

FreeCAD 1.1.3 as an AppImage; there is no `freecad` package in Ubuntu 24.04. Headless:

```bash
cd Mechanics/Tools
FC=~/Applications/FreeCAD_1.1.3-x86_64.AppImage
"$FC" freecadcmd measure_step.py bbox  ../Cad/MB1642-TCXD022IB5-D01.step
"$FC" freecadcmd measure_step.py holes ../Cad/MB1841-U545REQ-D01.step
```

The Nucleo model is 24 MB and takes a few minutes to import; the shield takes about one.
A Qt run needs `libxcb-cursor0` installed, which `freecadcmd` alone does not.

## Not kept here

The full ST packs (48 MB and 15 MB) and the Altium board design projects are not vendored —
`Tools/fetch_st_cad.sh` gets the models back from them. The models themselves are ST's, taken
unmodified from those packs and kept here so the enclosure has a fixed reference to build on.
