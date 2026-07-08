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

Init is register-level against CMSIS (see `src/main.c`); the default reset clock
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

The firmware serves an OTT CLI on the VCP; each test prints a machine-parseable
result without a debugger (FR-106 / FR-107):

```
ott list        # list available tests
ott blinky      # -> "OTT PASSED [blinky]" or "OTT FAILED [blinky]: <reason>"
```

Run them automatically from the host with the harness (stdlib Python, no pyserial):

```
python3 test/run_ott.py blinky      # exit 0 = PASS, 1 = FAIL, 2 = timeout
```

Tests that need a clean reset state (retained-RAM/reset, doc 09) and the
display/touchpad OTTs are added in Milestone 2.

## Layout

- `src/main.c` — super-loop: nominal ~1 Hz blink + OTT CLI polling.
- `src/led.c`, `src/uart.c` — LD2 (PA5) and LPUART1 VCP drivers.
- `src/ott.c` — OTT CLI framework + test registry (the `blinky` test).
- `test/run_ott.py` — host harness that drives an OTT and reports PASS/FAIL.
- `cmsis/core/`, `cmsis/device/` — vendored CMSIS core + STM32G4 device (headers,
  `system_stm32g4xx.c`).
- `startup/startup_stm32g431xx.s` — vector table + reset handler.
- `linker/STM32G431RBTx_FLASH.ld` — memory map (128 KB FLASH, 32 KB RAM) incl. a
  `.noinit` region reserved for the later OTT retained-RAM mechanism.
- `cmake/arm-none-eabi.toolchain.cmake`, `CMakeLists.txt` — build.
- `openocd.cfg` — ST-LINK V3 + STM32G4 flash/debug.
