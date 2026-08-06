# Firmware — MicroPacControllerMan

STM32U545RE-Q Nucleo-64 firmware. Built with **CMake + arm-none-eabi-gcc** against the
**STM32CubeMX / STM32 HAL** export under `ThirdParty/`, flashed with
**STM32CubeProgrammer** over ST-LINK V3E.

> The game is complete and plays on the board, and the menu offers **three games**: the arcade's own
> maze, the same maze played by the trained agent alone, or a **randomly generated maze per level**
> (FR-029/FR-040..043), each with its own high scores — RAM **71.6 %**
> (187,584 of 256 kB), flash **21.6 %** (111,504 of the 504 kB left
> after the high-score page), both builds warning-free, **449** host unit tests green. **Every
> requirement in the spec has a passing test**, FR-037's play strength included, though
> [M6 §14](../Docu/Design/M6-Pacman-AI.md) records how narrow that margin is; the two timing budgets
> that used to sit here as unmet are **withdrawn** rather than satisfied
> ([DEC-036](../Docu/PrePlanning/11-Decisions-and-As-Built.md)). The known edges are recorded in
> the [Refactoring Backlog](../Docu/Refactoring-Backlog.md); the milestone record is
> [04 §4.2](../Docu/PrePlanning/04-Implementation-Phases-and-Milestones.md#42-close-out) and
> [§4.3](../Docu/PrePlanning/04-Implementation-Phases-and-Milestones.md#43-milestone-5--random-mazes).

## Toolchain

**Or skip all of it and use the container** — see [Docker](#docker) below. On another machine that is
the shorter road: `./dev.sh docker` and you are building.

```
sudo apt-get install -y gcc-arm-none-eabi binutils-arm-none-eabi \
                        libnewlib-arm-none-eabi cmake openocd
```

`libnewlib-arm-none-eabi` is the target's C library — `math.h`, `libc.a`, `libm.a`, which the HAL
includes. It is only a *Recommends* of `gcc-arm-none-eabi`, so an ordinary `apt-get install` pulls it
along and it used to go unmentioned here. Anything installing without recommends does not get it, and
then the HAL fails to compile on `math.h: No such file or directory` — which is exactly how the first
build of the container failed.

Plus **STM32CubeProgrammer** for flashing, from
[st.com](https://www.st.com/en/development-tools/stm32cubeprog.html) (an ST account is
needed for the download). Its `STM32_Programmer_CLI` must be on `PATH` — symlinking it
into `~/.local/bin` is enough. Override the location with
`PROGRAMMER=/path/to/STM32_Programmer_CLI`.

`openocd` is still installed for two reasons: it provides the ST-LINK udev rules, and
it is the gdb server for debugging. After installing it, **unplug/replug the board
once** so a non-root user can reach SWD (the serial VCP already works without it).

**Why not flash with openocd?** It attaches to this part fine, but openocd 0.12.0 —
the newest Ubuntu noble ships — only knows `STM32U57/U58xx` (device ID 0x482) in its
flash driver. This board reports **0x455** (STM32U535/U545), so `program` fails with
`auto_probe failed`. U535/U545 support landed after 0.12.0. Details in `openocd.cfg`.

> Verified with gcc-arm-none-eabi 13.2.1, cmake 3.28, openocd 0.12.0 (debug only),
> STM32CubeProgrammer 2.23.0. A local, no-root install via xPack/Kitware tarballs to
> `~/.local/opt` also works if apt is unavailable. STM32CubeMX itself is only needed
> to *change* the pin configuration — its output is committed.

## Build & flash

One tree, two builds ([03 §3.8](../Docu/PrePlanning/03-Architecture.md#38-build--toolchain)).

**Target firmware** (the default):

```
cmake -B build -G "Unix Makefiles"
cmake --build build -j
STM32_Programmer_CLI -c port=SWD -w build/pacman.elf -v -rst
```

There is **no `-DCMAKE_TOOLCHAIN_FILE`**: the cross-compilation setup (target,
compiler, Cortex-M33 flags, `.elf` suffix) sits at the top of `CMakeLists.txt`, above
`project()`, which is when CMake reads it. One file describes the whole build. It
produces `build/pacman.elf` plus `.bin`/`.hex` and prints the flash/RAM usage.

**Host build** — no hardware, no cross-toolchain:

```
cmake -B build-host -DPACMAN_HOST_BUILD=ON -G "Unix Makefiles"
cmake --build build-host -j
```

`PACMAN_HOST_BUILD=ON` skips the cross-toolchain block entirely and builds
`pacman_host`, a static library of the modules that are genuinely hardware-independent —
by the end that is the whole of `App/` and `Services/`, with timing, display and flash
each reached through their host port. `pacman_host_app` (CON-103 / FR-104) links against
it, which is what makes the SDL window evidence about this firmware rather than about a
lookalike.

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

## Docker

For a second machine, or for not installing any of the above:

```
./dev.sh docker              # a shell in the image, this repository mounted
./dev.sh docker check        # formatting + unit tests + both builds, inside it
./dev.sh docker-build        # rebuild the image after editing docker/Dockerfile
```

The image is `Firmware/docker/Dockerfile`, built on **Ubuntu 24.04 on purpose**: every tool version
this project has been verified against is the one noble packages — gcc-arm-none-eabi 13.2.1,
cmake 3.28.3, clang-format 18, ruby 3.2, SDL2 2.30 — so `apt-get install` reproduces the verified
toolchain rather than approximating it. Ceedling is pinned to 1.1.1, the version the suite has been
run against throughout, because Ceedling generates the mocks and the runners and a different version
is a different test build.

**The repository is mounted, not copied.** An edit on the host is an edit in the container, and a
build artefact belongs to whoever ran the command: `dev.sh docker` passes the host's user id
through. The repository *root* is mounted at `/work` and the working directory is `/work/Firmware`,
because `.git` lives a level above the firmware and `install-hook` needs it.

The trainer's Python is in the image, at `/opt/venv` and first on `PATH`. So inside the container
there is no `Training/.venv` to create and the commands are plain `python3`:

```
python3 Training/train.py
python3 Training/evaluate.py
```

### One build directory per path

`build/`, `build-host/` and `build-test/` carry the absolute path they were configured at, so a
directory made on the host cannot be used inside the container — the tree is `/work/Firmware` in
there — or the other way round. `dev.sh` notices and says so with the fix, which is to delete the
directory; nothing is lost, it is all regenerated. If you switch between the two often, keep the
habit of `rm -rf build build-host build-test` when you cross over, or simply work in one of them.

### If it says the daemon cannot be reached

```
permission denied while trying to connect to the docker API at unix:///var/run/docker.sock
```

One error from Docker, three different faults behind it — so `dev.sh` checks which one before it
builds and tells you that, instead of listing all three:

| what it finds | what it means | the fix |
|---|---|---|
| `DOCKER_HOST` is set | not about the local socket at all | unset it, or fix what it points at |
| no socket at `/var/run/docker.sock` | the daemon is not running | `sudo systemctl enable --now docker` |
| socket there, you are in the `docker` group | the membership has not reached *this shell* — a group is read at login | `newgrp docker`, or log out and in |
| socket there, you are not in the group | the group owns the socket and a fresh install puts nobody in it | `sudo usermod -aG docker "$USER" && newgrp docker` |

**Do not use `sudo ./dev.sh docker` as a workaround.** It works, and then every artefact the build
writes into your tree belongs to root — including `build/`, `build-test/` and Ceedling's caches, which
you then cannot rebuild without sudo either. `dev.sh` does take `SUDO_UID` into account so that it is
merely unnecessary rather than destructive, but the group is the fix.

`DEPRECATED: The legacy builder is deprecated` alongside it is only a notice: the legacy builder
builds this image correctly. `sudo apt-get install docker-buildx` silences it.

### What the container cannot do

**Flashing.** `STM32_Programmer_CLI` comes from STM32CubeProgrammer, which is behind an ST account
and cannot be fetched unattended, so the image does not have it. Mount the host's install and point
`dev.sh` at it — it already takes the override:

```
docker run --rm -it     --user "$(id -u):$(id -g)"     --volume "$PWD/..:/work" --workdir /work/Firmware     --volume /opt/st:/opt/st:ro     --device /dev/ttyACM0 --volume /dev/bus/usb:/dev/bus/usb     --env PROGRAMMER=/opt/st/stm32cubeprg/bin/STM32_Programmer_CLI     micropac-dev ./dev.sh suite
```

`--device /dev/ttyACM0` is the serial console and is enough for `run_ott.py` on its own;
`/dev/bus/usb` is what the programmer needs to reach the ST-LINK. `./dev.sh docker` adds the serial
device for you when a board is plugged in, and nothing else — the USB bus and somebody else's
`/opt` are not things it should mount behind your back.

**The window, unless you hand it a display.** `pacman_host_app` opens an SDL window.
`./dev.sh docker` passes `DISPLAY` and `/tmp/.X11-unix` through when there is a display to pass, so
under X11 it works; under Wayland you will need `xhost` or an Xwayland socket. Building and testing
need no display at all.

> **Not yet verified.** The machine this was written on has no Docker installed, so the image has
> never been built. Everything in it is either a package whose exact version is installed and
> working on that machine, or a line of the same `dev.sh` the host uses — but "should work" is not
> "does", and the first `./dev.sh docker check` on a machine with Docker is what will say. If it
> fails, the likely places are the two the author could not exercise: the `gem install` and the
> `pip install`, both of which want a network at build time.

## Training overnight, and stopping it

`Training/campaign.py` runs the configured runs one after another and writes
`Training/campaign/summary.md` after each. It says at the start when it expects to be finished. The
only thing to edit for a longer or shorter night is each run's `hours`; the campaign's ceiling is
derived from them.

Detached, so it survives the terminal:

```
docker run -d --name micropac-train \
    --user "$(id -u):$(id -g)" \
    -v "$PWD/..:/work" -w /work/Firmware \
    micropac-dev python3 Training/campaign.py
docker logs -f micropac-train
```

**To stop it:** `docker stop -t 30 micropac-train`. Nothing is lost — `train.py` writes its winner on
every improvement rather than at the end — and the campaign is resumable, so a run whose winner file
already exists is measured rather than repeated.

Outside a container the order matters, and there is a trap in it:

```
pkill -f "[c]ampaign.py" && sleep 2 && pkill -f "[t]rain.py --out"
```

The campaign first. `pkill -f train.py` on its own matches the pool workers as well as the parent,
kills them, and leaves the parent waiting in `pool.map` for results that will never arrive.

**More cores do not finish sooner.** The budget is wall-clock: `train.py --max-seconds` stops between
generations, so a bigger machine spends the same night doing more generations. That is deliberate — a
generation is not a fixed amount of work, since a better agent lives longer and its episodes take
longer.

## After training: taking a winner into the firmware

Training produces `Training/winner.json` (or a file per run under `Training/campaign/`, which is not
in git). Getting one of those into the firmware is four files and one **order that matters**, so it
is a command rather than a recipe:

```
./dev.sh adopt-weights                        # Training/winner.json
./dev.sh adopt-weights Training/campaign/normal-seed1.json
./dev.sh adopt-weights --force <file>         # one that does not meet FR-037
```

It measures the winner first and **refuses to adopt one that fails VT-UNIT-010**, because training
produces a winner every time including a bad one, and the thing that must not happen quietly is a
worse agent replacing a better one. Then it exports `App/pacman_ai/ai_weights.[ch]`, rebuilds the
host library and **re-records the FR-039 state set** — in that order. Exporting weights changes what
the target computes, so a state set recorded against the old ones is a recording of a different
network; `ott ai_equivalence` refuses to run on a digest mismatch, which is the safety net, and this
is what keeps you off it.

Commit the four files together:

```
Training/winner.json
App/pacman_ai/ai_weights.c
App/pacman_ai/ai_weights.h
Test/Target/scripts/ott_ai_equivalence_states.c
```

Then, on the machine with the board, `./dev.sh suite`. `ott ai_equivalence` is what proves the port
agrees with the host about the *new* weights, and it is the only thing that can.

## On-Target Tests (OTT)

The firmware serves a command line on the ST-LINK virtual COM port (**USART1,
PA9/PA10, 115200 8N1**, usually `/dev/ttyACM0`), built on the vendored
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
python3 Test/run_ott.py --suite         # automatic: enumeration, boot banner, and every self-judging test
python3 Test/run_ott.py user_button     # interactive; exit 0 = PASS, 1 = FAIL, 2 = timeout
```

```
ott user_button  # live button state + every debounced press; passes after 3 presses
```

`user_button` is **interactive**: the firmware streams the live state and waits for you to
press B1, with a 30 s safety cap so the board always returns to nominal mode.

`display_id` is the automatic one: it resets the controller and reads its identification
registers, and the display either answers or it does not. Everything else needs an operator,
because what they check — a colour that is genuinely red, a name that matches the key pushed,
motion that looks smooth — is not something the firmware can judge about itself.

**Adding a scenario:** create `Test/Target/scripts/ott_<name>.c/.h` with a run function
— plus a setup function only if the test takes console arguments, otherwise the table
carries `NULL` — add one row to the table in `ott_scenarios.c`, and add the source to
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
| `App/playfield/`, `maze_gen/`, `agent/`, `pacman/`, `ghost_path/`, `ghost/`, `score/`, `game/`, `sprite_set/`, `game_view/`, `render/` | The Pacman game — pure logic, no hardware, host-testable in full. See [App/Readme.md](App/Readme.md). `maze_gen` builds the level's maze (FR-029, [M4 Random Mazes](../Docu/Design/M4-Random-Mazes.md)); `playfield` turns a maze into rules and `game_view` derives its appearance from the same map. |
| `Bsp/dio_bsp/` | Digital I/O. **The only module that calls HAL GPIO** — everything else names a pin via `dio_bsp_pin_e`. |
| `Bsp/uart_bsp/` | Blocking console transport. The instance and the 115200 8N1 contract are `#define`s. |
| `Bsp/systick_bsp/` | 1 kHz tick (`systick_bsp_get_tick`) plus a 1 ms callback hook. |
| `Bsp/switch/` | Reusable debounced-GPIO input primitive (32-sample history). |
| `Bsp/user_button/` | This board's B1 (PC13) instance of `switch`, incl. a latched press edge. |
| `Bsp/joystick/` | The shield's five-key joystick — five instances of `switch`, latched press edge per key. |
| `Bsp/spi_bsp/` | Blocking SPI master on SPI1, full duplex (reading a controller's ID register needs it). |
| `Bsp/retain_ram/` | The `.noinit` byte buffer that survives a software reset. |
| `Drivers/st7789/` | The ST7789V controller: init, ID self-check, rectangles, pixel blits with byte-order handling. |
| `Drivers/display/` | The display **port** the game sees: shows a `framebuffer_t`, whole or by rectangle (`display_present_region`, the lever behind the frame rate). `display.c` drives the ST7789V; `display_host.c` keeps the frame in memory for the host build. |
| `Services/delay/` | The blocking wait — the one place the firmware may burn time, so an overrun has a single suspect. |
| `Services/sw_timer/` | Non-blocking timers: every timeout and periodic job in the firmware. |
| `Services/framebuffer/` | A 240 x 320 **RGB565** frame buffer — memory plus the arithmetic to address it, no hardware. 153,600 bytes, so it lives in static storage. |
| `Services/gfx/` | Geometric primitives drawn into a frame buffer. Pure logic, fully host-tested. |
| `Services/sprite/` | Indexed pictures with transparency and a palette chosen at draw time. A sprite is stored as **text**, one character per pixel, so the art is editable in the source with no tooling. |
| `Services/active_object/` | The Active-Object template ([03 §3.5](../Docu/PrePlanning/03-Architecture.md#35-software-module-template-active-object)). One user in the end — `App/score` is built on it ([DEC-027](../Docu/PrePlanning/11-Decisions-and-As-Built.md)). |
| `Services/circular_buffer/` | Generic fixed-capacity FIFO ring buffer, any element type, caller-supplied storage, no heap. |
| `Services/msg/` | Topic IDs, payload types and the message envelope (03 §3.3). Header-only. |
| `Services/msg_queue/` | A `msg_t`-typed skin over `circular_buffer`. |
| `Services/msg_broker/` | The publish/subscribe bus between modules (FR-103/108/110). Instance-based; output queues, not callbacks. |
| `Services/neural_net/` | The feed-forward evaluator the trained agent runs on: an arbitrary NEAT topology held entirely in `const` data, no allocation, no state between calls. `float` only, a fixed accumulation order and ReLU only — all three for FR-039, because the host would otherwise promote to `double` and two libm implementations need not agree about `tanh`. `neural_net_is_well_formed` checks the table rather than trusting it. |
| `App/pacman_ai/` | The instance: 23 features in Pacman's own frame, four relative actions, and the one trained network the firmware carries (`ai_weights.c`, **generated** — see below). `pacman_ai_decide` is the whole decision in one call, which is what the target and the host both go through. |
| `Training/` | Host-only, and not firmware ([DEC-040](../Docu/PrePlanning/11-Decisions-and-As-Built.md)): the game as a shared library, the NEAT loop, the play-strength measurement, the weight exporter and the FR-039 state recorder. Nothing in the target build refers to it. |
| `Test/Host/` | Host unit tests (Ceedling + CMock). Cover everything above the BSP. |
| `Test/Target/` | The OTT core, the scenario registry, the one shared frame buffer (`ott_framebuffer`), and one module per scenario. |
| `Test/run_ott.py` | Host harness that drives an OTT and reports PASS/FAIL. |
| `App/pacman_ai/ai_weights.[ch]`, `Test/Target/scripts/ott_ai_equivalence_states.c` | **Generated, do not edit.** The trained weights come from `Training/export_c.py`; the recorded FR-039 states come from `./build-host/pacman_ai_record`. The second belongs to the first — the recording carries the weight table's digest and `ott ai_equivalence` refuses to run against a different one, so re-exporting means re-recording. |
| `ThirdParty/EmbeddedCli/` | Vendored [EmbeddedCli](https://github.com/MaxLell/EmbeddedCli) plus the `custom_assert.h` / `test_support.h` shims. Carries the memory-safety fixes from its PR #2. |
| `ThirdParty/STM32_U545RE_HAL/` | The STM32CubeMX export (not our code): HAL + CMSIS, startup, linker script, and the clock/peripheral init in `Core/`. The `.noinit` block in the linker script is ours and must be re-added after every regeneration. |
| `dev.sh` | The umbrella command: builds, tests, formats, flashes, runs the OTTs, and installs the commit hook. Not a second implementation of any of them — it calls `format.sh`, `ceedling` and `Test/run_ott.py`, so each job has one definition however it is reached. |
| `format.sh` | The single definition of "formatted" (NFR-102): the whole tree, a path, or just what is staged. |
| `CMakeLists.txt` | The whole build, cross-toolchain included. |
| `openocd.cfg` | ST-LINK V3E + STM32U5 **debug only** — flashing is STM32CubeProgrammer's job. |
| `.clang-format` | The [c-code-style](https://github.com/MaxLell/c-code-style) config (NFR-102). |

### Conventions worth knowing before editing

- **All GPIO goes through `dio_bsp`.** To add a pin: give it a name in
  `dio_bsp_pin_e` and one row in `g_pin_map`. Do not call `HAL_GPIO_*` elsewhere.
- **No tick arithmetic.** Use `delay_ms()` when blocking is fine, `sw_timer` when it
  is not. There is no `millis()`; `systick_bsp_get_tick()` exists for `delay` and
  `sw_timer` and should not be needed above them.
- **HAL over registers.** Direct register access needs a reason in a comment. There
  is exactly one today: `uart_bsp_read_character()` reads `RDR`, because the HAL has
  no non-blocking single-character read.
- **Named constants, no bare literals**, and no abbreviations in identifiers
  (`in_length`, not `len`; `in_memory_address`, not `mem_addr`).
- `prv_` prefixes module-private functions, `g_` prefixes globals **and file-scope
  statics including const lookup tables**, and pointer parameters carry `in_` /
  `out_` / `inout_`.

### Two things that survive a CubeMX re-generation only if you re-apply them

1. The **`.noinit` section** in `ThirdParty/STM32_U545RE_HAL/STM32U545xx_FLASH.ld`
   (just after `.bss`). It is marked NON-GENERATED in the script; without it the OTT
   reset flow silently stops working.
2. The **`app_main()` call** in the USER CODE block of the generated `Core/Src/main.c`.

The 1 ms tick hook is deliberately *not* on that list: `systick_bsp.c` provides a
strong `HAL_IncTick()`, overriding the HAL's `__weak` one, so the generated
`stm32u5xx_it.c` stays untouched and a re-generation cannot drop the hook.

**Editing the `.ioc` by hand** is fine for a pin *label*, which is what
`USER_BUTTON` and `JOYSTICK_NORTH` are: change `P<pin>.GPIO_Label` in the `.ioc` and apply the same
rename to the `<label>_Pin` / `<label>_GPIO_Port` macros in `Core/Inc/main.h` and
their uses in `Core/Src/gpio.c`, and the result is byte-identical to what CubeMX
would generate. Do **not** hand-edit peripheral *settings* that way (clock tree,
FIFO modes, timing words): those fan out into initialisation code across several
generated files, and getting the `.ioc` and the code out of step is worse than
leaving the setting alone. Open CubeMX for those.

## Hardware notes

**STM32U545RE-Q Nucleo-64**, Cortex-M33 at **160 MHz**, TrustZone off
(`CORTEX_M33_NS`). 512 KB flash; RAM is **256 KB contiguous** at `0x20000000` plus a
separate **16 KB SRAM4** at `0x28000000`. That headroom is the reason for the MCU
choice: a 240x320 RGB565 frame buffer is 153.6 kB and did not fit the 32 kB of the
STM32G431RB this project started on.

The clock tree is owned by the `.ioc`: PLL1 from **MSIS at 48 MHz** (`M=3, N=10,
R=1`) to 160 MHz SYSCLK, with a **1 kHz SysTick**. Never express a delay as a spin
count — use `Services/delay` or `Services/sw_timer`, which are clock-independent.

| Signal | Pin | Note |
|---|---|---|
| Console | PA9 / PA10 | USART1 -> ST-LINK V3E VCP, 115200 8N1 |
| User button B1 | PC13 | **active HIGH** — idle low, confirmed by reading `GPIOC->IDR` over SWD |
| SWD | PA13 / PA14 | SWDIO / SWCLK |

The **X-NUCLEO-GFX01M2** carries the display (**ST7789V**, 240x320 colour over SPI),
a 5-GPIO joystick, and a 64-Mbit SPI flash we deliberately do not use. Its pin map could
not be read out of UM2750 — that manual lists no STM32U5 board at all — so it was derived
by connector position against UM3062 and then **confirmed on hardware**, by tests that make
the parts answer rather than by a multimeter. Two traps worth knowing before touching it:
chip select is **active LOW** where UM2750 says high, and register reads carry a one-*bit*
dummy, so a byte-aligned read comes back looking like plausible garbage. The map and the
measurements are in [M2 Board Bring-Up](../Docu/Design/M2-Board-Bring-Up.md).

## Verification — done on hardware

**`./dev.sh all`** builds, flashes and walks both OTT suites. Set the port with
`PORT=/dev/ttyACMx ./dev.sh ...`. What has actually been confirmed on this board:

1. **Build** — target and host, warning-free. Flash 96,292 B (18.7 % of the 504 kB left
   after the high-score page), RAM 178,244 B (68.0 % of 256 kB).
2. **`.noinit` present** in the ELF right after `.bss`, so the OTT reset flow has its
   retained RAM.
3. **Flash + verify** with `STM32_Programmer_CLI` — it reports device ID `0x455`,
   "STM32U535/STM32U545", and verifies the download.
4. **Boot banner** over the VCP:
   `MicroPacControllerMan booted. Type 'ott' for tests, 'reset' to restart the game.`
5. **CLI answers**: `help` lists its commands; `ott` lists the scenarios.
6. **The suites pass** — `run_ott.py --suite` unattended, `--manual` with an operator at
   the board, including `ott pacman`, which plays a real run and reports the frame cost.
7. **371 host unit tests green** under `ceedling test:all`.

## Milestone history

- **M1 — Toolchain Bring-Up.** Proved build/flash/run end-to-end and stood up the OTT
  framework with its retained-RAM/reset flow. Init was register-level against
  vendored CMSIS at that point.
- **M2 — Board Bring-Up (first board).** Added the mikroBUS peripherals: LCD Mono Click
  (SPI1, slot 1) and Touchpad Click (I2C1, slot 2), plus the user button. Partway through,
  init moved from register-level to **STM32CubeMX + the STM32 HAL** (DEC-012 in
  [11 Decisions & As-Built](../Docu/PrePlanning/11-Decisions-and-As-Built.md)), which
  also retired the register-level `blinky` OTT. The `lacheck` / `dispdiag`
  logic-analyzer diagnostics were removed once R-001 closed and the regular tests
  covered the same ground.
- **M3 — Game (host only), first attempt.** The broker, the Active-Object base and the
  Pacman modules under `App/`, playable on the host via SDL. Opened as PR #10 and
  **parked**; `host_main.c` was the last thing salvaged from it.
- **Restart on new hardware.** After the PR #10 review the project went back to M1 on the
  **STM32U545RE-Q**, with the mikroBUS Clicks replaced by the X-NUCLEO-GFX01M2 and Pacman
  going colour. The drivers and OTTs for the old parts are gone, and so are the acceptance
  tests that described them — a test naming hardware nobody has misleads more than it
  documents.
- **M2 — Board Bring-Up (this board).** The panel turned out to be an **ST7789V**, not the
  ILI9341 this project had been assuming. Display, joystick, the RGB565 frame buffer and
  partial updates went in, and the two halves are verified against each other by
  `joystick_dot`. Measurement set the frame period at 16 ms rather than 33.
- **M3 — Game.** The rules, the Dossier's ghosts, the arcade's per-level difficulty to
  level 21, the 1980 sprite ROMs, the HUD, and the shell around the run — on the board and
  on the host from one set of sources.
- **M4 — System Integration.** The game on the target, three high scores in a
  linker-reserved flash page, and `ott pacman` as the test that plays it.
- **M5 — Random Mazes.** A maze generated per level (FR-029), its appearance derived from
  its walls as geometry. See
  [the Random Mazes design doc](../Docu/Design/M4-Random-Mazes.md).
- **M6 — Pacman AI.** An agent evolved on the host with NEAT and shipped as `const` weights, and
  the menu that now asks which of the two mazes to play (FR-040) — the AI is offered in the
  arcade's own one, which is what it was trained on. See
  [the Pacman AI design doc](../Docu/Design/M6-Pacman-AI.md).
