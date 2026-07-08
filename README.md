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
| [`Firmware/`](Firmware/README.md) | The STM32 firmware — CMake + arm-none-eabi-gcc, vendored CMSIS, OpenOCD. See its README for build/flash/test. |
| [`CLAUDE.md`](CLAUDE.md) | Quick-start + conventions (auto-loaded by Claude Code). |

## Quick start

Prerequisites (Debian/Ubuntu): `sudo apt-get install -y gcc-arm-none-eabi binutils-arm-none-eabi cmake openocd`.

```bash
cd Firmware
cmake -B build -G "Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.toolchain.cmake
cmake --build build -j
openocd -f openocd.cfg -c "program build/pacman.elf verify reset exit"   # LD2 blinks ~1 Hz
python3 Test/run_ott.py blinky --port /dev/ttyACM0                        # on-target self-test -> PASS
```

## Status

- **Milestone 1 — Toolchain Bring-Up: done.** Register-level blinky + the
  On-Target-Test (OTT) CLI framework with a retained-RAM/reset flow, verified on
  hardware via the Python harness.
- **Milestone 2 — Board Bring-Up: next.** Display, touchpad, and the confirmed
  mikroBUS pin mapping.

See [`Docu/PrePlanning/04-Implementation-Phases-and-Milestones.md`](Docu/PrePlanning/04-Implementation-Phases-and-Milestones.md).

## License

See [`LICENSE`](LICENSE).
