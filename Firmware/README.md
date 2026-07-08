# Firmware — MicroPacControllerMan

STM32G431RB Nucleo-64 firmware. Built with **CMake + arm-none-eabi-gcc** against
**vendored CMSIS** (no vendor HAL yet), flashed with **OpenOCD** over ST-LINK V3.

## Milestone 1 — Toolchain Bring-Up

Goal: prove the whole toolchain end-to-end — build, flash, and see the board run —
by blinking the on-board LED **LD2 (PA5)** and printing a heartbeat banner on the
**ST-LINK virtual COM port** (USART2, PA2/PA3, 115200 8N1). Target of
[VT-INT-005](../Docu/PrePlanning/06-Verification-and-Validation.md).

Init is register-level against CMSIS (see `src/main.c`); the default reset clock
(HSI 16 MHz) is used. CMSIS core + STM32G4 device headers, `system_stm32g4xx.c` and
the startup file are vendored under `cmsis/` and `startup/` from ST's public
repositories (`STM32CubeG4` / `cmsis_device_g4`); the linker script is `linker/`.

## Toolchain (installed locally, no root)

The embedded toolchain is installed under `~/.local/opt` (prebuilt xPack/Kitware
binaries — no `sudo`), sourced via an env file:

```
# one-time install (versions as used here)
mkdir -p ~/.local/opt
curl -fL https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/download/v15.2.1-1.1/xpack-arm-none-eabi-gcc-15.2.1-1.1-linux-x64.tar.gz | tar xz -C ~/.local/opt
curl -fL https://github.com/xpack-dev-tools/openocd-xpack/releases/download/v0.12.0-7/xpack-openocd-0.12.0-7-linux-x64.tar.gz | tar xz -C ~/.local/opt
curl -fL https://github.com/Kitware/CMake/releases/download/v4.3.3/cmake-4.3.3-linux-x86_64.tar.gz | tar xz -C ~/.local/opt

# put them on PATH (this session and future)
source ~/.local/opt/pacman-toolchain-env.sh
```

**One-time udev rule** so OpenOCD can access the ST-LINK USB interface without root
(the serial VCP works without it, SWD flashing does not):

```
sudo cp ~/.local/opt/xpack-openocd-0.12.0-7/openocd/contrib/60-openocd.rules /etc/udev/rules.d/60-openocd.rules
sudo udevadm control --reload-rules && sudo udevadm trigger
# then unplug/replug the board once
```

## Build & flash

```
source ~/.local/opt/pacman-toolchain-env.sh
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
