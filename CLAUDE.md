# MicroPacControllerMan — agent guide

Standalone embedded **Pacman** on an **STM32U545RE-Q Nucleo-64**: joystick input,
240×320 colour LCD, single NVM high score. Secondary goal: probe how far an
AI agent can carry a disciplined embedded project. See `Docu/Idea.md` for origin.

> **Closed on 2026-08-04 (DEC-028), reopened the same day (DEC-029)** when the owner asked
> for randomly generated mazes — Milestone 5, delivered. **Every requirement in the spec now
> has a passing test.** The two that used to be unmet — a 60 fps rendering rate and a 30 ms
> input latency — are **withdrawn, not satisfied** (DEC-036): the owner judged both irrelevant
> to this game, so NFR-002 and NFR-003 are deleted along with VT-INT-016 and the automatic
> latency half of VT-INT-013. Both figures survive as *design* figures — 60 fps is why the
> frame period is 16 ms, and 30 ms is why RF-014 noticed the 32 ms debounce window — but
> nothing is measured against them. The close-out is
> [04 §4.2](Docu/PrePlanning/04-Implementation-Phases-and-Milestones.md#42-close-out) and
> what came after it is
> [04 §4.3](Docu/PrePlanning/04-Implementation-Phases-and-Milestones.md#43-milestone-5--random-mazes).

## Source of truth — read before coding

The **Pre-Planning doc set is authoritative**, not this file and not the code.
Start at **[`Docu/PrePlanning/Index.md`](Docu/PrePlanning/Index.md)**. Most-used:

- **[02 Requirements](Docu/PrePlanning/02-Requirements.md)** — EARS FR/NFR/constraints (the "what").
- **[03 Architecture](Docu/PrePlanning/03-Architecture.md)** — MVP, pub-sub broker, tasks, and **§3.9 the required folder layout**.
- **[04 Milestones](Docu/PrePlanning/04-Implementation-Phases-and-Milestones.md)** — phases + entry/exit criteria.
- **[06 Verification & Validation](Docu/PrePlanning/06-Verification-and-Validation.md)** — the VT-UNIT / VT-INT test IDs a milestone must pass.
- **[09 OTT Mechanism](Docu/PrePlanning/09-OTT-Mechanism-and-Reset-Flow.md)** — the reset-based on-target-test flow.
- **[11 Decisions & As-Built](Docu/PrePlanning/11-Decisions-and-As-Built.md)** — what was actually built and why (deviations from the intended design).

Each milestone also gets its own **design document** under **[`Docu/Design/`](Docu/Design/)**
carrying the *how* — pin assignments, clock settings, transfer budgets, tool choices. The
requirements deliberately carry none of that: keep hardware detail out of `02` and put it there.

Firmware specifics live in **[`Firmware/README.md`](Firmware/README.md)**.

Known work deliberately left undone is tracked in
**[`Docu/Refactoring-Backlog.md`](Docu/Refactoring-Backlog.md)** (`RF-xxx`) — check it
before "fixing" something that was a conscious deferral, and add to it rather than
silently working around a wart.

## Status

- **M1 Toolchain Bring-Up on the U545RE — done, verified on hardware.** Build → flash →
  boot → console, with the OTT CLI answering and `ott user_button` passing. Flash 38.9 kB
  (7.4 %), RAM 3.5 kB (1.3 %), `.noinit` present for the retained-RAM reset flow.
- **The doc set is re-baselined onto what is built** (DEC-027, 2026-08-04). **There is no
  RTOS and none is planned:** CON-104 is deleted, FR-105 is now *cooperative execution*
  (one loop plus the 1 kHz tick, [03 §3.4](Docu/PrePlanning/03-Architecture.md#34-execution-model)),
  FR-108 keeps the broker delivering to its subscribers but without a task of its own.
  FR-103 asks for what the firmware guarantees — no shared mutable state, everything
  crossing a boundary a fixed-size value copied by value — rather than for the
  system-level bus that was never built. One broker instance exists: the game's own,
  carrying its events to `score`.
- **M2 Board Bring-Up — done, merged (PR #14).** ST7789V display + joystick on the GFX01M2. The pin
  map is **measured, not assumed**: the joystick keys were confirmed by the `joystick` OTT
  and the display by `display_id`, which got the controller to answer. Chip select turned out
  **active LOW**, not the active high UM2750 claims. The ST7789V driver, the RGB565 frame
  buffer and partial updates are in (3 fps whole-frame becomes 290 fps for what a game
  actually changes), and `joystick_dot` and `animation` put input and display together.
  **The frame-rate target became 60 FPS, not 30** — measured: five moving actors cost 5.26 ms
  of a 16.7 ms frame, the unpaced ceiling is 175 fps, and the panel itself refreshes at 60 Hz.
  Open, and deliberately so: the 32 ms debounce window is a whole 30 ms input budget
  (RF-014), to be chosen against a real game loop. Both figures were requirements at the
  time and are design figures now (DEC-036).
  See [M2 Board Bring-Up](Docu/Design/M2-Board-Bring-Up.md).
- **M3 Game — done, playable on the board and on the host.** `game` publishes a
  246-byte state, `game_view` turns cells into pixels and interpolates between simulation
  steps, `render` owns the one frame buffer and erases by save-under, and `game_session`
  is the frame all three callers run — the target's `app_main`, the SDL window
  (`./build-host/pacman_host_app`) and `ott pacman`, so the window is evidence about the
  firmware rather than about a lookalike. No Data-Pool — every
  module talks through messages only, and no message carries a pointer (DEC-016). PR #10
  is closed; `host_main.c` was the last thing salvaged from it.
  The playfield is now the **arcade's own 28 × 31 maze at 8 px per cell**, one maze for
  every level, and the difficulty is the arcade's own progression in `App/difficulty` —
  per-level speeds, the tunnel crawl, the shrinking frightened window and **Cruise Elroy**
  (DEC-017). The run ends at level 21, where that table stops changing. The figures and the
  wall tiles are the **1980 ROMs**, decoded offline into `sprite_set`, so the maze is thin
  blue outlines and 244 pellets rather than filled blocks (DEC-018/019). Interpolation
  measures the step already taken rather than guessing the next one — which is what made
  corners stutter. The maze is written down twice on purpose, rules in `playfield` and
  appearance in `game_view`, with a unit test holding the two together. The **HUD** is in
  (DEC-020): score, level and lives in the arcade's own font, sent slot by slot so only the
  digit that moved travels. The **double frame buffer is resolved** (DEC-021): `render`
  owns the one buffer, `ott_framebuffer` borrows it, and `render` is in the target build.
- **M3 runs on the board.** `app_main` starts the game at power-on and polls the console in
  the same loop; `ott <name>` reboots into the test and `reset` returns to the game
  (DEC-022). Playing it is a test in its own right: `ott pacman` (VT-INT-022) starts a run
  with no menu in front of it and reports what a frame costs. Measured on the target:
  **RAM 67.3 %, flash 17.3 %** (176,428 of 256 kB; 89,496 of the 504 kB the linker
  leaves the firmware). Wiring it in broke
  `run_ott.py` — the UART receive register holds one character with no FIFO, and a loop
  that now spends milliseconds inside a frame drops most of a command line; the console
  samples it from the 1 ms tick into a ring buffer instead (RF-016 for the interrupt).
- **The game sits inside a shell** (DEC-026): loading screen, menu, the run, the score screen, back
  to the menu (FR-001/002/003/023) — and since DEC-045/046 the menu is where one of three games is
  chosen. The title is set in the tile ROM's own font, plus **one glyph that was drawn and not
  decoded**: the hyphen of `PAC-MAN`, because the ROM extract is letters, digits and a space.
  `start` on the console presses the start key, so the whole flow is walkable from `run_ott.py`
  (VT-INT-011 is now automatic).
- **The high scores are in flash** (DEC-025, DEC-046): three tables of three, one per game the menu
  offers, behind a magic word, a version and a CRC, in one 8 KB page the **linker** reserves so the
  firmware cannot grow into it.
  `highscore` on the console prints them, `highscore reset` clears them, and `ott high_score`
  proves the round trip on real silicon — which is how the ICACHE was caught answering
  reads with what the page used to hold.
- **The ghosts are the Dossier's** (DEC-023): straight-line distance, the arcade's look-aheads
  and shy radius, a ghost house nobody may
  re-enter and Pacman may never enter, arcade spawn positions and dot-counter release, and
  the scatter targets in the unreachable dead space with the corners assigned the right way
  round. **Two deliberate departures, both asked for by the owner:** seeking is a route search
  rather than the arcade's one-cell greedy choice, and **a ghost never turns around** — the
  arcade's forced reversal on a mode change is gone (DEC-037), so a mode change moves a ghost's
  target and takes effect from the next junction. Only a dead-end stub still forces the way
  back, and no generated maze has one. Measured either side: 86 reversals over 25 runs became 0.

- **M5 Random Mazes — done, verified on hardware** (DEC-029/030, 2026-08-04). Every level of a
  **random-maze** game plays a maze **generated** for it (FR-029); since DEC-045 the arcade's one
  layout is the other game the menu offers rather than a retired fixture. `App/maze_gen` is a
  faithful port of the tetris-stacking generator from
  [shaunlebron/pacman-mazegen](https://github.com/shaunlebron/pacman-mazegen) — 9 × 5 grid of
  stacked pieces, upscaled by three, mirrored, which *is* 28 × 31. Faithful on purpose, JavaScript
  accidents included, because that is what made it checkable: the original and the port were run
  under the same seeded PRNG and their output compared **byte for byte over 300 seeds**. The seed
  is **the tick at the moment start was pressed**, and `ott pacman` prints it so a maze can be
  reproduced. The ghost house, its gate, the four ghost starts and Pacman's start stay at the
  arcade's coordinates — the generator's own grid already puts them there — so release, revival,
  the gate rule and the scatter targets keep their meaning. **Tunnels are shorter** than the
  arcade's six cells (one or two), so the tunnel is a weaker escape.
  **The maze is no longer written down twice**: `game_view` derives the appearance from the walls
  (DEC-030), which reproduces the arcade's hand-drawn tile map for **764 of 764 cells** outside
  the two tunnel masses; the 64 inside them are excluded and asserted. Two tiles were added as
  vertical mirrors of the top tees — the 1980 ROM has no bottom-edge tee and 62 % of generated
  mazes need one. **The outer frame is 6 px thick, not the arcade's 2** (DEC-031): the owner asked
  for it to match the weight a two-cell inner wall already renders at, knowing that at 6 px in an
  8 px cell the screen corners become solid right angles. The ghost house keeps the arcade's 2 px
  wall and has four tiles of its own for it. **A wall's outline is a 6 px stroke set 5 px in
  whatever its thickness** (DEC-032), drawn across two cell rings — the same rule one cell further
  in, turned half a turn, so no new art was needed; a wall exactly three cells thick has no room
  for a hole and is drawn solid, which is most generated boxes. A **tunnel mouth is 18 px**, the
  same gap a corridor leaves between two wall lines. **The outer wall has a second cell in the
  panel's margin** (DEC-033) and is therefore an ordinary two-cell wall: the whole frame tile
  family and its rules are gone, pellets are centred to 0.0 px everywhere, and flash went *down*.
  The ghost house has no margin to borrow, so it keeps a one-cell wall with the 6 px band centred
  in it. **And then the tile alphabet went away** (DEC-034): the maze is drawn as geometry now —
  a pixel is ink when its distance to the wall's nearest edge is in `[inset, inset + 6)`, inset
  being half the spare depth capped at 5 — so width and setback are arithmetic rather than a
  choice from 24 ROM tiles that only composed at one thickness. The pixels travel in the display
  list (`DISPLAY_ITEM_WALL`). The arcade tile comparison is gone with them; what replaces it are
  two unit tests that rebuild the picture and measure it — every pellet within 1 px of its
  corridor's centre, every tunnel mouth exactly a corridor's gap wide. Every wall stroke's centre
  lands on a cell boundary — including the ghost house's, which needed a second drawing cell to get
  there (a one-cell ring can have the 6 px stroke, the grid, or a roomy inside: any two, and the
  owner chose the grid).
  RAM 68.0 %, flash 18.7 %; frame cost unchanged at 8 ms of 16.
  See [M4 Random Mazes](Docu/Design/M4-Random-Mazes.md).

- **M6 Pacman AI — in progress (DEC-038..044, 2026-08-05).** An agent evolved on the host with
  **NEAT**, ported to the target as `const` weights, and offered in two of the three games the menu
  now lists. Everything is built and everything that touches hardware is verified there. **FR-037 is
  met**: 4,980 points on the normal maze against 433.5 for a random policy, a factor of 11.5 where
  the requirement asks for 10. The margin is not comfortable, and [M6 §14](Docu/Design/M6-Pacman-AI.md)
  says so — a second training run from a different starting population reached only 4,260.
  - **The network that trains is the network that ships** (DEC-042). Training does *not* use
    neat-python's evaluator: a genome is flattened into the arrays `Services/neural_net` reads and
    the C side plays the whole episode, so there is exactly one implementation of inference in the
    project and FR-039 cannot be violated rather than merely being checked. It is also why a
    generation costs 11 s on two cores instead of the budgeted 12 s per core.
  - **What the agent sees is Pacman's own frame** — forward/left/right/back, 23 features, distances
    by breadth-first search over the open cells counting the tunnel wrap. Every level's maze is
    generated, so a policy in compass coordinates would learn "wall to the north" four times over.
    The **frightened timer is deliberately not an input**: the player has no countdown either, only
    the flashing.
  - **The curriculum is runtime configuration, not a training build** (DEC-041): `game_config_t`
    turns ghosts and power pellets off so the agent learns to walk, then to fear, then to hunt.
    Training therefore exercises the code that ships (FR-112).
  - **In the game:** the board button toggles the AI during a run and still means start on the menu
    and the score screen — `shell` decides, because `shell` knows the screen. The joystick is dead
    while the AI plays, enforced in `game_session_set_direction` alone so it holds for all three
    callers. Two flags, and the difference is FR-034: `game_session` knows whether the AI plays
    *now*, `shell` latches whether it played *at any point*, and the high-score lockout reads the
    latch — so handing control back before the last life does not launder the score.
  - **Verified on the board:** `ott ai_equivalence` (VT-INT-024) replays four states recorded on the
    host — ordinary play, frightened mode, a tunnel, a life just lost — and the target chose the same
    direction for every one; `ott ai_high_score` (VT-INT-025) plays two whole runs and shows the AI's
    stays out of flash while the player's gets in. The whole automatic suite takes 1 min 42 s.
    `ott pacman_ai` (VT-INT-023) is the manual one and **still needs somebody at the board** — note
    its buttons are swapped: B1 toggles the AI, the stick's centre confirms.
  - RAM 71.5 %, flash 21.0 %. The trained tables are **298 bytes**; the search scratch is 4.3 kB of
    RAM; the rest of the growth is the equivalence test's own recorded states and playfield.
  - **Training** lives in `Firmware/Training/` (DEC-040), host-only: `train.py` evolves,
    `evaluate.py` is VT-UNIT-010, `export_c.py` writes `App/pacman_ai/ai_weights.[ch]`,
    `pacman_ai_record` writes the FR-039 state set, `campaign.py` runs several time-budgeted
    trainings unattended and writes one summary to read afterwards.
    **Everything trains and is measured on the normal maze** (DEC-045) — one episode per genome,
    because that maze is fixed and the game has nothing random in it, so a score is a measurement
    rather than a draw. The acceptance seed set 1000..1019 and the rule against training on it are
    retired; `evaluate.py --maze generated` still asks the generalisation question and says out loud
    that the answer is not an FR-037 verdict.
    See [M6 Pacman AI](Docu/Design/M6-Pacman-AI.md).

- **The player picks one of three games (DEC-045/046, FR-040..043).** The menu carries the options
  and the high scores of the selected one, and nothing else: the title and the row of actors are the
  loading screen's, which has just shown them. The joystick's up/down keys move a **Pac-Man cursor**
  (an *actor*, so a move costs the cursor's rectangle plus three score rows instead of a blanked
  screen) and start plays what is selected.
  - `NORMAL MAZE` — the arcade's own layout at every level, the game of the `Pacman_running` tag,
    drawn by today's geometry renderer rather than that tag's ROM tiles. **B1 hands Pac-Man to the
    agent and takes him back** (FR-030); Pac-Man is **green** while it plays.
  - `PAC-MAN AI` — the same maze, the agent from the first frame, and no way to take over (FR-042).
    **B1 here toggles the endless mode** (FR-043): a finished run starts the next one instead of
    returning to the menu, and the HUD says `LOOP`. It refuses to start at all if the weight table
    cannot be evaluated, rather than starting a game that plays itself with nobody playing it.
  - `RANDOM MAZE` — the generated mazes of FR-029, and no AI at all.
  - **Three high-score tables, one per game** (FR-041), in the same flash page at layout version 2.
    FR-034's lockout narrowed with them: an AI-touched run of a *person's* game reaches no table,
    not even the agent's, and the agent's own game files into its own.
  - **The button has one owner.** `shell_press_user_button` decides what B1 means from the screen
    and the game; `app_main` only reports the press. `select` and `button` on the console push the
    stick and the button the way `start` presses start, which is what makes VT-INT-026/027
    unattended.

## Build · flash · test (all from `Firmware/`)

```bash
# Build (arm-none-eabi-gcc + CMake; STM32CubeMX + STM32 HAL under ThirdParty — see 11 DEC-012).
# The cross-toolchain lives in CMakeLists.txt above project(), so no -DCMAKE_TOOLCHAIN_FILE.
cmake -B build -G "Unix Makefiles"
cmake --build build -j                                   # -> build/pacman.elf, warning-free

# Flash over ST-LINK V3E. NOT openocd — see "Hardware facts" below.
STM32_Programmer_CLI -c port=SWD -w build/pacman.elf -v -rst

# Run an on-target test end-to-end (schedules, resets, reports over the VCP)
python3 Test/run_ott.py --suite                          # the automatic ones, unattended
python3 Test/run_ott.py --manual                         # the ones needing you at the board
python3 Test/run_ott.py pacman --port /dev/ttyACM0        # one by name; exit 0 = PASS

# Or the umbrella, which wraps every one of these
./dev.sh check                                           # format + unit tests + both builds
./dev.sh all                                             # build + flash + both OTT suites
./dev.sh install-hook                                    # format staged files + test on commit

# Host build — no hardware, no cross-toolchain
cmake -B build-host -DPACMAN_HOST_BUILD=ON -G "Unix Makefiles" && cmake --build build-host -j
./build-host/pacman_host_app                             # play it: arrows/WASD pick the game and steer,
                                                         # space starts it, esc quits

# Host unit tests (Ceedling + Unity + CMock; needs ruby + `gem install ceedling`)
ceedling test:all

# Train the AI (host only; needs the host build for libpacman_env.so — DEC-040)
python3 -m venv Training/.venv && Training/.venv/bin/pip install -r Training/requirements.txt
Training/.venv/bin/python Training/train.py                 # the whole curriculum, all cores
Training/.venv/bin/python Training/evaluate.py              # VT-UNIT-010: FR-037 and its baseline
Training/.venv/bin/python Training/campaign.py               # several runs unattended -> campaign/summary.md
Training/.venv/bin/python Training/export_c.py              # winner.json -> App/pacman_ai/ai_weights.[ch]
./build-host/pacman_ai_record > Test/Target/scripts/ott_ai_equivalence_states.c
```

**Re-exporting weights means re-recording the FR-039 state set** — that last line, in that order.
The recorded expectations belong to one weight table and carry its digest, so `ott ai_equivalence`
refuses to run against a different one rather than reporting a stale recording as a porting fault.

- **What gets a unit test: everything above the BSP.** The BSP is the *mocking*
  boundary — mock a `Bsp/` header to test the module above it; don't unit-test the BSP
  itself. Hardware is verified by the OTTs in `Test/Target`, which are never mocked and
  never unit-tested. Details in [`Firmware/Test/Readme.md`](Firmware/Test/Readme.md).
- A **platform port** is one shared header + one `.c` per platform, selected in
  `CMakeLists.txt` — see `Bsp/systick_bsp` (`systick_bsp.c` / `systick_bsp_host.c`).
  Prefer that over `#ifdef`s inside a module.
- `ASSERT` comes from the vendored `ThirdParty/embedded_utils`; a test can verify that
  a precondition fires via `Test/support/assert_probe.h`.

Toolchain (verified): gcc-arm-none-eabi **13.2.1**, cmake **3.28**, openocd **0.12.0**
(debug only), **STM32CubeProgrammer 2.23.0** (flashing).
`sudo apt-get install -y gcc-arm-none-eabi binutils-arm-none-eabi cmake openocd`, plus
CubeProgrammer from st.com. After installing openocd, **unplug/replug the board once**
so a non-root user can reach SWD — openocd ships the udev rules even though it cannot
program this part.

## Hardware facts

- **STM32U545RE-Q Nucleo-64**, Cortex-M33, TrustZone **off** (`CORTEX_M33_NS`).
  On-board **ST-LINK V3E** = SWD debug **+** virtual COM port.
- **512 kB flash, 274 kB SRAM** — 256 kB contiguous at `0x20000000` plus 16 kB SRAM4 at
  `0x28000000`. That headroom is *why* the MCU changed: a 240×320 RGB565 frame buffer is
  153.6 kB and the G431RB had 32 kB in total.
- **SYSCLK 160 MHz** (PLL1 from **MSIS 48 MHz**: `M=3 → 16 MHz × N=10 → 160 MHz, R=1`),
  1 kHz SysTick. Owned by the CubeMX `.ioc`; see
  [M2 Board Bring-Up §2](Docu/Design/M2-Board-Bring-Up.md).
  Never express a delay as a spin count — use `Services/delay` / `Services/sw_timer`.
- Serial console: **USART1 on PA9/PA10**, **115200 8N1**, at **`/dev/ttyACM0`**.
  The VCP is on the ST-LINK side, so it **stays enumerated across a target reset** —
  that is what makes the OTT reset flow work on one serial handle.
- **User button B1 = PC13, active HIGH** (idle low). Measured, not assumed: read
  `GPIOC->IDR` over SWD with the button released. **LED LD2 = PA5** (also Arduino SPI1 SCK).
- **Flashing needs STM32CubeProgrammer, not openocd.** openocd 0.12.0 attaches fine and
  is the gdb server, but its flash driver only knows `STM32U57/U58xx` (device ID 0x482)
  while this board reports **0x455** (STM32U535/U545), so `program` fails with
  `auto_probe failed`. Reasoning is in `Firmware/openocd.cfg`.
- **Reading target registers over SWD is a cheap way to settle a hardware question**
  without anyone at the board: `openocd -f openocd.cfg -c "init; halt; puts [format 0x%08X
  [mrw 0x42020810]]; resume; shutdown"` is `GPIOC->IDR` (GPIO on AHB2, `0x42020000` +
  `0x400` per port, IDR at `+0x10`). Always take a control reading from a pin whose level
  you already know — an all-zero register looks the same as a disabled clock.
- **X-NUCLEO-GFX01M2** (**ST7789V**, 240×320 colour, 5-GPIO joystick, plus a 64-Mbit SPI
  flash we deliberately do not use). Pin map measured and recorded in
  [M2 Board Bring-Up §1](Docu/Design/M2-Board-Bring-Up.md): display SCK **PA5** / MOSI
  **PA7** / MISO **PA6** / CS **PC7** (**active LOW**) / DCX **PB10** / RESET **PA1**;
  joystick NORTH **PC0**, SOUTH **PB4**, EAST **PB0**, WEST **PC9**, CENTER **PC6**, all
  active low with the shield's own pull-ups. Two traps: UM2750 claims CS is active *high*
  and it is not, and register reads carry a one-**bit** dummy so byte-aligned reads land
  off by one bit.

## Conventions

- **Layout:** layered tree `App / Bsp / Drivers / Services / Test / ThirdParty`,
  one folder per module — the binding rules are [03 Architecture §3.9](Docu/PrePlanning/03-Architecture.md#39-firmware-source-tree-layout).
  BSP peripheral wrappers carry the **`_bsp` suffix** (`dio_bsp`, `i2c_bsp`, …),
  matching [BareMetalHollowClockFw](https://github.com/MaxLell/BareMetalHollowClockFw);
  a generic primitive and its instance are separate modules (`switch` vs. `user_button`).
- **All GPIO goes through `dio_bsp`** — name the pin in `dio_bsp_pin_e` + one row in
  `g_pin_map`. Do not call `HAL_GPIO_*` anywhere else.
- **No tick arithmetic, no `millis()`.** `Services/delay` for blocking waits,
  `Services/sw_timer` for every timeout and periodic job.
- **HAL over registers.** Direct register access needs a justifying comment; there is
  exactly one today (`uart_bsp_read_character()` reads `RDR` — the HAL has no
  non-blocking single-character read).
- **Adding an OTT test:** new `Firmware/Test/Target/scripts/ott_<name>.c/.h` + one row
  in `ott_scenarios.c` + the source in `CMakeLists.txt`; nothing else changes.
- **Coding standard:** [c-code-style](https://github.com/MaxLell/c-code-style)
  (NFR-102), vendored as `Firmware/.clang-format`: Allman braces, 4 spaces, 120
  columns, `prv_` static functions, `g_` for globals **and file-scope statics** (including
  const lookup tables — there is no separate `k_` prefix), `in_`/`out_`/`inout_` on
  pointer parameters, named constants over literals, no abbreviations, `ASSERT` on
  public-function preconditions. Keep comments sparse — say *why*, not *what*.
  Fixes to the standard itself go via a separate PR to that repo, only with the
  owner's prior approval.
- **Verify on hardware** when a change has a runtime effect (build → flash → run
  the relevant OTT), the way M1 was verified.

## Git workflow

- Each milestone lands as its **own reviewed PR against `main`**; branch, don't
  commit to `main` directly.
- **Only push to this repo.** Never push to or modify other repos (e.g.
  c-code-style, EmbeddedCli) without explicit approval.
- After pushing to an open PR, re-check for new review comments.
