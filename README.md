# MicroPacControllerMan

A standalone embedded **Pacman** running on an **STM32U545RE-Q Nucleo-64** with an
**X-NUCLEO-GFX01M2** shield: directional input from the shield's 5-key joystick,
rendered in colour on a 240×320 **ST7789V** LCD, with the three best scores persisted
in flash. Every level plays a maze **generated for it**, not the arcade's one layout.
Beyond the game, the project is a testbed for how far an AI coding agent can carry a
disciplined, requirements-driven embedded project from spec to firmware.

**It is finished and it plays on the board.** Flash 18.7 % (96,292 of the 504 kB left
after the high-score page), RAM 68.0 % (178,244 of 256 kB), both builds warning-free,
371 host unit tests green, and every requirement in the spec covered by a passing test.

## Where things are

| Path | What |
|---|---|
| [`Docu/PrePlanning/`](Docu/PrePlanning/Index.md) | **Source of truth**: requirements (EARS), architecture, milestones, verification, risks, the OTT mechanism, and a decisions/as-built log. Start at `Index.md`. |
| [`Docu/Design/`](Docu/Design/) | Per-milestone design docs — the *how* the requirements deliberately omit: pin assignments, clock settings, transfer budgets, algorithms. |
| [`Docu/Idea.md`](Docu/Idea.md) | The original idea capture (historical). |
| [`Firmware/`](Firmware/README.md) | The STM32 firmware — CMake + arm-none-eabi-gcc, STM32CubeMX/HAL, STM32CubeProgrammer. See its README for build/flash/test. |
| [`Docu/Refactoring-Backlog.md`](Docu/Refactoring-Backlog.md) | Known work deliberately not done (`RF-xxx`) — deferrals, warts, and what "done" would look like for each. |
| [`CLAUDE.md`](CLAUDE.md) | Quick-start + conventions (auto-loaded by Claude Code). |

## Quick start

Prerequisites (Debian/Ubuntu): `sudo apt-get install -y gcc-arm-none-eabi binutils-arm-none-eabi cmake openocd`,
plus **STM32CubeProgrammer** from st.com.

Flashing needs CubeProgrammer, **not openocd**: openocd 0.12.0 attaches to this part and
serves gdb, but its flash driver does not know device ID `0x455` (STM32U535/U545). Install
it anyway for the ST-LINK udev rules and the debug server, and unplug/replug the board once
afterwards so a non-root user can reach SWD.

```bash
cd Firmware
cmake -B build -G "Unix Makefiles"      # the cross-toolchain is configured inside CMakeLists.txt
cmake --build build -j
STM32_Programmer_CLI -c port=SWD -w build/pacman.elf -v -rst
```

The board now boots straight into the game — loading screen, menu, run, score screen. To
drive it from the host instead:

```bash
python3 Test/run_ott.py --list           # what exists, and which kind each test is
python3 Test/run_ott.py --suite          # the automatic ones, unattended
python3 Test/run_ott.py --manual         # the ones you have to watch and confirm
python3 Test/run_ott.py pacman           # or one by name: plays a run and reports the frame cost
```

`./dev.sh all` does build + flash + both suites in sequence; `./dev.sh check` is what a
reviewer wants green (format + unit tests + both builds).

**No hardware?** The same game logic builds and plays on the host:

```bash
cmake -B build-host -DPACMAN_HOST_BUILD=ON -G "Unix Makefiles" && cmake --build build-host -j
./build-host/pacman_host_app             # arrows/WASD, space, esc
```

That window links the *firmware's* modules, not a lookalike — which is what makes playing
it evidence about the code that ships.

## How the firmware is organised

Layered tree, one folder per module, following the reference project
[BareMetalHollowClockFw](https://github.com/MaxLell/BareMetalHollowClockFw). The
binding rules are [03 Architecture §3.9](Docu/PrePlanning/03-Architecture.md#39-firmware-source-tree-layout).

```
App/         app_main, the shell, and the game: playfield,     -> may use anything below
             maze_gen, pacman, ghost, ghost_path, difficulty,
             game, game_view, render, score, high_score
Drivers/     device drivers: st7789, display                   -> uses Bsp + Services
Services/    middleware: framebuffer, gfx, sprite, msg_broker, -> nothing hardware-specific
             msg_queue, circular_buffer, sw_timer, delay,
             console, crc, active_object
Bsp/         board/MCU access: dio_bsp, spi_bsp, uart_bsp,     -> uses the STM32 HAL only
             flash_bsp, systick_bsp, switch, joystick,
             user_button, retain_ram
Test/        on-target tests, host unit tests, the harness
ThirdParty/  STM32CubeMX/HAL export, EmbeddedCli, embedded_utils
```

Four rules carry most of the weight:

- **`dio_bsp` is the only module that calls HAL GPIO.** Everything else names a pin
  (`DIO_BSP_PIN_DISPLAY_CS`) instead of a port and bit mask, so the pin map lives in
  exactly one table.
- **Nothing does arithmetic on a tick counter.** `Services/delay` owns the blocking
  wait; `Services/sw_timer` owns every timeout and periodic job. There is no
  `millis()`.
- **A generic primitive and its instance are separate modules.** `Bsp/switch` is the
  reusable debounced-input primitive; `Bsp/user_button` and `Bsp/joystick` are this
  board's instances of it.
- **A platform port is one shared header plus one `.c` per platform**, selected in
  `CMakeLists.txt` — never `#ifdef`s inside a module. `systick_bsp`, `display` and
  `flash_bsp` each have a target and a host implementation, which is what lets the whole
  of `App/` and `Services/` run and be unit-tested off the board.

Modules talk to each other by **message only** — no shared mutable state, and no message
carries a pointer. There is no RTOS and none is planned: one cooperative loop plus the
1 kHz tick ([03 §3.4](Docu/PrePlanning/03-Architecture.md#34-execution-model)).

## On-target testing

The board runs its own tests and reports the verdict over the ST-LINK virtual COM
port, so no debugger is needed. `ott <name>` stores the request in a `.noinit` RAM
buffer, resets the board, and the next boot runs the test and prints
`OTT PASSED [<name>]` or `OTT FAILED [<name>]: <reason>`, then falls through into the
game. `reset` goes back. Details in
[09 OTT Mechanism](Docu/PrePlanning/09-OTT-Mechanism-and-Reset-Flow.md); adding a
test is a new `Test/Target/scripts/ott_<name>.c` plus one registry row.

The console carries a few commands of its own alongside the tests: `start` presses the
start key (so the whole shell flow is walkable from a script), and `highscore` prints or
resets what is in flash.

## Status

**Done.** Every milestone is met and verified on hardware.

- **M1 Toolchain Bring-Up** — build → flash → boot → console, plus the OTT framework and
  its retained-RAM/reset flow.
- **M2 Board Bring-Up** — ST7789V display and joystick, the RGB565 frame buffer, and
  partial updates (3 fps whole-frame becomes 290 fps for what a game actually changes).
  The shield pin map is **measured, not assumed**, and UM2750 is wrong about chip select
  being active high. See [M2 Board Bring-Up](Docu/Design/M2-Board-Bring-Up.md).
- **M3 Game** — the rules, the ghosts from the Pacman Dossier, the arcade's per-level
  difficulty progression to level 21, the 1980 sprite ROMs, the HUD, and the shell around
  the run. Runs on the board and on the host from the same sources.
- **M4 System Integration** — the game on the target: three high scores in a
  linker-reserved flash page, and `ott pacman` playing a real run.
- **M5 Random Mazes** — a maze generated per level (FR-029), ported faithfully from
  [shaunlebron/pacman-mazegen](https://github.com/shaunlebron/pacman-mazegen) and checked
  byte-for-byte against the original over 300 seeds. The maze's appearance is *derived*
  from its walls as geometry rather than assembled from tiles.
  See [the Random Mazes design doc](Docu/Design/M4-Random-Mazes.md).

Two non-functional requirements — a 60 fps rendering rate and a 30 ms input latency — are
**withdrawn rather than satisfied** ([DEC-036](Docu/PrePlanning/11-Decisions-and-As-Built.md)):
the owner judged both irrelevant to this game. Both survive as *design* figures — 60 fps is
why the frame period is 16 ms — but nothing is measured against them.

The project started on an **STM32G431RB** with mikroBUS Click boards and a 128×128
monochrome panel, and restarted on the U545RE when Pacman went colour: a 240×320 RGB565
frame buffer is 153.6 kB and the G431RB had 32 kB in total. The reasoning for that and every
other deviation from the intended design is in
[11 Decisions & As-Built](Docu/PrePlanning/11-Decisions-and-As-Built.md).

Milestone entry/exit criteria and the close-out record are in
[04 Implementation Phases & Milestones](Docu/PrePlanning/04-Implementation-Phases-and-Milestones.md).

## Coding standard

[c-code-style](https://github.com/MaxLell/c-code-style) (NFR-102), vendored as
`Firmware/.clang-format`: Allman braces, 4-space indent, 120 columns, `prv_` for
module-private functions, `g_` for globals **and file-scope statics**, `in_`/`out_`/`inout_`
on pointer parameters, named constants instead of literals, no abbreviations, and `ASSERT`
on the preconditions of public functions.

## License

See [`LICENSE`](LICENSE).
