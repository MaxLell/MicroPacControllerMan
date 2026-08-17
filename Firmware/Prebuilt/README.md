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
| built from | the commit that deleted the trained network (DEC-054), on `feat/lookahead-evaluation` |
| built on | 2026-08-17 |
| target | STM32U545RE-Q Nucleo-64, TrustZone off |
| toolchain | gcc-arm-none-eabi 13.2.1, CMake 3.28, `cmake -B build` (Debug) |
| flash | 108,092 B of 504 kB — 20.9 % |
| RAM | 230,888 B of 256 kB — 88.1 %, plus 12,008 B of SRAM4 — 73.3 % |
| `pacman.hex` | sha256 `fde101b8b5ea7904d1b35d13f3e31fb3c2d48818747c7e043ed34d01ae0717f4` |
| `pacman.bin` | sha256 `3ad2a32fd2be6c5c7b4574087fc9a68194126270c19ac43a7b4402bbe73396a4` |

**Verified on hardware, unlike the image this replaces.** This exact build was flashed to the board
and the whole automatic suite passed — thirteen tests including `ai_equivalence`, `high_score` and
`lookahead_cost` — eleven tests now, two fewer because `ai_equivalence` and `ai_frame_cost` went with the trained network. The host side is green too: 446 unit tests, both builds.

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

**The machine lives in the `PAC-MAN AI` game.** Push the stick **down** once, to `PAC-MAN AI`, and
press start.

There is only one machine now: the trained network was deleted on 2026-08-17
([DEC-054](../../Docu/PrePlanning/11-Decisions-and-As-Built.md)), so there is nothing to choose and
sideways on the menu does nothing. It has Pac-Man from the first frame and keeps him. While it plays
the HUD says so, in the gap between `1UP` and `LEVEL`:

```
   1UP  21870        AI          LEVEL 6
```

**B1 in that game means the endless mode** (`LOOP` on the lives row). In the two games a person
plays, B1 now does nothing at all — handing Pac-Man over mid-run went with the network.

Expect it to average **21,870** and reach **level 5.9** — for scale, a cleared level 1 is 2,600
points and the trained network it replaced managed 3,531 at level 1. It hunts ghosts rather than
hoovering pellets, because a four-ghost sweep is 3,000 points where a whole level of pellets is
2,440, and that is what fitting its evaluation weights against whole games decided for it.

**It still dithers a little** — about one decision in twenty walks back the way it came, down from
one in five before its leaves could see. [M6 §17](../../Docu/Design/M6-Pacman-AI.md) has the numbers.

`NORMAL MAZE` and `RANDOM MAZE` are the games you play, and the machine is not offered in either.
The AI's own runs go into its own high-score table and stay out of a person's (FR-034/FR-041).

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
