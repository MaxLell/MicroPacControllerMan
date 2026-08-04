# 5 Risks, Assumptions & Dependencies

[← Back to Index](Index.md)

## 5.1 Risks

| ID | Risk | Impact | Likelihood | Mitigation |
|---|---|---|---|---|
| R-002 | Frequent high-score writes could wear the internal flash over time. | Low | Low | NFR-004 restricts writes to only when the high score actually changes. |
| R-004 | The timing budgets (NFR-002: 60 FPS, NFR-003: 30 ms input latency) may not be achievable, because a full 240 x 320 colour frame is 153,600 bytes and the SPI transfer — not the CPU — dominates a frame. | Medium — sluggish or choppy rendering hurts playability. | **Closed for the frame rate.** Measured on the board: a full frame is 252 ms (3 FPS), but sending only the twelve 8x8 cells a Pacman frame actually changes is 3.43 ms (290 FPS) — one percent of the data, 73 times faster, using 10 % of a 30 FPS budget. The target was then raised to **60 FPS** on the strength of a second measurement — the same motion held at 60 FPS costs 5.26 ms of a 16.7 ms frame, and unpaced the path runs at 175 FPS. The bit rate does not need raising for either. | The render path must send only what changed; the display port provides `display_present_region` for it. Motion was additionally judged on the panel, at a constant speed with the rate varied under it; **60 FPS is the chosen target** and the firmware idles 69 % of every frame while holding it. **Open for the input latency, and not because of the display:** the drawing half of NFR-003 measures 2.08 ms, while the 32 ms debounce window of `Bsp/switch` alone exceeds the 30 ms the requirement allows — see [RF-014](../Refactoring-Backlog.md#rf-014). Arithmetic and measurements in [M2 Board Bring-Up §3](../Design/M2-Board-Bring-Up.md). |
| R-006 | Behaviour of the menu screen immediately after a game over (e.g. whether/how a "new high score" is indicated) is not yet specified beyond FR-008/FR-009. | Low | Low | Revisit during Pacman Development if it turns out to matter for playability. |
| R-007 | The system message bus uses fixed-size, copy-by-value messages (see [03 Architecture §3.2](03-Architecture.md#32-message-broker)), but the game frame that must reach the Render module (maze + Pacman + four ghosts + pellet state) was thought too large to pass by value. | Medium — shapes the rendering path and affects NFR-002. | **Closed, and the premise was wrong.** What is large is the rendered *image*, not the game state: an 11 x 9 maze is two 99-bit pellet maps, five actors and a few counters — **56 bytes**, measured, which copies like any other payload. The image never travels, because Render draws it. | The handle-to-a-snapshot exception is **withdrawn**; no message carries a pointer. This also removes the double buffer it depended on, which would not have fitted anyway — one frame is 60 % of SRAM. Recorded as [DEC-016](11-Decisions-and-As-Built.md). |
| R-011 | Display SCK is PA5, which also drives LD2. The LED is a load on a clock line intended to run at tens of MHz. | Low — LD2 is transistor-buffered, so PA5 sees a base resistor rather than an LED, and ST ships this configuration as the default. | Low. | Leave solder bridge SB10 at its default (ON) and accept LD2 flickering with display traffic. If the display misbehaves at a high SPI clock, opening SB10 is the first thing to try. Note separately that LD2 becomes undriveable regardless, because PA5 turns into an alternate-function pin — that is what retires the `blinky` OTT, not the bridge. Detail in [M2 Board Bring-Up §1.2](../Design/M2-Board-Bring-Up.md). |

## 5.2 Assumptions

| ID | Assumption | Depends on / affects |
|---|---|---|
| A-001 | A 3-second loading screen (NFR-001) is an acceptable default; not explicitly specified by the project owner. | [NFR-001](02-Requirements.md) |
| A-002 | *(closed)* NFR-002 asks for **60 FPS**, no longer 30. The stack sustains it with room to spare — five actors moving over a pellet field cost 5.26 ms of a 16.7 ms frame, and the same frames unpaced run at 175 FPS — and 60 Hz is also what the ST7789V's own refresh does, so a higher figure would buy nothing the eye can reach. Measured in [M2 Board Bring-Up §3.2](../Design/M2-Board-Bring-Up.md). | [NFR-002](02-Requirements.md), R-004 |
| A-003 | A 30 ms input-to-render latency budget (NFR-003) is the target for responsive controls; whether it is achievable given the pub-sub/task pipeline is unconfirmed — see R-004. | [NFR-003](02-Requirements.md), R-004 |
| A-004 | The single high-score entry fits comfortably in the microcontroller's internal flash — one 32-bit value, written only when beaten. No external storage is required. | FR-008, FR-009, CON-006 |
| A-005 | *(closed)* The core Pacman game rules ([FR-010..FR-021](02-Requirements.md)) are realised by a concrete design — maze layout, movement/tick model, ghost targeting algorithms, power-pellet/frightened behaviour, scoring and end conditions — specified in [10 Pacman Game Design](10-Pacman-Game-Design.md). Only numeric tuning may still change (A-006). | [10 Pacman Game Design](10-Pacman-Game-Design.md); A-006; R-006 |
| A-006 | The gameplay tuning constants have concrete starting defaults in [10 Pacman Game Design §10.8](10-Pacman-Game-Design.md#108-tunable-constants-defaults) — game-step period, point values (FR-011/FR-017/FR-019), frightened duration (FR-020), and the scatter/chase schedule (FR-015). The rules are fixed; these numbers remain tunable for game feel during Pacman Development. | FR-011, FR-015, FR-017, FR-019, FR-020 |

## 5.3 Dependencies

| ID | Dependency | Notes |
|---|---|---|
| D-001 | [c-code-style](https://github.com/MaxLell/c-code-style) coding standard repository | External repo; any fixes to the standard itself require a separate PR against that repo, with the project owner's prior approval before touching it. |
| D-002 | Ceedling / Unity toolchain | Must be installed on the host development machine for the unit tests. |
| D-003 | arm-none-eabi-gcc and CMake for the target firmware; STM32CubeProgrammer for flashing | Target firmware build — see [03 Architecture §3.8](03-Architecture.md#38-build--toolchain) and [M2 Board Bring-Up §4](../Design/M2-Board-Bring-Up.md). The FreeRTOS port this row used to name is no longer a dependency ([DEC-027](11-Decisions-and-As-Built.md)). |
| D-004 | Physical hardware: STM32U545RE-Q Nucleo-64, X-NUCLEO-GFX01M2 display shield, USB cable | On hand. |
| D-008 | UM2750 (X-NUCLEO-GFX01M2) and UM3062 (STM32U3/U5 Nucleo-64 boards, MB1841) | Both in hand. Together they give the pin map by connector position; neither alone is sufficient. |
| D-005 | SDL2 library | Required for the host build's View (CON-103). |
| D-006 | [BareMetalHollowClockFw](https://github.com/MaxLell/BareMetalHollowClockFw) (reference project) | Source of the OTT (On-Target Test) CLI pattern adapted in [FR-106/FR-107](03-Architecture.md#37-on-target-test-ott-cli-framework), and of the shared `Services` modules. Not a build dependency, only a design reference. See also [09 OTT Mechanism & Reset Flow](09-OTT-Mechanism-and-Reset-Flow.md). |
| D-007 | [EmbeddedCli](https://github.com/MaxLell/EmbeddedCli) | Serial CLI framework backing the OTT console (§3.7). Vendored into `Firmware/ThirdParty/EmbeddedCli/`; two memory-safety fixes were contributed upstream (EmbeddedCli PR #2) and are carried in the vendored copy. |
