---
title: Risks, Assumptions & Dependencies
---

# 5 Risks, Assumptions & Dependencies

[[Index|← Back to Index]]

## 5.1 Risks

| ID | Risk | Impact | Likelihood | Mitigation |
|---|---|---|---|---|
| R-001 | Exact mikroBUS-socket-to-STM32G431 GPIO pin mapping is not confirmed from public MikroE/ST documentation alone (the Click Shield schematic shows mikroBUS signal names but not the underlying MCU pin names per socket). | High — wrong assumptions here block all hardware bring-up. | Medium | Verify with continuity check / logic analyzer during Board Bring-Up; close via [[06-Verification-and-Validation\|VT-INT-003]] / [[06-Verification-and-Validation\|VT-INT-004]]; record confirmed pins back into [[02-System-Requirements-and-Architecture]]. |
| R-002 | Frequent high-score writes could wear on-chip flash NVM over time. | Low | Low | NFR-004 restricts writes to only when the high score actually changes. |
| R-003 | The exact quadrant boundaries of the raw-touch-position-based "Game-Control-Cross" (FR-004) — dead-zone size near center, quadrant edges — are untuned and may not feel precise as a 4-directional "d-pad" for Pacman. | Medium — poor controls hurt playability. | Medium | Tune quadrant boundaries/dead-zone empirically during Board Bring-Up; the Touchpad Click's gesture-detection API remains a fallback if raw-position mapping proves unworkable. |
| R-004 | The tightened timing budgets (NFR-002: 30 FPS, NFR-003: 30 ms input latency) may not be achievable given full-frame SPI push cost to the 128×128 1bpp memory LCD (2 KB/frame) plus FreeRTOS/pub-sub scheduling overhead across `InputTask` → `GameLogicTask` → `RenderTask`. | Medium — sluggish or choppy controls hurt playability. | Medium | Measure achievable frame rate and end-to-end input latency during Board Bring-Up; revisit NFR-002/NFR-003 if unreachable. |
| R-005 | The Click Shield has per-socket 3.3V/5V logic-level switches; an incorrect switch position at first power-on risks damaging a Click board. | Medium | Low | Verify switch positions against Click board voltage requirements before first power-on; add as a checklist item in [[08-Troubleshooting-Guide]]. |
| R-006 | Behaviour of the menu screen immediately after a game over (e.g., whether/how a "new high score" is indicated) is not yet specified beyond FR-008/FR-009. | Low | Low | Revisit during Pacman Development if it turns out to matter for playability. |

## 5.2 Assumptions

| ID | Assumption | Depends on / affects |
|---|---|---|
| A-001 | A 3-second loading screen (NFR-001) is an acceptable default; not explicitly specified by the project owner. | [[02-System-Requirements-and-Architecture\|NFR-001]] |
| A-002 | A minimum of 30 FPS (NFR-002) is the target rendering rate for a playable Pacman on a 128×128 monochrome display; whether the hardware/software stack can sustain it is unconfirmed — see R-004. | [[02-System-Requirements-and-Architecture\|NFR-002]], R-004 |
| A-003 | A 30 ms input-to-render latency budget (NFR-003) is the target for responsive controls; whether it is achievable given the pub-sub/task pipeline is unconfirmed — see R-004. | [[02-System-Requirements-and-Architecture\|NFR-003]], R-004 |
| A-004 | The single high-score entry can be stored using the STM32G431's internal flash (no external EEPROM required); to be confirmed once the exact NVM strategy is chosen during Board Bring-Up. | FR-008, FR-009 |

## 5.3 Dependencies

| ID | Dependency | Notes |
|---|---|---|
| D-001 | [c-code-style](https://github.com/MaxLell/c-code-style) coding standard repository | External repo; any fixes to the standard itself require a separate PR against that repo, with the project owner's prior approval before touching it. |
| D-002 | Ceedling / Unity toolchain | Must be installed on the host development machine for [[03-Software-Requirements-and-Architecture\|Milestone 3 unit tests]]. |
| D-003 | STM32 toolchain (e.g. STM32CubeIDE/CubeMX or equivalent) + a FreeRTOS port for the STM32G4 series | Required for target firmware builds. |
| D-004 | Physical hardware: STM32G431RB Nucleo-64, Click Shield for Nucleo-64, LCD Mono Click, Touchpad Click, USB-C cable | Required from Board Bring-Up onward; lead time if not already on hand. |
| D-005 | SDL2 library | Required for the host build's View (CON-103). |
| D-006 | [BareMetalHollowClockFw](https://github.com/MaxLell/BareMetalHollowClockFw) (reference project) | Source of the OTT (On-Target Test) CLI pattern adapted in [[03-Software-Requirements-and-Architecture|FR-106/FR-107]]; not a build dependency, only a design reference. |

## 5.4 Related Documents

- [[06-Verification-and-Validation]] — tests that close out R-001 through R-005.
- [[08-Troubleshooting-Guide]] — playbook entries seeded from these risks.
