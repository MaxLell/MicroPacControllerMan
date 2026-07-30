# MicroPacControllerMan — agent guide

Standalone embedded **Pacman** on an **STM32U545RE-Q Nucleo-64**: joystick input,
240×320 colour LCD, single NVM high score. Secondary goal: probe how far an
AI agent can carry a disciplined embedded project. See `Docu/Idea.md` for origin.

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
  Current branch: `feat/m1-u545-bring-up`.
- **Requirements re-baselined** for the new hardware — [02 Requirements](Docu/PrePlanning/02-Requirements.md)
  is current; the rest of the doc set still lags in places.
- **M2 Board Bring-Up — in progress.** ST7789V display + joystick on the GFX01M2. The pin
  map is **measured, not assumed**: the joystick keys were confirmed by the `joystick` OTT
  and the display by `display_id`, which got the controller to answer. Chip select turned out
  **active LOW**, not the active high UM2750 claims. Next: the ST7789V driver and the RGB565
  colour path. See [M2 Board Bring-Up](Docu/Design/M2-Board-Bring-Up.md).
- **M3 Game — parked.** The host-only game is open as PR #10 and is not to be touched
  until M1/M2 stand on the new hardware. Its architecture rework (Data-Pool instead of
  Active Objects) is agreed in outline but not settled.

## Build · flash · test (all from `Firmware/`)

```bash
# Build (arm-none-eabi-gcc + CMake; STM32CubeMX + STM32 HAL under ThirdParty — see 11 DEC-012).
# The cross-toolchain lives in CMakeLists.txt above project(), so no -DCMAKE_TOOLCHAIN_FILE.
cmake -B build -G "Unix Makefiles"
cmake --build build -j                                   # -> build/pacman.elf, warning-free

# Flash over ST-LINK V3E. NOT openocd — see "Hardware facts" below.
STM32_Programmer_CLI -c port=SWD -w build/pacman.elf -v -rst

# Run an on-target test end-to-end (schedules, resets, reports over the VCP)
python3 Test/run_ott.py --suite                          # enumeration + boot banner
python3 Test/run_ott.py user_button --port /dev/ttyACM0  # exit 0 = PASS
./m2.sh all                                              # build + flash + the interactive tests

# Host build — no hardware, no cross-toolchain
cmake -B build-host -DPACMAN_HOST_BUILD=ON -G "Unix Makefiles" && cmake --build build-host -j

# Host unit tests (Ceedling + Unity + CMock; needs ruby + `gem install ceedling`)
ceedling test:all
```

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
