# 2 Requirements

[← Back to Index](Index.md) · See also [01 System Overview & Context](01-System-Overview-and-Context.md)

Requirements use the [EARS](https://alistairmavin.com/ears/) notation (Ubiquitous / Event-driven / State-driven / Unwanted-behaviour / Optional-feature templates) and are grouped by feature/concern. This project does **not** separate "system" from "software" requirements — they all live here together. The `FR-0xx` / `FR-1xx` (and `NFR` / `CON`) numbers are stable identifiers only; the numbering carries no system-vs-software meaning.

> **Hardware baseline changed (2026-07).** After the M3 review the project restarted at M1 on
> the **STM32U545RE-Q** with the **X-NUCLEO-GFX01M2** display shield, and Pacman became a
> **colour** game. The two mikroBUS Clicks (Sharp 128×128 mono LCD, MTCH6102 touchpad) are
> gone. Requirements touched by that change are marked below; the reasoning is in
> [11 Decisions & As-Built](11-Decisions-and-As-Built.md).

## 2.1 Functional Requirements

### 2.1.1 Boot & Menu

| Unique-ID | Name | Description |
|---|---|---|
| FR-001 | Loading Screen | Upon power-on, the system shall display a loading screen containing the Pacman logo. *(timing — see NFR-005)* |
| FR-002 | High Score Menu | After the loading screen completes, the system shall display a menu screen showing the current high score. |
| FR-003 | Game Start | While the menu screen is displayed, when the user presses the Nucleo board user button, the system shall start a new Pacman game. |

### 2.1.2 Player Control & Rendering

| Unique-ID | Name | Description |
|---|---|---|
| FR-004 | Directional Control | While a game is in progress, when the user presses one of the four directional keys of the joystick on the display shield, the system shall move Pacman in the corresponding direction. |
| FR-005 | Game Rendering | While a game is in progress, the system shall render the current game state on the colour display. |
| FR-028 | Colour Rendering | While a game is in progress, the system shall render walls, pellets, Pacman and the ghosts in distinguishable colours, with each of the four ghosts visually distinct from the other three. |

FR-004 reads four discrete, active-low GPIO keys — there is no touch surface, no quadrant
geometry and no dead zone to tune. That removes the whole class of problem the old
touchpad mapping carried (closing [R-003](05-Risks-Assumptions-and-Dependencies.md#51-risks)):
a directional key is either pressed or it is not. The joystick's fifth, centre key is
reserved and unused for now; FR-003 deliberately keeps the **Nucleo** user button as the
game-start input, because that button is also what confirms the manual on-target tests.

FR-028 is what the hardware change bought. On the previous 1-bpp panel every entity was the
same colour and the four ghosts could only be told apart by position; the requirement was
not expressible.

### 2.1.3 Maze, Pellets & Movement

| Unique-ID | Name | Description |
|---|---|---|
| FR-010 | Maze Confinement | While a game is in progress, the system shall confine Pacman and the ghosts to the open paths of a single fixed maze, preventing any movement through walls. |
| FR-011 | Pellet Consumption | While a game is in progress, when Pacman enters a maze cell containing a pellet, the system shall remove that pellet and add its point value to the current score. |
| FR-012 | Tunnel Wrap-Around | While a game is in progress, when Pacman or a ghost exits the maze through a tunnel opening at a maze edge, the system shall re-enter it through the opposite tunnel opening on the same row (horizontal tunnel) or same column (vertical tunnel). |
| FR-022 | Playfield Size | Each level's maze shall use a **28 × 31 cell** grid rendered at **8 × 8 pixels per cell**, in portrait orientation on the 240 × 320 display, leaving the remaining 240 × 72 pixel band for the HUD. |

FR-022 is the classic arcade grid, at last affordable: 28 × 8 = 224 px wide and 31 × 8 = 248 px
tall fits the display in portrait with 72 px to spare. The previous 128 × 128 panel forced a
*reduced, display-fit* maze, which is what
[R-008](05-Risks-Assumptions-and-Dependencies.md#51-risks) was about — that risk is
**superseded**: the constraint that caused it is gone. Landscape does not work (248 > 240),
so the orientation is not a free choice.

### 2.1.4 Ghosts

| Unique-ID | Name | Description |
|---|---|---|
| FR-013 | Ghost Presence | While a game is in progress, the system shall move four ghosts through the maze concurrently with Pacman. |
| FR-014 | Distinct Ghost Behaviors | While a game is in progress and frightened mode (FR-018) is not active, the system shall drive each of the four ghosts by its own distinct targeting behavior, in the style of the classic Blinky/Pinky/Inky/Clyde ghosts. |
| FR-015 | Scatter / Chase Alternation | While a game is in progress, the system shall periodically alternate the ghosts between a chase phase (pursuing their individual targets) and a scatter phase (retreating toward fixed maze corners). |
| FR-016 | Ghost Collision | While a game is in progress and frightened mode (FR-018) is not active, when a ghost occupies the same maze cell as Pacman, the system shall treat Pacman as caught and reduce the remaining lives (see FR-006 / FR-007). |

### 2.1.5 Power Pellets & Frightened Mode

| Unique-ID | Name | Description |
|---|---|---|
| FR-017 | Power Pellet Consumption | While a game is in progress, when Pacman enters a maze cell containing a power pellet, the system shall remove it, add its point value to the score, and activate frightened mode. |
| FR-018 | Frightened Ghosts | While frightened mode is active, the system shall render all ghosts in a visibly frightened state and make them flee from Pacman. |
| FR-019 | Eating Frightened Ghosts | While frightened mode is active, when Pacman occupies the same maze cell as a frightened ghost, the system shall add a ghost-eaten bonus to the score and return that ghost to the ghost pen to regenerate, without reducing Pacman's lives. |
| FR-020 | Frightened Timeout | While frightened mode is active, when its bounded duration elapses, the system shall end frightened mode and return all ghosts to their normal behavior. |

### 2.1.6 Lives, Levels & End of Game

| Unique-ID | Name | Description |
|---|---|---|
| FR-006 | Starting Lives | The system shall start each game run with a fixed number of Pacman lives (default 3 — *tunable*). |
| FR-024 | Life Lost & Respawn | When Pacman is caught and at least one life remains, the system shall decrement the lives, reset Pacman and the ghosts to their level start positions, and continue the current level. |
| FR-007 | Game Over | When Pacman's remaining lives reach zero, the system shall end the game and return to the menu screen. |
| FR-025 | Level Count & Mazes | The game shall consist of 5 levels, each with its own distinct maze. |
| FR-021 | Level Clear | While a game is in progress, when Pacman has consumed the last pellet and power pellet of the current level's maze and it is not the final level, the system shall advance to the next level — loading the next maze and applying that level's higher difficulty — while keeping the accumulated score and remaining lives. |
| FR-026 | Difficulty Scaling | Each successive level shall be harder than the previous one — faster ghosts, shorter frightened duration, and less scatter time — per the progression in [10 Pacman Game Design](10-Pacman-Game-Design.md). |
| FR-027 | Game Completion | When Pacman clears the final (5th) level, the system shall end the game as fully completed (won). |
| FR-023 | End-of-Game Score Screen | When a game run ends (all lives lost, or the final level cleared), the system shall display the final score on its own screen for 2 seconds before returning to the menu screen. *(2 s default — tunable)* |

### 2.1.7 High Score & Persistence

| Unique-ID | Name | Description |
|---|---|---|
| FR-008 | High Score Update | If the final score of a completed game exceeds the stored high score, then the system shall store the new value as the high score in NVM. |
| FR-009 | High Score Persistence | The system shall retain the high score value in NVM across power cycles. |

One 32-bit value, written only when it is beaten. The NVM is the microcontroller's own
flash (CON-006) — the 64-Mbit SPI flash that happens to sit on the display shield stays
unused, because a single word does not justify a second storage device and its driver.

The concrete rules and values that realise these requirements — the maze layout, movement/tick model, ghost targeting algorithms, power-pellet/frightened behaviour, scoring and end conditions — are specified in [10 Pacman Game Design](10-Pacman-Game-Design.md). The numeric constants there remain tunable for game feel (see [A-006](05-Risks-Assumptions-and-Dependencies.md#52-assumptions)).

### 2.1.8 Software Structure & Messaging

See [03 Architecture](03-Architecture.md) for how these are realized.

| Unique-ID | Name | Description |
|---|---|---|
| FR-101 | MVP Separation | The software shall implement the game using Model-View-Control (MVP) architecture, separating game state (Model), rendering (View), and game logic (Control) into distinct components. |
| FR-102 | Stateless Control | The Control component shall not persist game state between invocations; all game state shall reside exclusively in the Model. |
| FR-103 | Message-Based Communication | Firmware modules shall exchange data exclusively through published/subscribed messages on the message bus, not through direct cross-module function calls. |
| FR-104 | Host Buildability | The Model and Control components shall be buildable and executable unmodified on the host computer, using SDL to implement the View. |
| FR-105 | FreeRTOS Task Separation | The firmware shall execute input handling, rendering, game-logic ticking, and persistence as separate FreeRTOS tasks. |
| FR-108 | Dedicated Message-Broker Task | The firmware shall route inter-module messages through a single dedicated message-broker task whose sole responsibility is to move each published message from the broker's input queue into the output queue of every module subscribed to that message's topic. |
| FR-109 | Active-Object Modules | *(**under revision** — the M3 review dropped the Active-Object pattern. The replacement, a Data-Pool that modules query by request/response message, is not settled yet; see [11 Decisions & As-Built](11-Decisions-and-As-Built.md). FR-103 and FR-110 are unaffected.)* Each firmware module shall be structured as an active object: exactly one owning FreeRTOS task, a single inbound message queue as its only external input, run-to-completion handling of each received message, and no application state shared directly with other modules. |
| FR-110 | Pacman Internal Message Bus | The Pacman application shall exchange messages between its Model, View, and Control modules over its own internal message-bus instance, separate from the system-level message bus. |

### 2.1.9 On-Target Test (OTT) Framework

| Unique-ID | Name | Description |
|---|---|---|
| FR-106 | On-Target Test (OTT) Framework | The firmware shall expose each on-target test named in [06 Verification & Validation](06-Verification-and-Validation.md) as an individual command over the serial console CLI, following an OTT pattern: a setup step that parses/validates the command's arguments, and a run step that performs the action and asserts the expected outcome internally. See the [OTT CLI Framework](03-Architecture.md#37-on-target-test-ott-cli-framework) for the mechanism. |
| FR-107 | OTT Result Reporting | After executing an on-target test, the firmware shall report PASS or FAIL (with reason on failure) to the serial console, without requiring a debugger to be attached. |

### 2.1.10 Error Handling

| Unique-ID | Name | Description |
|---|---|---|
| FR-111 | Fatal-Error Halt | When the firmware detects a fatal, unrecoverable error, its error handler shall halt execution at a debugger breakpoint (`bkpt`) so the fault can be inspected. *(minimal handling for now; may be extended in later phases)* |

## 2.2 Non-Functional Requirements

### 2.2.1 Timing & Performance

| Unique-ID | Name | Description |
|---|---|---|
| NFR-001 | Loading Screen Duration | The loading screen shall be displayed for no more than 3 seconds before the menu is shown. *(default value — see [A-001](05-Risks-Assumptions-and-Dependencies.md#52-assumptions))* |
| NFR-002 | Rendering Rate | While a game is in progress, the system shall refresh the display at a minimum of 30 frames per second. *(default value — see [A-002](05-Risks-Assumptions-and-Dependencies.md#52-assumptions))* |
| NFR-003 | Input Latency | When a joystick directional key is pressed, the system shall reflect the corresponding movement on the display within 30 ms. *(default value — see [A-003](05-Risks-Assumptions-and-Dependencies.md#52-assumptions))* |
| NFR-005 | Logo Display Delay | Upon power-on, the system shall wait 200 ms before displaying the Pacman logo of the loading screen (FR-001). |

**NFR-002 cannot be met by pushing whole frames, and the arithmetic is not close.** A
240 × 320 frame at 16 bits per pixel is **153,600 bytes** — 1.23 Mbit over SPI for every
frame. At 40 Mbit/s that is **≈31 ms**, i.e. ~32 FPS with the entire budget spent inside the
display transfer and nothing left for the game; at 20 Mbit/s it is ≈61 ms, or 16 FPS. The
bigger, colour display therefore made this budget *harder* than the old 2 kB monochrome
frame did, even though the CPU is faster.

What makes 30 FPS comfortable instead of impossible is that **almost nothing changes between
two Pacman frames**: the maze is static, and only Pacman, four ghosts and the odd eaten
pellet move. Redrawing six 8 × 8 cells is 768 bytes — under 0.2 ms. The requirement stays at
30 FPS; *how* the rendering path exploits this belongs to
[03 Architecture](03-Architecture.md), and the measurement obligation is carried by
[R-004](05-Risks-Assumptions-and-Dependencies.md#51-risks).

### 2.2.2 Persistence

| Unique-ID | Name | Description |
|---|---|---|
| NFR-004 | NVM Write Frequency | The system shall write to NVM only when the high score changes, in order to minimize flash wear. |

### 2.2.3 Quality & Testability

| Unique-ID | Name | Description |
|---|---|---|
| NFR-101 | Unit Testability | The Model and Control components shall be unit-testable with Ceedling/Unity without requiring target hardware or FreeRTOS. |
| NFR-102 | Coding Standard Compliance | All C source code shall conform to the [c-code-style](https://github.com/MaxLell/c-code-style) coding standard. |
| NFR-103 | No Runtime Heap Allocation | The message bus implementation shall not perform dynamic heap allocation at runtime on target. |
| NFR-105 | Message-Bus Backpressure | The message broker shall provide an API for a publisher to query the free capacity of the broker input queue, and a publish attempt on a full queue shall return a status (bounded, non-blocking) rather than blocking the publisher indefinitely. |

### 2.2.4 Test Harness

| Unique-ID | Name | Description |
|---|---|---|
| NFR-104 | OTT Harness Compatibility | Every on-target test classified as automatable in [06 Verification & Validation](06-Verification-and-Validation.md) shall be triggerable via the OTT CLI (FR-106) and have its PASS/FAIL result parseable by an external test-runner script, without manual intervention beyond flashing. |

## 2.3 Constraints

### 2.3.1 Hardware

| Unique-ID | Name | Description |
|---|---|---|
| CON-001 | Target Hardware | The system shall run on the **STM32U545RE-Q Nucleo-64** board. |
| CON-002 | Display Hardware | The system shall use the display of the **X-NUCLEO-GFX01M2** expansion board (2.2" TFT, **240 × 320**, colour, ILI9341 controller over SPI) as its display. |
| CON-003 | Input Hardware | The system shall use the **joystick of the X-NUCLEO-GFX01M2** — four directional keys plus a centre key, as discrete active-low GPIO inputs — as its directional input device. |
| CON-004 | Carrier Hardware | The display shield shall connect to the Nucleo board through the **ST-Morpho headers (CN7/CN10)**; no separate carrier board is used. *(pin mapping — see §2.3.3 and [R-009](05-Risks-Assumptions-and-Dependencies.md#51-risks))* |
| CON-005 | Debug Interface | The system shall use the on-board **ST-LINK V3E** for debugging (SWD) and serial console output. |
| CON-006 | Non-Volatile Storage | The high score shall be stored in the microcontroller's **internal flash**. The external SPI flash on the display shield shall not be used. |

The MCU choice follows from CON-002 and FR-028 rather than the other way round: a 240 × 320
RGB565 frame buffer is 153.6 kB, and the STM32G431RB this project started on has **32 kB** of
SRAM in total. The STM32U545RE has **274 kB** (256 kB contiguous plus 16 kB SRAM4), so the
frame buffer fits with room to spare. Colour Pacman on the old part was not a tuning
problem, it was arithmetically impossible.

### 2.3.3 X-NUCLEO-GFX01M2 ↔ STM32U545RE pin mapping (CON-004 / R-009)

**Status: NOT confirmed — the STM32 pin column is still to be derived.**
The shield's signals are known by their **ST-Morpho positions**, taken from the shield's
published device-tree description. What is *not* yet established is the translation from
those header positions to STM32U545RE port pins, and nothing here may be treated as
as-built until it has been checked on the board with a logic analyzer — the same discipline
that closed R-001 for the previous shield, and for the same reason: on that shield an
assumed pin map was the root cause of a blank display.

| Function | ST-Morpho | STM32U545RE | Notes |
|---|---|---|---|
| Display SCK | *(SPI, position TBD)* | TBD | 4-wire SPI; the shield is specified up to 32 MHz |
| Display MOSI | *(SPI, position TBD)* | TBD | |
| Display CS | CN10-21 | TBD | active LOW |
| Display DC | CN10-25 | TBD | data/command select |
| Display RESET | CN7-30 | TBD | active LOW |
| Display TE | CN7-28 | TBD | tearing effect; optional |
| Joystick Up | CN7-38 | TBD | active LOW, pull-up |
| Joystick Down | CN10-27 | TBD | active LOW, pull-up |
| Joystick Left | CN10-17 | TBD | active LOW, pull-up |
| Joystick Right | CN7-34 | TBD | active LOW, pull-up |
| Joystick Select | CN10-19 | TBD | active LOW, pull-up; reserved, unused |
| SPI flash CS | CN10-23 | TBD | second SPI; **unused** per CON-006 |

**Known conflict to resolve first.** The console occupies **PA9/PA10** (§2.3.4). A
provisional reading of the Morpho map puts **display CS (CN10-21) on PA9** — the console's
TX. If that holds, the display and the serial console cannot coexist as configured and one
of the two must move. This must be settled before any display driver is written, because
it is exactly the class of mistake that cost the previous bring-up its schedule.

**As-built and confirmed on this board (M1):**

| Function | Pin | Note |
|---|---|---|
| Console | PA9 / PA10 | USART1 → ST-LINK V3E VCP, 115200 8N1 |
| User button B1 | PC13 | **active HIGH** — idle low, confirmed by reading `GPIOC->IDR` over SWD with the button released |
| LED LD2 | PA5 | `.ioc` label `LED_GREEN`, active HIGH. Note PA5 is also the Arduino-header SPI1 SCK |
| SWD | PA13 / PA14 | SWDIO / SWCLK |

### 2.3.4 Clock configuration (as configured)

Owned by the STM32CubeMX `.ioc` ([DEC-012](11-Decisions-and-As-Built.md)) and applied by its
generated `SystemClock_Config()` before the firmware entry point is reached.

| Item | Value | Note |
|---|---|---|
| SYSCLK / HCLK | **160 MHz** | The STM32U545's maximum |
| Source | PLL1 | from **MSIS at 48 MHz**: `PLLM 3 → 16 MHz × PLLN 10 → 160 MHz, PLLR 1` |
| Time base | SysTick @ **1 kHz** | Owned by the HAL; `Bsp/systick_bsp` reads it and hangs the 1 ms input-debounce hook off it |
| USART1 kernel clock | 160 MHz | Console rate is pinned to 115200 in firmware, not in the `.ioc` |
| Security | TrustZone **off** | Single non-secure domain (`CORTEX_M33_NS`) |
| Instruction cache | ICACHE enabled | Generated by CubeMX; wanted on this part |
| Display SPI bit rate | TBD | Set once the display pin map (§2.3.3) is confirmed. See the NFR-002 note in §2.2.1 — this rate, not the CPU, decides the frame budget |

CubeMX resolved the PLL source to MSIS rather than the HSI16 that was asked for. It is
functionally equivalent here and the console runs correctly at 115200, so it stands as
configured rather than being fought.

**Consequence worth knowing:** never express a delay as a *spin count*. Use
`Services/delay` or `Services/sw_timer`, which are clock-independent.

### 2.3.2 Software & Toolchain

| Unique-ID | Name | Description |
|---|---|---|
| CON-101 | Language | The firmware and game logic shall be written in C. |
| CON-102 | Test Framework | Unit tests shall run under Ceedling/Unity. |
| CON-103 | Host View Library | The host build's View shall use SDL. |
| CON-104 | RTOS | The target firmware shall run on FreeRTOS. |
| CON-105 | Flashing Tool | The firmware shall be flashed with **STM32CubeProgrammer**. OpenOCD 0.12.0 — the newest version the development host packages — attaches to this part but cannot program it: its flash driver knows only `STM32U57/U58xx` (device ID 0x482) while this board reports **0x455** (STM32U535/U545). OpenOCD remains in use as the gdb server and for the ST-LINK udev rules. |
