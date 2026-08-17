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
| built from | the commit that made the menu two axes (DEC-055), on `feat/lookahead-evaluation` |
| built on | 2026-08-17 |
| target | STM32U545RE-Q Nucleo-64, TrustZone off |
| toolchain | gcc-arm-none-eabi 13.2.1, CMake 3.28, `cmake -B build` (Debug) |
| flash | 108,544 B of 504 kB — 21.0 % |
| RAM | 230,920 B of 256 kB — 88.1 %, plus 12,008 B of SRAM4 — 73.3 % |
| `pacman.hex` | sha256 `d38cc59417b25af7e6ee205955c42a18eb4ab4f843b213b2736049b043abc3b3` |
| `pacman.bin` | sha256 `22fc569e625e3de8c0ba4bea28e899bc88c7e6d994a35cacffe98a04941c7070` |

**Verified on hardware, unlike the image this replaces.** This exact build was flashed to the board
and the whole automatic suite passed — thirteen tests including `ai_equivalence`, `high_score` and
`lookahead_cost` — eleven tests. The host side is green too: 445 unit tests, both builds.

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

**The menu is two rows and a start row.** Up and down move the Pac-Man cursor between them; left and
right change the row it is on:

```
        - PLAY -              or  - AI PLAYS -
        - CLASSIC -           or  - RANDOM -
        - ENDLESS OFF -       or  - ENDLESS ON -      (only while the AI plays)
              START
```

So there are four combinations, and each keeps its own high-score table. **The endless row is only
there while the AI plays** — a run of yours has nothing to loop — and moving the top row back to
`PLAY` takes the row away, puts the cursor on `START` and switches the loop off with it.

Press start from any row. While the AI plays, the HUD says `AI` in the gap between `1UP` and
`LEVEL`, and `LOOP` on the lives row when the endless mode is on.

Expect it to average **21,870** and reach **level 5.9** on `CLASSIC`, and **19,744** at level 5.0 on
`RANDOM` — which is the more interesting number, because its evaluation weights were fitted on the
arcade's layout and it keeps 90 % of them on mazes it has never seen. For scale, a cleared level 1 is
2,600 points and the trained network it replaced managed 3,531 at level 1. It hunts ghosts rather than
hoovering pellets, because a four-ghost sweep is 3,000 points where a whole level of pellets is 2,440.

**It still dithers a little** — about one decision in twenty walks back the way it came, down from one
in five before its leaves could see. [M6 §17](../../Docu/Design/M6-Pacman-AI.md) has the numbers.

**B1 means start and nothing else.** Handing Pac-Man over mid-run and toggling the loop both used to
live on it; the first went with the trained network and the second onto the menu. A machine's run goes
into a machine's table and stays out of yours (FR-034/FR-041).

**The scores stored by an older image are gone** the first time this one boots: four tables instead of
three is a new layout, and the version check discards a page it cannot read rather than misreading
it.

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
frame's slice of thinking cost — expect a mean near 4.5 ms and a worst near 11 ms of the 13 a frame
has spare, ten frames and about 1,800 simulated ticks to a decision, and 2.8 of the 3 junctions
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
