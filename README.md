# MicroPacControllerMan

A standalone embedded **Pacman** clone running on an **STM32G431RB Nucleo-64**:
directional input from a capacitive **Touchpad Click**, rendered on a 128×128
monochrome **LCD Mono Click**, with a single high score persisted in NVM. Beyond
the game, the project is a testbed for how far an AI coding agent can carry a
disciplined, requirements-driven embedded project from spec to firmware.

## Where things are

| Path | What |
|---|---|
| [`Docu/PrePlanning/`](Docu/PrePlanning/Index.md) | **Source of truth**: requirements (EARS), architecture, milestones, verification, risks, the OTT mechanism, and a decisions/as-built log. Start at `Index.md`. |
| [`Docu/Idea.md`](Docu/Idea.md) | The original idea capture (historical). |
| [`Firmware/`](Firmware/README.md) | The STM32 firmware — CMake + arm-none-eabi-gcc, STM32CubeMX/HAL, OpenOCD. See its README for build/flash/test. |
| [`Docu/Refactoring-Backlog.md`](Docu/Refactoring-Backlog.md) | Known work deliberately not done yet (`RF-xxx`) — deferrals, warts, and what "done" would look like for each. |
| [`CLAUDE.md`](CLAUDE.md) | Quick-start + conventions (auto-loaded by Claude Code). |

## Quick start

Prerequisites (Debian/Ubuntu): `sudo apt-get install -y gcc-arm-none-eabi binutils-arm-none-eabi cmake openocd`.
After installing `openocd`, unplug/replug the board once so a non-root user can flash over SWD.

```bash
cd Firmware
cmake -B build -G "Unix Makefiles"      # the cross-toolchain is configured inside CMakeLists.txt
cmake --build build -j
openocd -f openocd.cfg -c "program build/pacman.elf verify reset exit"
python3 Test/run_ott.py --suite         # enumeration + boot banner -> PASS
```

Then confirm the hardware interactively. Each test streams live and ends when you
press the USER button (B1):

```bash
python3 Test/run_ott.py --list          # what exists, and which kind each test is
python3 Test/run_ott.py --suite         # the automatic ones, unattended
python3 Test/run_ott.py --manual        # the ones you have to watch and confirm
python3 Test/run_ott.py joystick_dot    # or one by name
```

`./dev.sh all` does build + flash + both suites in sequence; `./dev.sh check` is what a
reviewer wants green.

## How the firmware is organised

Layered tree, one folder per module, following the reference project
[BareMetalHollowClockFw](https://github.com/MaxLell/BareMetalHollowClockFw). The
binding rules are [03 Architecture §3.9](Docu/PrePlanning/03-Architecture.md#39-firmware-source-tree-layout).

```
App/         entry point and (from M3) the game modules       -> may use anything below
Drivers/     device drivers: display, gfx, touchpad           -> uses Bsp + Services
Services/    middleware: delay, sw_timer                      -> nothing hardware-specific
Bsp/         board/MCU access: dio_bsp, i2c_bsp, spi_bsp,      -> uses the STM32 HAL only
             uart_bsp, systick_bsp, switch, user_button,
             retain_ram
Test/        on-target tests + the host harness
ThirdParty/  STM32CubeMX/HAL export, EmbeddedCli
```

Three rules carry most of the weight:

- **`dio_bsp` is the only module that calls HAL GPIO.** Everything else names a pin
  (`DIO_BSP_PIN_DISPLAY_CS`) instead of a port and bit mask, so the pin map lives in
  exactly one table.
- **Nothing does arithmetic on a tick counter.** `Services/delay` owns the blocking
  wait; `Services/sw_timer` owns every timeout and periodic job. There is no
  `millis()`.
- **A generic primitive and its instance are separate modules.** `Bsp/switch` is the
  reusable debounced-input primitive; `Bsp/user_button` is this board's B1 instance.
  The same split is expected for the game's inputs.

## On-target testing

The board runs its own tests and reports the verdict over the ST-LINK virtual COM
port, so no debugger is needed. `ott <name>` stores the request in a `.noinit` RAM
buffer, resets the board, and the next boot runs the test and prints
`OTT PASSED [<name>]` or `OTT FAILED [<name>]: <reason>`. Details in
[09 OTT Mechanism](Docu/PrePlanning/09-OTT-Mechanism-and-Reset-Flow.md); adding a
test is a new `Test/Target/scripts/ott_<name>.c` plus one registry row.

## Status

- **Milestone 1 — Toolchain Bring-Up: done.** Build/flash/run proven end-to-end,
  plus the On-Target-Test framework with its retained-RAM/reset flow.
- **Milestone 2 — Board Bring-Up: done.** LCD Mono Click (SPI1) and Touchpad Click
  (I2C1) verified on hardware; the mikroBUS pin map is confirmed with a logic
  analyzer (R-001 closed). The Click Shield mates with the **ST-Morpho** headers, so
  slot-1 SPI1 is **SCK PB3 / MOSI PB5 / DISP PB4 / CS PB12 / EXTCOMIN PC8** — not
  the Arduino-header pins, whose mismatch was the original blank-display cause.
- **Milestone 3 — Game: next.** The pub-sub broker, the Active-Object tasks and the
  Pacman modules under `App/`.

M2 was started register-level and moved onto STM32CubeMX + the STM32 HAL partway
through; the reasoning is DEC-012 in
[11 Decisions & As-Built](Docu/PrePlanning/11-Decisions-and-As-Built.md).

See [`Docu/PrePlanning/04-Implementation-Phases-and-Milestones.md`](Docu/PrePlanning/04-Implementation-Phases-and-Milestones.md).

## Coding standard

[c-code-style](https://github.com/MaxLell/c-code-style) (NFR-102), vendored as
`Firmware/.clang-format`: Allman braces, 4-space indent, 120 columns, `prv_` for
module-private functions, `g_` for globals, `in_`/`out_`/`inout_` on pointer
parameters, named constants instead of literals, and `ASSERT` on the preconditions
of public functions.

## License

See [`LICENSE`](LICENSE).
