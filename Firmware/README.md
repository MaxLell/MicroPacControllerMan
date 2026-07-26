# Firmware — MicroPacControllerMan

STM32G431RB Nucleo-64 firmware. Built with **CMake + arm-none-eabi-gcc** against
**vendored CMSIS** (no vendor HAL yet), flashed with **OpenOCD** over ST-LINK V3.

## Milestone 1 — Toolchain Bring-Up

Goal: prove the whole toolchain end-to-end — build, flash, and run — and stand up
the base **On-Target Test (OTT)** infrastructure. The board blinks the on-board LED
**LD2 (PA5)** at ~1 Hz and serves an OTT CLI on the **ST-LINK virtual COM port**
(LPUART1, PA2/PA3, 115200 8N1). `ott blinky` drives PA5 and reads the pin back to
verify the blink mechanism automatically — target of
[VT-INT-005](../Docu/PrePlanning/06-Verification-and-Validation.md).

Init is register-level against CMSIS (see `App/main.c`); the default reset clock
(HSI 16 MHz) is used. The ST/ARM-provided device files — CMSIS core + STM32G4
device headers, `system_stm32g4xx.c`, the startup file and the linker script —
are vendored verbatim under `ThirdParty/STM32G431/` from ST's public repositories
(`STM32CubeG4` / `cmsis_device_g4`).

## Milestone 2 — Board Bring-Up

Adds the mikroBUS peripherals on the Click Shield for Nucleo-64: the **LCD Mono
Click** (Sharp LS013B7DH03, 128×128 mono, SPI1) in slot 1 and the **Touchpad
Click** (MTCH6102, I2C1) in slot 2, plus the on-board user button (B1/PC13).
New BSP transports (`Bsp/spi`, `Bsp/i2c`, `Bsp/button`, `Bsp/systick`), device
drivers (`Drivers/display`, `Drivers/gfx`, `Drivers/touchpad`), and three OTT
scenarios:

```
ott display     # geometric GFX demo (lines/rects/circles/triangles) — confirm on the LCD (VT-INT-006)
ott touchpad    # live X/Y + touch-present streamed to the console — confirm it tracks your finger (VT-INT-007)
ott touchdot    # a dot on the LCD follows your finger — confirms display + touchpad together
```

All three are **interactive**: they run until you press the USER button (B1),
with a 120 s safety cap. The pin map is **HW-confirmed** (logic analyzer, R-001
closed) and recorded in [02 §2.3.3](../Docu/PrePlanning/02-Requirements.md#233-mikrobus--stm32g431-pin-mapping-con-004--r-001).
Key facts: the Click Shield for Nucleo-64 mates with the **ST-Morpho** headers, so
slot-1 SPI1 = **SCK PB3 / MOSI PB5 / DISP PB4 (MISO line) / CS PB12 (active-high) /
EXTCOMIN PC8**; the MTCH6102 is at I2C address **0x25** on PB8/PB9; set the shield's
**VLS switch to 3V3**; the LCD's EXTMODE selects software VCOM (EXTMODE=low).

### M2 hardware-verification checklist

Shortcut: **`./m2.sh all`** builds, flashes once, and walks through the three
interactive tests in sequence (or `./m2.sh display|touchpad|touchdot|suite` for
one). Set the port with `PORT=/dev/ttyACMx ./m2.sh …`. The manual steps below are
the same thing spelled out.

Run on the physical board to close M2 (R-001 / VT-INT-003/004/006/007):

1. **Pre-power-on ([doc 08 §8.1](../Docu/PrePlanning/08-Troubleshooting-Guide.md)):** set each Click Shield socket's 3V3/5V switch to 3.3 V; seat LCD Mono in slot 1, Touchpad in slot 2; set LCD Mono JP1 to software-VCOM (EXTMODE=low).
2. **Pin map (VT-INT-003/004): DONE** — confirmed on hardware with a logic analyzer (R-001 closed). Slot-1 SPI1 = PB3/PB5/PB4/PB12/PC8, slot-2 I2C1 = PB8/PB9; see [02 §2.3.3](../Docu/PrePlanning/02-Requirements.md#233-mikrobus--stm32g431-pin-mapping-con-004--r-001). The `lacheck` OTT re-verifies wiring via per-pin fingerprints if you ever need to re-check.
3. **Build & flash**, then `python3 Test/run_ott.py --suite` — expect PASS for enumeration, banner, blinky.
4. **`python3 Test/run_ott.py display`** — watch the geometric patterns cycle on the LCD; press B1. If blank, see [doc 08 §8.4](../Docu/PrePlanning/08-Troubleshooting-Guide.md).
5. **`python3 Test/run_ott.py touchpad`** — move your finger; the streamed x/y should track it; press B1. "not responding" → check the I2C map / RST.
6. **`python3 Test/run_ott.py touchdot`** — the dot should follow your finger; press B1. If axes are swapped/mirrored, flip the mapping in `ott_touchdot.c`.

## Toolchain

Install the cross toolchain, build system, and flasher (Debian/Ubuntu):

```
sudo apt-get install -y gcc-arm-none-eabi binutils-arm-none-eabi cmake openocd
```

The `openocd` package also installs the ST-LINK udev rules; after installing,
**unplug/replug the board once** so a non-root user can flash over SWD (the serial
VCP already works without it).

> Verified with gcc-arm-none-eabi 13.2.1, cmake 3.28, openocd 0.12.0. No vendor
> IDE or STM32CubeMX is required. (A local, no-root install via xPack/Kitware
> tarballs to `~/.local/opt` also works if apt is unavailable.)

## Build & flash

```
cmake -B build -G "Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.toolchain.cmake
cmake --build build -j
openocd -f openocd.cfg -c "program build/pacman.elf verify reset exit"

```

Expected: **LD2 blinks at ~1 Hz**.

## On-Target Tests (OTT)

The firmware serves an OTT CLI on the VCP (built on the vendored **EmbeddedCli**
framework, `ThirdParty/EmbeddedCli/`); each test prints a machine-parseable
result without a debugger (FR-106 / FR-107):

```
help            # list CLI commands
ott             # list available OTT tests
ott blinky      # -> "OTT PASSED [blinky]" or "OTT FAILED [blinky]: <reason>"
```

`ott <name>` uses the **retained-RAM/reset flow** from
[doc 09](../Docu/PrePlanning/09-OTT-Mechanism-and-Reset-Flow.md): the command's
`setup` step writes a request (magic word + checksum + parameter blob) into a
`.noinit` RAM region that survives a software reset, then triggers
`NVIC_SystemReset()`. On the next boot `ott_execute_pending()` validates the
request, invalidates it (so a mid-test crash can't loop-boot the same test), runs
the scenario's `run` step, and prints the `OTT PASSED/FAILED [<name>]` result over
the console before falling through into normal operation. The ST-LINK VCP stays
enumerated across the target reset, so the host harness reads the result on the
same serial handle. This framework was brought forward from Milestone 2 so the
display/touchpad OTTs added there only need a new scenario module.

Run them from the host with the harness (stdlib Python, no pyserial):

```
python3 Test/run_ott.py --suite     # automatic regression: enumeration + banner + blinky
python3 Test/run_ott.py blinky      # one automatic test; exit 0 = PASS, 1 = FAIL, 2 = timeout
python3 Test/run_ott.py display     # interactive: streams live, confirm on the board, press B1
```

## Layout

The tree follows the layered layout of the reference project
([BareMetalHollowClockFw](https://github.com/MaxLell/BareMetalHollowClockFw)) —
`App` → `Drivers` → `Services` → `Bsp` → CMSIS, plus `Test`. Each layer has a
`Readme.md`; each module lives in its own folder. The full layer rules and
"what goes where" are the project's source of truth in
[03 Architecture §3.9](../Docu/PrePlanning/03-Architecture.md#39-firmware-source-tree-layout).

- `App/main.c` — runs any pending OTT (`ott_execute_pending`) on boot, then the
  super-loop: nominal ~1 Hz blink + OTT CLI polling.
- `Bsp/led/`, `Bsp/uart/` — register-level LD2 (PA5) and LPUART1 VCP drivers
  (`uart_flush` drains the last byte before the OTT reset).
- `Bsp/retain_ram/` — the `.noinit` retained-RAM object carrying the OTT request
  across the reset.
- `Bsp/systick/`, `Bsp/spi/`, `Bsp/i2c/`, `Bsp/button/` — M2 transports: 1 kHz
  time base, SPI1 (slot 1), I2C1 (slot 2), user button (PC13).
- `Drivers/display/`, `Drivers/gfx/`, `Drivers/touchpad/` — M2 device drivers:
  LS013B7DH03 framebuffer, geometric primitives, MTCH6102 touch read.
- `Services/` — reserved layer (Readme only for now); populated in
  Milestone 3+ (message broker, Active-Object base, etc.).
- `Test/Target/ott.c` — OTT core: the `ott` CLI command (schedule + reset) and the
  boot-time `ott_execute_pending()` runner/reporter.
- `Test/Target/ott_scenarios.c` — the OTT test registry; add a test by adding one row.
- `Test/Target/scripts/` — one module per OTT scenario: `ott_blinky` (VT-INT-005),
  `ott_display` (VT-INT-006), `ott_touchpad` (VT-INT-007), `ott_touchdot` (combined).
- `Test/run_ott.py` — host harness that drives an OTT and reports PASS/FAIL.
- `ThirdParty/EmbeddedCli/` — vendored [EmbeddedCli](https://github.com/MaxLell/EmbeddedCli)
  (CLI parser/dispatch) plus small `custom_assert.h`/`test_support.h` shims. Carries
  the memory-safety fixes from EmbeddedCli PR #2.
- `ThirdParty/STM32G431/` — ST/ARM-provided device support (not our code):
  `CMSIS/` (core + STM32G4 device headers and `system_stm32g4xx.c`),
  `startup_stm32g431xx.s` (vector table + reset handler), and
  `STM32G431RBTx_FLASH.ld` (128 KB FLASH / 32 KB RAM memory map incl. the
  `.noinit` region for the OTT retained-RAM mechanism). See its `Readme.md`.
- `cmake/arm-none-eabi.toolchain.cmake`, `CMakeLists.txt` — build.
- `openocd.cfg` — ST-LINK V3 + STM32G4 flash/debug.
