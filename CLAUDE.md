# MicroPacControllerMan — agent guide

Standalone embedded **Pacman** on an **STM32G431RB Nucleo-64**: touchpad input,
128×128 monochrome LCD, single NVM high score. Secondary goal: probe how far an
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

Firmware specifics live in **[`Firmware/README.md`](Firmware/README.md)**.

Known work deliberately left undone is tracked in
**[`Docu/Refactoring-Backlog.md`](Docu/Refactoring-Backlog.md)** (`RF-xxx`) — check it
before "fixing" something that was a conscious deferral, and add to it rather than
silently working around a wart.

## Status

- **M1 Toolchain Bring-Up — done & merged.** Build/flash/run proven end-to-end +
  the OTT CLI framework with the retained-RAM/reset flow, on real hardware.
- **M2 Board Bring-Up — done, display + touchpad verified on hardware; R-001 closed.**
  The mikroBUS pin map ([02 §2.3.3](Docu/PrePlanning/02-Requirements.md#233-mikrobus--stm32g431-pin-mapping-con-004--r-001))
  is HW-confirmed with a logic analyzer: the Click Shield for Nucleo-64 routes
  slot 1 via the **ST-Morpho** headers, so SPI1 is **SCK=PB3 / MOSI=PB5 /
  DISP=PB4 / CS=PB12 / EXTCOMIN=PC8** — not the Arduino-header PA5/PA6/PA7, whose
  mismatch was the blank-display root cause. `user_button`, `display`, `touchpad`
  and `touchdot` all pass; `run_ott.py` has an automatic suite. On-target
  checklist in [`Firmware/README.md`](Firmware/README.md#m2-hardware-verification-checklist).
- **Post-M2 structural refactor — landed ([11 DEC-013](Docu/PrePlanning/11-Decisions-and-As-Built.md)).**
  BSP renamed to the `*_bsp` suffix; new `dio_bsp` owns **all** GPIO; `button` →
  `switch` + `user_button`; `millis()` replaced by `Services/delay` +
  `Services/sw_timer`; `retain_ram` reduced to a byte buffer with the `ott_spec`
  layout moved into `ott.c`; `lacheck`/`dispdiag` deleted; the coding standard
  applied tree-wide (`Firmware/.clang-format`).
- **M3 Game — next.**

## Build · flash · test (all from `Firmware/`)

```bash
# Build (arm-none-eabi-gcc + CMake; STM32CubeMX + STM32 HAL under ThirdParty — see 11 DEC-012).
# The cross-toolchain lives in CMakeLists.txt above project(), so no -DCMAKE_TOOLCHAIN_FILE.
cmake -B build -G "Unix Makefiles"
cmake --build build -j                                   # -> build/pacman.elf, warning-free

# Flash over ST-LINK V3
openocd -f openocd.cfg -c "program build/pacman.elf verify reset exit"

# Run an on-target test end-to-end (schedules, resets, reports over the VCP)
python3 Test/run_ott.py --suite                          # enumeration + boot banner
python3 Test/run_ott.py user_button --port /dev/ttyACM0  # exit 0 = PASS; also display/touchpad/touchdot
./m2.sh all                                              # build + flash + all four, interactively

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
(`sudo apt-get install -y gcc-arm-none-eabi binutils-arm-none-eabi cmake openocd`).
After installing openocd, **unplug/replug the board once** so a non-root user can flash.

## Hardware facts

- STM32G431RB Nucleo-64; on-board **ST-LINK V3** = SWD debug **+** virtual COM port.
- **SYSCLK 170 MHz** (PLL, `HSI/4 × 85` — the part's maximum), 1 kHz SysTick. Owned by
  the CubeMX `.ioc`; see [02 §2.3.4](Docu/PrePlanning/02-Requirements.md#234-clock-configuration-as-configured).
  Consequence: never express a delay as a spin count — at this clock it is ~10× shorter
  than the 16 MHz the project started on. Use `Services/delay` / `Services/sw_timer`.
- Serial console: **LPUART1 on PA2/PA3**, **115200 8N1**, at **`/dev/ttyACM0`**.
  The VCP is on the ST-LINK side, so it **stays enumerated across a target reset** —
  that is what makes the OTT reset flow work on one serial handle.
- LCD Mono Click (Sharp LS013B7DH03, SPI) in slot 1; Touchpad Click (MTCH6102, I2C)
  in slot 2. The Click Shield for Nucleo-64 mates with the **ST-Morpho** headers,
  so slot-1 SPI1 = **SCK PB3 / MOSI PB5 / DISP PB4 / CS PB12 / EXTCOMIN PC8**, slot-2
  I2C1 = **PB8/PB9** (HW-confirmed, R-001 closed — see [02 §2.3.3](Docu/PrePlanning/02-Requirements.md#233-mikrobus--stm32g431-pin-mapping-con-004--r-001)).
  Set the shield's **VLS switch to 3V3**.

## Conventions

- **Layout:** layered tree `App / Bsp / Drivers / Services / Test / ThirdParty`,
  one folder per module — the binding rules are [03 Architecture §3.9](Docu/PrePlanning/03-Architecture.md#39-firmware-source-tree-layout).
  BSP peripheral wrappers carry the **`_bsp` suffix** (`dio_bsp`, `i2c_bsp`, …),
  matching [BareMetalHollowClockFw](https://github.com/MaxLell/BareMetalHollowClockFw);
  a generic primitive and its instance are separate modules (`switch` vs. `user_button`).
- **All GPIO goes through `dio_bsp`** — name the pin in `dio_bsp_pin_e` + one row in
  `k_pin_map`. Do not call `HAL_GPIO_*` anywhere else.
- **No tick arithmetic, no `millis()`.** `Services/delay` for blocking waits,
  `Services/sw_timer` for every timeout and periodic job.
- **HAL over registers.** Direct register access needs a justifying comment; there is
  exactly one today (`uart_bsp_read_character()` reads `RDR` — the HAL has no
  non-blocking single-character read).
- **Adding an OTT test:** new `Firmware/Test/Target/scripts/ott_<name>.c/.h` + one row
  in `ott_scenarios.c` + the source in `CMakeLists.txt`; nothing else changes.
- **Coding standard:** [c-code-style](https://github.com/MaxLell/c-code-style)
  (NFR-102), vendored as `Firmware/.clang-format`: Allman braces, 4 spaces, 120
  columns, `prv_` statics, `g_` globals, `k_` const tables, `in_`/`out_`/`inout_` on
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
