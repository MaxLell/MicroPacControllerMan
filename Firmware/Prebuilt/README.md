# A flashable image, committed on purpose

For flashing the board from a machine that has **no cross-toolchain** — a Mac with nothing
installed, the case this exists for. Everything else in this repository builds the firmware from
source, and that is still the way to get one; this is a convenience, not an artefact the build
depends on.

**It is a copy and it goes stale silently.** Nothing rebuilds it and no test checks it against the
tree. The provenance below is the only thing that says whether it is still the firmware this
repository describes — check the commit before trusting the image, and re-export it (see the bottom)
rather than assuming.

## What this image is

| | |
|---|---|
| built from | the commit that made the look-ahead player think across frames (DEC-052), on `feat/m6-lookahead` |
| built on | 2026-08-17 |
| target | STM32U545RE-Q Nucleo-64, TrustZone off |
| toolchain | gcc-arm-none-eabi 13.2.1, CMake 3.28, `cmake -B build` (Debug) |
| flash | 120,040 B of 504 kB — 23.3 % |
| RAM | 235,736 B of 256 kB — 89.9 %, plus 12,008 B of SRAM4 — 73.3 % |
| weight table | `arcade-danger`, digest `41cc70f5ce88b97e` — **3,531 against FR-037's 4,600** |
| `pacman.hex` | sha256 `9ec0492a9c227ca668f8a2d1da7c1b8822c02d9c1005ecaa40ad34d4c566d30f` |
| `pacman.bin` | sha256 `8426b7d63ca1d73914c99b8e2c9dc9ff64547dd0993f926dc261102ceb17af3d` |

**Verified on hardware, unlike the image this replaces.** This exact build was flashed to the board
and the whole automatic suite passed — thirteen tests including `ai_equivalence`, `high_score` and
`lookahead_cost`. The host side is green too: 485 unit tests, both builds.

The `.elf` is **not** here. It is 2.3 MB, almost all of it debug information, and nothing flashes
from it that the two files here cannot flash — it is worth having only with a debugger attached, and
a machine with a debugger attached can build.

## Flashing it from a Mac

**STM32CubeProgrammer** is the supported way and the one this project uses on Linux
(`Docu`/`CLAUDE.md` explain why openocd cannot program this part — its flash driver does not know
device ID 0x455). It is a free download from st.com and needs an ST account.

```sh
STM32_Programmer_CLI -c port=SWD -w pacman.hex -v -rst
```

The GUI does the same thing: connect over ST-LINK, open `pacman.hex`, *Download*.

**The drag-and-drop route**, if the board's ST-LINK presents itself as a USB drive: copy
`pacman.hex` onto that volume and wait for its LED to stop blinking. It is much the easier path when
it works — but **this was not verified**, because the board was unplugged from the build machine
before it could be checked, and whether the ST-LINK V3E on this Nucleo exposes mass storage depends
on its firmware version. If no drive appears, use CubeProgrammer.

Use `pacman.hex` for either. `pacman.bin` is the same image without addresses in it and has to be
written to `0x08000000` explicitly; it is here only for a tool that insists on raw binary.

## What to press

**The two agents live in the `PAC-MAN AI` game and are chosen on the menu.**

1. Push the stick **down** once, to `PAC-MAN AI`. The line under it reads `AGENT NEAT`.
2. Push **left or right** to change it to `AGENT SEARCH`, and back.
3. Press start.

The agent has Pac-Man from the first frame and keeps him. While it plays, the HUD **names it**, in
the gap between `1UP` and `LEVEL`:

```
   1UP   3531        NEAT        LEVEL 1
   1UP  11652        SEARCH      LEVEL 5
```

so there is nothing to remember. **B1 in that game still means the endless mode** (`LOOP` on the
lives row), unchanged.

Expect `SEARCH` to look **much** better. It averages **11,652** against `NEAT`'s **3,531**, and it
is the only thing in this firmware that clears the 4,600 the requirement asks for — though it does
not *satisfy* that requirement, which wants trained weights and not a search. It is also *slower to
decide*, but you will not see that either: since DEC-052 it thinks a slice at a time across the ten
frames a cell lasts, and a slice is under 3 ms of the 13 a frame has spare.

**It still dithers a little** — about one decision in nine walks back the way it came, down from one
in five. What is left is a real limit rather than a bug: nothing in its evaluation pulls toward food
it cannot already see, so a stretch of maze with no pellet and no ghost in reach leaves every
direction worth exactly the same. [M6 §16.5](../../Docu/Design/M6-Pacman-AI.md) has the numbers.

`NORMAL MAZE` is the game you play, and there **B1 hands Pac-Man to the trained network and takes
him back**, exactly as before. The search is deliberately not offered there. Any run you touch B1
in stays out of that game's high-score table (FR-034).

## The console, if you want the numbers

The ST-LINK's virtual COM port appears on a Mac as `/dev/cu.usbmodem…` at 115200 8N1:

```sh
ls /dev/cu.usbmodem*
screen /dev/cu.usbmodem1103 115200        # your number will differ
```

Then type, at the prompt:

```
ott lookahead_cost
```

The board reboots into the test, plays two thousand *frames* of a real run and prints what one
frame's slice of thinking cost — expect a mean near 2.9 ms and a worst near 11 ms of the 13 a frame
has spare, ten frames and about 1,400 simulated ticks to a decision, and 2.97 of the 3 junctions
reached. It measures frames rather than decisions because since DEC-052 a decision is deliberately
larger than a frame, so the slice is the thing that has to fit. `reset` returns it to the game. `help` lists the rest; `ott pacman` starts a run with no
menu in front of it.

(`Test/run_ott.py` drives all of this unattended and is stdlib-only, so it needs no pip install —
but its serial setup is written against Linux `stty` and has never been run on macOS. `screen` and
typing the command is the route that is known to work.)

## Re-exporting it

```sh
cd Firmware
cmake --build build -j
cp build/pacman.hex build/pacman.bin Prebuilt/
shasum -a 256 Prebuilt/pacman.hex Prebuilt/pacman.bin   # update the table above
```

`.gitignore` excludes `*.hex` and `*.bin` everywhere else in the tree; the two exceptions naming
this directory are what let these be committed at all.
