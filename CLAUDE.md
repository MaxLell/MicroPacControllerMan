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

## Status

- **M1 Toolchain Bring-Up — done & merged.** Register-level blinky (LD2/PA5) +
  the OTT CLI framework with the retained-RAM/reset flow; `ott blinky`
  (VT-INT-005) passes via the Python harness on real hardware.
- **M2 Board Bring-Up — display + touchpad verified on hardware; R-001 closed.**
  The mikroBUS pin map ([02 §2.3.3](Docu/PrePlanning/02-Requirements.md#233-mikrobus--stm32g431-pin-mapping-con-004--r-001))
  is HW-confirmed with a logic analyzer: the Click Shield for Nucleo-64 routes
  slot 1 via the **ST-Morpho** headers, so SPI1 is **SCK=PB3 / MOSI=PB5 /
  DISP=PB4 / CS=PB12 / EXTCOMIN=PC8** — not the Arduino-header PA5/PA6/PA7, whose
  mismatch was the blank-display root cause. SPI/I2C/button/systick BSP +
  display/gfx/touchpad drivers on HAL; `display`, `touchpad`, `button` and the
  `lacheck` / `dispdiag` diagnostics pass; `run_ott.py` has an automatic suite.
  Remaining: the `touchdot` (display+touchpad) scenario. On-target checklist in
  [`Firmware/README.md`](Firmware/README.md#m2-hardware-verification-checklist).

## Build · flash · test (all from `Firmware/`)

```bash
# Build (arm-none-eabi-gcc + CMake, vendored CMSIS, no vendor HAL)
cmake -B build -G "Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.toolchain.cmake
cmake --build build -j                                   # -> build/pacman.elf, expect LD2 ~1 Hz

# Flash over ST-LINK V3
openocd -f openocd.cfg -c "program build/pacman.elf verify reset exit"

# Run an on-target test end-to-end (schedules, resets, reports over the VCP)
python3 Test/run_ott.py blinky --port /dev/ttyACM0       # exit 0 = PASS
```

Toolchain (verified): gcc-arm-none-eabi **13.2.1**, cmake **3.28**, openocd **0.12.0**
(`sudo apt-get install -y gcc-arm-none-eabi binutils-arm-none-eabi cmake openocd`).
After installing openocd, **unplug/replug the board once** so a non-root user can flash.

## Hardware facts

- STM32G431RB Nucleo-64; on-board **ST-LINK V3** = SWD debug **+** virtual COM port.
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
- **Adding an OTT test:** new `Firmware/Test/Target/scripts/ott_<name>.c/.h` + one row
  in `ott_scenarios.c`; nothing else changes.
- **Coding standard:** [c-code-style](https://github.com/MaxLell/c-code-style)
  (NFR-102). Fixes to the standard itself go via a separate PR to that repo, only
  with the owner's prior approval.
- **Verify on hardware** when a change has a runtime effect (build → flash → run
  the relevant OTT), the way M1 was verified.

## Git workflow

- Each milestone lands as its **own reviewed PR against `main`**; branch, don't
  commit to `main` directly.
- **Only push to this repo.** Never push to or modify other repos (e.g.
  c-code-style, EmbeddedCli) without explicit approval.
- After pushing to an open PR, re-check for new review comments.
