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
(HSI 16 MHz) is used. CMSIS core + STM32G4 device headers, `system_stm32g4xx.c` and
the startup file are vendored under `cmsis/` and `startup/` from ST's public
repositories (`STM32CubeG4` / `cmsis_device_g4`); the linker script is `linker/`.

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
framework, `third_party/embedded_cli/`); each test prints a machine-parseable
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

Run them automatically from the host with the harness (stdlib Python, no pyserial):

```
python3 Test/run_ott.py blinky      # exit 0 = PASS, 1 = FAIL, 2 = timeout
```

The display/touchpad OTT scenarios are added in Milestone 2.

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
- `Drivers/`, `Services/` — reserved layers (Readme only for now); populated in
  Milestone 2+ (display/touchpad drivers, message broker, etc.).
- `Test/Target/ott.c` — OTT core: the `ott` CLI command (schedule + reset) and the
  boot-time `ott_execute_pending()` runner/reporter.
- `Test/Target/ott_scenarios.c` — the OTT test registry; add a test by adding one row.
- `Test/Target/scripts/ott_blinky.c` — the `blinky` scenario (VT-INT-005): drives
  PA5 and reads the pin back. Individual OTT scripts live in `Test/Target/scripts/`.
- `Test/run_ott.py` — host harness that drives an OTT and reports PASS/FAIL.
- `third_party/embedded_cli/` — vendored [EmbeddedCli](https://github.com/MaxLell/EmbeddedCli)
  (CLI parser/dispatch) plus small `custom_assert.h`/`test_support.h` shims. Carries
  the memory-safety fixes from EmbeddedCli PR #2.
- `cmsis/core/`, `cmsis/device/` — vendored CMSIS core + STM32G4 device (headers,
  `system_stm32g4xx.c`).
- `startup/startup_stm32g431xx.s` — vector table + reset handler.
- `linker/STM32G431RBTx_FLASH.ld` — memory map (128 KB FLASH, 32 KB RAM) incl. a
  `.noinit` region reserved for the later OTT retained-RAM mechanism.
- `cmake/arm-none-eabi.toolchain.cmake`, `CMakeLists.txt` — build.
- `openocd.cfg` — ST-LINK V3 + STM32G4 flash/debug.
