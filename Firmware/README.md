# Firmware — MicroPacControllerMan

STM32G431RB Nucleo-64 firmware. Built with **CMake + arm-none-eabi-gcc** against
**vendored CMSIS** (no vendor HAL yet), flashed with **OpenOCD** over ST-LINK V3.

## Milestone 1 — Toolchain Bring-Up

Goal: prove the whole toolchain end-to-end — build, flash, and see the board run —
by blinking the on-board LED **LD2 (PA5)** and printing a heartbeat banner on the
**ST-LINK virtual COM port** (LPUART1, PA2/PA3, 115200 8N1). Target of
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

# watch the ST-LINK VCP
stty -F /dev/ttyACM0 115200 raw -echo && cat /dev/ttyACM0
```

Expected: **LD2 blinks at ~1 Hz** and the console prints `heartbeat N` once per toggle.

## Layout

- `src/main.c` — application (blinky LD2 + serial heartbeat).
- `cmsis/core/`, `cmsis/device/` — vendored CMSIS core + STM32G4 device (headers,
  `system_stm32g4xx.c`).
- `startup/startup_stm32g431xx.s` — vector table + reset handler.
- `linker/STM32G431RBTx_FLASH.ld` — memory map (128 KB FLASH, 32 KB RAM) incl. a
  `.noinit` region reserved for the later OTT retained-RAM mechanism.
- `cmake/arm-none-eabi.toolchain.cmake`, `CMakeLists.txt` — build.
- `openocd.cfg` — ST-LINK V3 + STM32G4 flash/debug.
