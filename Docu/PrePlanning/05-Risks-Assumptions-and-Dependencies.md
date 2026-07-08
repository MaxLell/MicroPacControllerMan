# 5 Risks, Assumptions & Dependencies

[← Back to Index](Index.md)

## 5.1 Risks

| ID | Risk | Impact | Likelihood | Mitigation |
|---|---|---|---|---|
| R-001 | Exact mikroBUS-socket-to-STM32G431 GPIO pin mapping is not confirmed from public MikroE/ST documentation alone (the Click Shield schematic shows mikroBUS signal names but not the underlying MCU pin names per socket). | High — wrong assumptions here block all hardware bring-up. | Medium | **M2 update:** a full mapping has been *derived* from the shield/Nucleo/Click schematics and datasheets and recorded in [02 §2.3.3](02-Requirements.md#233-mikrobus--stm32g431-pin-mapping-con-004--r-001) (DEC-008); the M2 firmware targets it. **Still open:** confirm on hardware via continuity check / logic analyzer and the drivers running — close via [VT-INT-003](06-Verification-and-Validation.md)/[VT-INT-004](06-Verification-and-Validation.md). Least-certain items: SCL/SDA on PB8/PB9, active-HIGH CS, DISP-on-MISO. |
| R-002 | Frequent high-score writes could wear on-chip flash NVM over time. | Low | Low | NFR-004 restricts writes to only when the high score actually changes. |
| R-003 | The exact quadrant boundaries of the raw-touch-position-based "Game-Control-Cross" (FR-004) — dead-zone size near center, quadrant edges — are untuned and may not feel precise as a 4-directional "d-pad" for Pacman. | Medium — poor controls hurt playability. | Medium | Tune quadrant boundaries/dead-zone empirically during Board Bring-Up; the Touchpad Click's gesture-detection API remains a fallback if raw-position mapping proves unworkable. |
| R-004 | The tightened timing budgets (NFR-002: 30 FPS, NFR-003: 30 ms input latency) may not be achievable given full-frame SPI push cost to the 128×128 1bpp memory LCD (2 KB/frame) plus FreeRTOS/pub-sub scheduling overhead across `InputTask` → `GameLogicTask` → `RenderTask`. | Medium — sluggish or choppy controls hurt playability. | Medium | Measure achievable frame rate and end-to-end input latency during Board Bring-Up; revisit NFR-002/NFR-003 if unreachable. |
| R-005 | The Click Shield has per-socket 3.3V/5V logic-level switches; an incorrect switch position at first power-on risks damaging a Click board. | Medium | Low | Verify switch positions against Click board voltage requirements before first power-on; add as a checklist item in [08 Troubleshooting Guide](08-Troubleshooting-Guide.md). |
| R-006 | Behaviour of the menu screen immediately after a game over (e.g., whether/how a "new high score" is indicated) is not yet specified beyond FR-008/FR-009. | Low | Low | Revisit during Pacman Development if it turns out to matter for playability. |
| R-007 | The system message bus uses fixed-size, copy-by-value messages (see [03 Architecture §3.2](03-Architecture.md#32-message-broker)), but the game frame that `MSG_RENDER_FRAME` must deliver to the Render module (maze + Pacman + four ghosts + pellet state) is far larger than a small message payload, so passing it by value is impractical. | Medium — shapes the rendering path and affects NFR-002. | High | Decide the frame-transfer mechanism before implementing rendering. **Decided:** a version + pointer/handle to a double-buffered read-only snapshot — the Game module owns two statically-allocated buffers and swaps them each tick; Render draws the last-published one (small queues, no heap, NFR-103). Realized in [03 Architecture §3.6](03-Architecture.md#36-pacman-sub-application-architecture). |
| R-008 | A classic 28×31-tile Pacman maze leaves only ~4 px per tile on the 128×128 monochrome display — likely illegible for Pacman, four ghosts and pellets. | Medium — affects playability and rendering. | Medium | **Decided:** a reduced, display-fit maze at a legible tile size — a custom layout smaller than the classic 28×31 grid (consistent with the "first playable maze" scope). The concrete reference maze (11 × 9 grid) is in [10 §10.2](10-Pacman-Game-Design.md#102-the-maze-playfield). |

## 5.2 Assumptions

| ID | Assumption | Depends on / affects |
|---|---|---|
| A-001 | A 3-second loading screen (NFR-001) is an acceptable default; not explicitly specified by the project owner. | [NFR-001](02-Requirements.md) |
| A-002 | A minimum of 30 FPS (NFR-002) is the target rendering rate for a playable Pacman on a 128×128 monochrome display; whether the hardware/software stack can sustain it is unconfirmed — see R-004. | [NFR-002](02-Requirements.md), R-004 |
| A-003 | A 30 ms input-to-render latency budget (NFR-003) is the target for responsive controls; whether it is achievable given the pub-sub/task pipeline is unconfirmed — see R-004. | [NFR-003](02-Requirements.md), R-004 |
| A-004 | The single high-score entry can be stored using the STM32G431's internal flash (no external EEPROM required); to be confirmed once the exact NVM strategy is chosen during Board Bring-Up. | FR-008, FR-009 |
| A-005 | *(closed)* The core Pacman game rules ([FR-010..FR-021](02-Requirements.md)) are now realised by a concrete design — maze layout, movement/tick model, ghost targeting algorithms, power-pellet/frightened behaviour, scoring and end conditions — specified in [10 Pacman Game Design](10-Pacman-Game-Design.md). Only numeric tuning may still change (A-006). | [10 Pacman Game Design](10-Pacman-Game-Design.md); A-006; R-006 |
| A-006 | The gameplay tuning constants now have concrete starting defaults in [10 Pacman Game Design §10.8](10-Pacman-Game-Design.md#108-tunable-constants-defaults) — game-step period, point values (FR-011/FR-017/FR-019), frightened duration (FR-020), and the scatter/chase schedule (FR-015). The rules are fixed; these numbers remain tunable for game feel during Pacman Development. | FR-011, FR-015, FR-017, FR-019, FR-020 |

## 5.3 Dependencies

| ID | Dependency | Notes |
|---|---|---|
| D-001 | [c-code-style](https://github.com/MaxLell/c-code-style) coding standard repository | External repo; any fixes to the standard itself require a separate PR against that repo, with the project owner's prior approval before touching it. |
| D-002 | Ceedling / Unity toolchain | Must be installed on the host development machine for [Milestone 3 unit tests](04-Implementation-Phases-and-Milestones.md). |
| D-003 | arm-none-eabi-gcc + CMake for the target firmware; a FreeRTOS port for the STM32G4 series (from M4) | Target firmware build — see [03 Architecture §3.8](03-Architecture.md#38-build--toolchain). As built, initialisation is register-level CMSIS with **no CubeMX/HAL** ([DEC-001](11-Decisions-and-As-Built.md)). |
| D-004 | Physical hardware: STM32G431RB Nucleo-64, Click Shield for Nucleo-64, LCD Mono Click, Touchpad Click, USB-C cable | Required from Board Bring-Up onward; lead time if not already on hand. |
| D-005 | SDL2 library | Required for the host build's View (CON-103). |
| D-006 | [BareMetalHollowClockFw](https://github.com/MaxLell/BareMetalHollowClockFw) (reference project) | Source of the OTT (On-Target Test) CLI pattern adapted in [FR-106/FR-107](03-Architecture.md#37-on-target-test-ott-cli-framework); not a build dependency, only a design reference. See also [09 OTT Mechanism & Reset Flow](09-OTT-Mechanism-and-Reset-Flow.md). |
| D-007 | [EmbeddedCli](https://github.com/MaxLell/EmbeddedCli) | Serial CLI framework backing the OTT console (§3.7). Vendored into `Firmware/third_party/embedded_cli/`; two memory-safety fixes were contributed upstream (EmbeddedCli PR #2) and carried in the vendored copy. |
