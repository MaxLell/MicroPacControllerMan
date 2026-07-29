# 5 Risks, Assumptions & Dependencies

[← Back to Index](Index.md)

## 5.1 Risks

| ID | Risk | Impact | Likelihood | Mitigation |
|---|---|---|---|---|
| R-002 | Frequent high-score writes could wear the internal flash over time. | Low | Low | NFR-004 restricts writes to only when the high score actually changes. |
| R-004 | The timing budgets (NFR-002: 30 FPS, NFR-003: 30 ms input latency) may not be achievable, because a full 240 × 320 colour frame is 153,600 bytes and the SPI transfer — not the CPU — dominates a frame. | Medium — sluggish or choppy rendering hurts playability. | Medium | Transmit only the screen regions that changed: between two Pacman frames the maze is static and only Pacman, four ghosts and the odd eaten pellet move. Budget arithmetic and the design consequences are in [M2 Board Bring-Up §3](../Design/M2-Board-Bring-Up.md). Measure the achieved rate once the display runs, and only then revisit NFR-002/NFR-003. |
| R-006 | Behaviour of the menu screen immediately after a game over (e.g. whether/how a "new high score" is indicated) is not yet specified beyond FR-008/FR-009. | Low | Low | Revisit during Pacman Development if it turns out to matter for playability. |
| R-007 | The system message bus uses fixed-size, copy-by-value messages (see [03 Architecture §3.2](03-Architecture.md#32-message-broker)), but the game frame that must reach the Render module (maze + Pacman + four ghosts + pellet state) is far larger than a small message payload, so passing it by value is impractical. | Medium — shapes the rendering path and affects NFR-002. | High | *Decided:* a version plus a handle to a read-only snapshot the Game module owns; Render draws the last published one (small queues, no heap, NFR-103). Realized in [03 Architecture §3.6](03-Architecture.md#36-pacman-sub-application-architecture). |
| R-009 | The translation from the X-NUCLEO-GFX01M2's ST-Morpho header positions to STM32U545RE port pins is not established from public documentation alone, and the shield's SPI pins are not yet identified. | High — a wrong pin map blocks all display and input bring-up, and the symptom is hard to distinguish from a software fault. | Medium | Derive the map from UM2750 and the board manual, then confirm every signal on the board with a logic analyzer before writing any display driver. One conflict is already suspected: display CS may land on PA9, which carries the console's transmit line. Tracked in [M2 Board Bring-Up §1](../Design/M2-Board-Bring-Up.md). |

## 5.2 Assumptions

| ID | Assumption | Depends on / affects |
|---|---|---|
| A-001 | A 3-second loading screen (NFR-001) is an acceptable default; not explicitly specified by the project owner. | [NFR-001](02-Requirements.md) |
| A-002 | A minimum of 30 FPS (NFR-002) is the target rendering rate for a playable Pacman; whether the hardware/software stack can sustain it is unconfirmed — see R-004. | [NFR-002](02-Requirements.md), R-004 |
| A-003 | A 30 ms input-to-render latency budget (NFR-003) is the target for responsive controls; whether it is achievable given the pub-sub/task pipeline is unconfirmed — see R-004. | [NFR-003](02-Requirements.md), R-004 |
| A-004 | The single high-score entry fits comfortably in the microcontroller's internal flash — one 32-bit value, written only when beaten. No external storage is required. | FR-008, FR-009, CON-006 |
| A-005 | *(closed)* The core Pacman game rules ([FR-010..FR-021](02-Requirements.md)) are realised by a concrete design — maze layout, movement/tick model, ghost targeting algorithms, power-pellet/frightened behaviour, scoring and end conditions — specified in [10 Pacman Game Design](10-Pacman-Game-Design.md). Only numeric tuning may still change (A-006). | [10 Pacman Game Design](10-Pacman-Game-Design.md); A-006; R-006 |
| A-006 | The gameplay tuning constants have concrete starting defaults in [10 Pacman Game Design §10.8](10-Pacman-Game-Design.md#108-tunable-constants-defaults) — game-step period, point values (FR-011/FR-017/FR-019), frightened duration (FR-020), and the scatter/chase schedule (FR-015). The rules are fixed; these numbers remain tunable for game feel during Pacman Development. | FR-011, FR-015, FR-017, FR-019, FR-020 |

## 5.3 Dependencies

| ID | Dependency | Notes |
|---|---|---|
| D-001 | [c-code-style](https://github.com/MaxLell/c-code-style) coding standard repository | External repo; any fixes to the standard itself require a separate PR against that repo, with the project owner's prior approval before touching it. |
| D-002 | Ceedling / Unity toolchain | Must be installed on the host development machine for the unit tests. |
| D-003 | arm-none-eabi-gcc and CMake for the target firmware; STM32CubeProgrammer for flashing; a FreeRTOS port for the STM32U5 series (from M4) | Target firmware build — see [03 Architecture §3.8](03-Architecture.md#38-build--toolchain) and [M2 Board Bring-Up §4](../Design/M2-Board-Bring-Up.md). |
| D-004 | Physical hardware: STM32U545RE-Q Nucleo-64, X-NUCLEO-GFX01M2 display shield, USB cable | On hand. |
| D-005 | SDL2 library | Required for the host build's View (CON-103). |
| D-006 | [BareMetalHollowClockFw](https://github.com/MaxLell/BareMetalHollowClockFw) (reference project) | Source of the OTT (On-Target Test) CLI pattern adapted in [FR-106/FR-107](03-Architecture.md#37-on-target-test-ott-cli-framework), and of the shared `Services` modules. Not a build dependency, only a design reference. See also [09 OTT Mechanism & Reset Flow](09-OTT-Mechanism-and-Reset-Flow.md). |
| D-007 | [EmbeddedCli](https://github.com/MaxLell/EmbeddedCli) | Serial CLI framework backing the OTT console (§3.7). Vendored into `Firmware/ThirdParty/EmbeddedCli/`; two memory-safety fixes were contributed upstream (EmbeddedCli PR #2) and are carried in the vendored copy. |
