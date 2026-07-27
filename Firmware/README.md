# Firmware — MicroPacControllerMan

STM32G431RB Nucleo-64 firmware. Built with **CMake + arm-none-eabi-gcc** against the
**STM32CubeMX / STM32 HAL** export under `ThirdParty/`, flashed with **OpenOCD** over
ST-LINK V3.

## Toolchain

```
sudo apt-get install -y gcc-arm-none-eabi binutils-arm-none-eabi cmake openocd
```

The `openocd` package also installs the ST-LINK udev rules; after installing,
**unplug/replug the board once** so a non-root user can flash over SWD (the serial
VCP already works without it).

> Verified with gcc-arm-none-eabi 13.2.1, cmake 3.28, openocd 0.12.0. A local,
> no-root install via xPack/Kitware tarballs to `~/.local/opt` also works if apt is
> unavailable. STM32CubeMX itself is only needed to *change* the pin configuration —
> its output is committed.

## Build & flash

One tree, two builds ([03 §3.8](../Docu/PrePlanning/03-Architecture.md#38-build--toolchain)).

**Target firmware** (the default):

```
cmake -B build -G "Unix Makefiles"
cmake --build build -j
openocd -f openocd.cfg -c "program build/pacman.elf verify reset exit"
```

There is **no `-DCMAKE_TOOLCHAIN_FILE`**: the cross-compilation setup (target,
compiler, Cortex-M4F flags, `.elf` suffix) sits at the top of `CMakeLists.txt`, above
`project()`, which is when CMake reads it. One file describes the whole build. It
produces `build/pacman.elf` plus `.bin`/`.hex` and prints the flash/RAM usage.

**Host build** — no hardware, no cross-toolchain:

```
cmake -B build-host -DPACMAN_HOST_BUILD=ON -G "Unix Makefiles"
cmake --build build-host -j
```

`PACMAN_HOST_BUILD=ON` skips the cross-toolchain block entirely and builds
`pacman_host`, a static library of the modules that are genuinely hardware-independent
— today `Services/delay` and `Services/sw_timer`, plus the tick source through its host
implementation. It is what the SDL application (CON-103 / FR-104) will link against, and
it grows as more platform seams are cut (RF-003).

**Unit tests** run under Ceedling, which does its own compilation with mocked
dependencies — not through CMake:

```
ceedling test:all
```

Needs `ruby` and `gem install ceedling`. Configuration is `project.yml`; the tests live
in `Test/Host/`. See [`Test/Readme.md`](Test/Readme.md) for what belongs in a unit test
(everything above the BSP), how to mock a dependency, and how to assert that an
assertion fires.

`-Wall -Wextra` is on everywhere; the tree builds warning-free.

### How a platform port is shaped

`systick_bsp` is the worked example, and the pattern to copy: **one shared header, one
`.c` per platform, selected in `CMakeLists.txt`.** `systick_bsp.h` contains no HAL, so
`systick_bsp.c` (SysTick) and `systick_bsp_host.c` (`clock_gettime`) are
interchangeable and `Services/` never learns which one it got. Prefer this over
`#ifdef`s inside a module — the only conditional compilation left is where a platform
genuinely lacks the concept, such as `retain_ram`'s `.noinit` section, which no host
process has.

## On-Target Tests (OTT)

The firmware serves a command line on the ST-LINK virtual COM port (**LPUART1,
PA2/PA3, 115200 8N1**, usually `/dev/ttyACM0`), built on the vendored
**EmbeddedCli**. Each test prints a machine-parseable verdict with no debugger
attached (FR-106 / FR-107):

```
help                 # list CLI commands
ott                  # list available OTT tests
ott user_button      # -> "OTT PASSED [user_button]" or "OTT FAILED [user_button]: <reason>"
reset                # reboot into nominal mode (re-emits the boot banner)
```

`ott <name>` uses the **retained-RAM/reset flow** from
[doc 09](../Docu/PrePlanning/09-OTT-Mechanism-and-Reset-Flow.md): the command's
`setup` step writes a request (magic word + checksum + parameter blob) into a
`.noinit` RAM region that survives a software reset, then triggers
`NVIC_SystemReset()`. On the next boot `ott_execute_pending()` validates the request,
invalidates it (so a mid-test crash cannot loop-boot the same test), runs the
scenario's `run` step, prints the verdict, and falls through into normal operation.
The VCP is on the ST-LINK side, so it stays enumerated across the target reset and
the host harness reads the result on the same serial handle.

`Test/Target/ott.c` owns the request layout; `Bsp/retain_ram` only owns the 64-byte
buffer it lives in and knows nothing about its contents.

Run them from the host with the harness (stdlib Python, no pyserial):

```
python3 Test/run_ott.py --suite         # automatic: enumeration (VT-INT-001) + boot banner (VT-INT-002)
python3 Test/run_ott.py user_button     # interactive; exit 0 = PASS, 1 = FAIL, 2 = timeout
python3 Test/run_ott.py display         # interactive: streams live, confirm on the board, press B1
```

The four current scenarios are all **interactive** by design — the firmware
renders/prints and waits for you to confirm with B1, with a safety cap (30 s for
`user_button`, 120 s for the rest) so the board always returns to nominal mode:

```
ott user_button  # live button state + every debounced press; passes after 3 presses
ott display      # geometric GFX patterns — confirm on the LCD (VT-INT-006)
ott touchpad     # live x/y + touch-present on the console (VT-INT-007)
ott touchdot     # a dot on the LCD follows your finger — display + touchpad together
```

**Adding a scenario:** create `Test/Target/scripts/ott_<name>.c/.h` with a setup and
a run function, add one row to the table in `ott_scenarios.c`, and add the source to
`CMakeLists.txt`. Nothing in the OTT core or the CLI changes.

## Layout

Layered tree following the reference project
([BareMetalHollowClockFw](https://github.com/MaxLell/BareMetalHollowClockFw)) —
`App` → `Drivers` → `Services` → `Bsp` → HAL, plus `Test`. Each layer has a
`Readme.md`; each module lives in its own folder. The layer rules are the project's
source of truth in
[03 Architecture §3.9](../Docu/PrePlanning/03-Architecture.md#39-firmware-source-tree-layout).

| Module | What |
|---|---|
| `App/app_main.c` | Entry point, called from the generated `main()`. Initialises the platform, runs a pending OTT, prints the boot banner, then the super-loop. |
| `Bsp/dio_bsp/` | Digital I/O. **The only module that calls HAL GPIO** — everything else names a pin via `dio_bsp_pin_e`. |
| `Bsp/uart_bsp/` | Blocking console transport. The instance and the 115200 8N1 contract are `#define`s. |
| `Bsp/i2c_bsp/` | Blocking I2C master, device-agnostic, timeout-bounded, status enum. |
| `Bsp/spi_bsp/` | Blocking transmit-only SPI master. Chip-select belongs to the device driver. |
| `Bsp/systick_bsp/` | 1 kHz tick (`systick_bsp_get_tick`) plus a 1 ms callback hook. |
| `Bsp/switch/` | Reusable debounced-GPIO input primitive (32-sample history). |
| `Bsp/user_button/` | This board's B1 (PC13) instance of `switch`, incl. a latched press edge. |
| `Bsp/retain_ram/` | The `.noinit` byte buffer that survives a software reset. |
| `Drivers/display/` | The display **port**: shows a `framebuffer_t`. `display.c` = LS013B7DH03 panel over SPI, `display_host.c` = headless host. Owns the panel's inverted bit sense and its COM inversion. |
| `Drivers/touchpad/` | Touchpad Click (MTCH6102): a `touchpad_reading_t` per read. Not yet a port (RF-003). |
| `Services/delay/` | The blocking wait. One place to change when the RTOS arrives. |
| `Services/sw_timer/` | Non-blocking timers: every timeout and periodic job in the firmware. |
| `Services/framebuffer/` | A 1-bpp frame buffer — memory plus bit arithmetic, no hardware. Colours are logical: a set bit is ink. |
| `Services/gfx/` | Geometric primitives drawn into a frame buffer. Pure logic, fully host-tested. |
| `Services/circular_buffer/` | Generic fixed-capacity FIFO ring buffer, any element type, caller-supplied storage, no heap. |
| `Services/msg/` | Topic IDs, payload types and the message envelope (03 §3.3). Header-only. |
| `Services/msg_queue/` | A `msg_t`-typed skin over `circular_buffer`. |
| `Services/msg_broker/` | The publish/subscribe bus between modules (FR-103/108/110). Instance-based; output queues, not callbacks. |
| `Test/Host/` | Host unit tests (Ceedling + CMock). Cover everything above the BSP. |
| `Test/Target/` | The OTT core, the scenario registry, and one module per scenario. |
| `Test/run_ott.py` | Host harness that drives an OTT and reports PASS/FAIL. |
| `ThirdParty/EmbeddedCli/` | Vendored [EmbeddedCli](https://github.com/MaxLell/EmbeddedCli) plus the `custom_assert.h` / `test_support.h` shims. Carries the memory-safety fixes from its PR #2. |
| `ThirdParty/STM32_G431RB_HAL/` | The STM32CubeMX export (not our code): HAL + CMSIS, startup, linker script, and the clock/peripheral init in `Core/`. |
| `CMakeLists.txt` | The whole build, cross-toolchain included. |
| `openocd.cfg` | ST-LINK V3 + STM32G4 flash/debug. |
| `.clang-format` | The [c-code-style](https://github.com/MaxLell/c-code-style) config (NFR-102). |

### Conventions worth knowing before editing

- **All GPIO goes through `dio_bsp`.** To add a pin: give it a name in
  `dio_bsp_pin_e` and one row in `k_pin_map`. Do not call `HAL_GPIO_*` elsewhere.
- **No tick arithmetic.** Use `delay_ms()` when blocking is fine, `sw_timer` when it
  is not. There is no `millis()`; `systick_bsp_get_tick()` exists for `delay` and
  `sw_timer` and should not be needed above them.
- **HAL over registers.** Direct register access needs a reason in a comment. There
  is exactly one today: `uart_bsp_read_character()` reads `RDR`, because the HAL has
  no non-blocking single-character read.
- **Named constants, no bare literals**, and no abbreviations in identifiers
  (`in_length`, not `len`; `in_memory_address`, not `mem_addr`).
- `prv_` prefixes module-private functions, `g_` prefixes globals, `k_` prefixes
  file-scope constant tables, and pointer parameters carry `in_` / `out_` / `inout_`.

### Two things that survive a CubeMX re-generation only if you re-apply them

1. The **`.noinit` section** in `ThirdParty/STM32_G431RB_HAL/STM32G431xx_FLASH.ld`
   (just after `.bss`). It is marked NON-GENERATED in the script; without it the OTT
   reset flow silently stops working.
2. The **`app_main()` call** in the USER CODE block of the generated `Core/Src/main.c`.

The 1 ms tick hook is deliberately *not* on that list: `systick_bsp.c` provides a
strong `HAL_IncTick()`, overriding the HAL's `__weak` one, so the generated
`stm32g4xx_it.c` stays untouched and a re-generation cannot drop the hook.

One cosmetic item: `Core/Src/gpio.c` carries hand-added explanations on the generated
comments (`… : DISPLAY_DISP (PB4) high = panel on`). A re-generation drops those, but
the *code* it emits is identical, so nothing breaks — only the reading aid is lost.

**Editing the `.ioc` by hand** is fine for a pin *label*, which is what
`TOUCHPAD_RESET` was: change `P<pin>.GPIO_Label` in the `.ioc` and apply the same
rename to the `<label>_Pin` / `<label>_GPIO_Port` macros in `Core/Inc/main.h` and
their uses in `Core/Src/gpio.c`, and the result is byte-identical to what CubeMX
would generate. Do **not** hand-edit peripheral *settings* that way (clock tree,
FIFO modes, timing words): those fan out into initialisation code across several
generated files, and getting the `.ioc` and the code out of step is worse than
leaving the setting alone. Open CubeMX for those.

## Hardware notes

The Click Shield for Nucleo-64 (MIKROE-5193) mates with the **ST-Morpho** headers,
not the Arduino headers — that mismatch was the original blank-display cause. The
map is **HW-confirmed** with a logic analyzer (R-001 closed) and recorded in
[02 §2.3.3](../Docu/PrePlanning/02-Requirements.md#233-mikrobus--stm32g431-pin-mapping-con-004--r-001).

The clock tree is owned by the `.ioc` too: **SYSCLK 170 MHz** (PLL, `HSI/4 × 85`, the
part's maximum) with a **1 kHz SysTick**. Full table in
[02 §2.3.4](../Docu/PrePlanning/02-Requirements.md#234-clock-configuration-as-configured).
One consequence is worth internalising: a delay written as a *spin count* is ~10×
shorter at 170 MHz than at the 16 MHz this project started on, which is why the only
such delay left (the display's chip-select settle loop) carries a comment saying so.
Everything else uses `Services/delay` or `Services/sw_timer`, which are
clock-independent.

| Signal | Pin | Note |
|---|---|---|
| SPI1 SCK / MOSI | PB3 / PB5 | mikroBUS slot 1, mode 0, LSB-first, ~0.66 MHz |
| Display CS | PB12 | **active HIGH** |
| Display DISP | PB4 | the Click reuses the idle MISO line as the panel enable |
| Display EXTCOMIN | PC8 | mikroBUS PWM1, external VCOM clock |
| I2C1 SCL / SDA | PB8 / PB9 | mikroBUS slot 2, MTCH6102 at address **0x25** |
| Touchpad reset | PA4 | mikroBUS signal `RST`; `.ioc` label `TOUCHPAD_RESET`, active LOW |
| User button B1 | PC13 | **active HIGH** on this board (external pull-down) |
| Console | PA2 / PA3 | LPUART1 → ST-LINK VCP, 115200 8N1 |

Also: set the shield's **VLS switch to 3V3**, and leave the LCD's JP1 at
software-VCOM (EXTMODE low) — though the driver serves both modes on every
`display_service_vcom()` call, so either setting works.

Note there is no LD2 blink any more: PA5 is the Arduino-header SCK and was freed up
when slot 1 moved to ST-Morpho, but the LED test was retired with the M2 pivot.

## M2 hardware-verification checklist

Shortcut: **`./m2.sh all`** builds, flashes once, and walks through the four
interactive tests in sequence (or `./m2.sh user_button|display|touchpad|touchdot|suite`
for one). Set the port with `PORT=/dev/ttyACMx ./m2.sh …`. The manual steps below are
the same thing spelled out.

1. **Pre-power-on ([doc 08 §8.1](../Docu/PrePlanning/08-Troubleshooting-Guide.md)):** set each Click Shield socket's 3V3/5V switch to 3.3 V; seat LCD Mono in slot 1, Touchpad in slot 2; set LCD Mono JP1 to software-VCOM (EXTMODE=low).
2. **Pin map (VT-INT-003/004): DONE** — confirmed on hardware with a logic analyzer (R-001 closed); see the table above.
3. **Build & flash**, then `python3 Test/run_ott.py --suite` — expect PASS for enumeration and the boot banner.
4. **`python3 Test/run_ott.py user_button`** — press B1 three times; the console echoes each press. If nothing changes, PC13 is stuck.
5. **`python3 Test/run_ott.py display`** — watch the geometric patterns cycle on the LCD; press B1. If blank, see [doc 08 §8.4](../Docu/PrePlanning/08-Troubleshooting-Guide.md).
6. **`python3 Test/run_ott.py touchpad`** — move your finger; the streamed x/y should track it; press B1. "not responding" → check the I2C map / RST.
7. **`python3 Test/run_ott.py touchdot`** — the dot should follow your finger; press B1. If the axes are swapped or mirrored, flip `OTT_TOUCHDOT_SWAP_AXES` / `OTT_TOUCHDOT_INVERT_X` / `OTT_TOUCHDOT_INVERT_Y` in `ott_touchdot.c`.

## Milestone history

- **M1 — Toolchain Bring-Up.** Proved build/flash/run end-to-end and stood up the OTT
  framework with its retained-RAM/reset flow. Init was register-level against
  vendored CMSIS at that point.
- **M2 — Board Bring-Up.** Added the mikroBUS peripherals: LCD Mono Click (SPI1, slot
  1) and Touchpad Click (I2C1, slot 2), plus the user button. Partway through, init
  moved from register-level to **STM32CubeMX + the STM32 HAL** (DEC-012 in
  [11 Decisions & As-Built](../Docu/PrePlanning/11-Decisions-and-As-Built.md)), which
  also retired the register-level `blinky` OTT. The `lacheck` / `dispdiag`
  logic-analyzer diagnostics were removed once R-001 closed and the regular tests
  covered the same ground.
- **M3 — Game: next.** The pub-sub broker, the Active-Object tasks and the Pacman
  modules under `App/`.
