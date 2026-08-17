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
| built from | the commit that added this file, on `feat/m6-lookahead` |
| built on | 2026-08-09 |
| target | STM32U545RE-Q Nucleo-64, TrustZone off |
| toolchain | gcc-arm-none-eabi 13.2.1, CMake 3.28, `cmake -B build` (Debug) |
| flash | 119,376 B of 504 kB — 23.1 % |
| RAM | 235,688 B of 256 kB — 89.9 % |
| weight table | `arcade-danger`, digest `41cc70f5ce88b97e` — **3,531 against FR-037's 4,600** |
| `pacman.hex` | sha256 `a85a77eb5c641f12099cce1ffe46403de8e73f9f0f0c203654ce40dc6b5e5cbc` |
| `pacman.bin` | sha256 `48ab855efbff4a57b7347ad11413bcda30f7f23f9978202ef43ab20e1c1360b6` |

**Not verified on hardware.** The board was on another machine when this was built, so nothing here
has been through `run_ott.py`. The host side is green — 468 unit tests, both builds — and that is a
different claim.

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
   1UP   3132        SEARCH      LEVEL 1
```

so there is nothing to remember. **B1 in that game still means the endless mode** (`LOOP` on the
lives row), unchanged.

Expect `SEARCH` to look a little better and to **dither**. It averages **3,132** over FR-037's
twenty draws against `NEAT`'s **3,531** on its own measurement — the two are close, and neither
meets the 4,600 the requirement asks for. It is also *slower to decide* — about 10 ms of each frame
— which you will not see, because a frame has 20.

**The dithering is worth watching for**, because it is the honest symptom of a real limit: about
one decision in fourteen walks back the way it came, and it does that because it can only see 1.63
junctions ahead and past that every direction looks alike. Given ten times the thinking time the
same code averages 7,076 and reaches level 4 — but ten times does not fit in a frame.
[M6 §15.5](../../Docu/Design/M6-Pacman-AI.md) has the measurements.

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

The board reboots into the test, plays 200 decisions and prints what each one cost — expect a mean
near 9.5 ms and a worst near 11 ms of the 13 a frame has spare, about 500 simulated ticks per
decision. `reset` returns it to the game. `help` lists the rest; `ott pacman` starts a run with no
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
