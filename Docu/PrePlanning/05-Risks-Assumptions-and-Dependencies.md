---
title: Risks, Assumptions & Dependencies
---

# 5 Risks, Assumptions & Dependencies

[[Index|← Back to Index]]

## 5.1 Risks

| ID | Risk | Impact | Likelihood | Mitigation |
|---|---|---|---|---|
| R-001 | Exact mikroBUS-socket-to-STM32G431 GPIO pin mapping is not confirmed from public MikroE/ST documentation alone (the Click Shield schematic shows mikroBUS signal names but not the underlying MCU pin names per socket). | High — wrong assumptions here block all hardware bring-up. | Medium | Verify with continuity check / logic analyzer during Board Bring-Up; close via [[06-Verification-and-Validation\|VT-HW-002]] / [[06-Verification-and-Validation\|VT-HW-003]]; record confirmed pins back into [[02-System-Requirements-and-Architecture]]. |
| R-002 | Frequent high-score writes could wear on-chip flash NVM over time. | Low | Low | NFR-004 restricts writes to only when the high score actually changes. |
| R-003 | The Touchpad Click's gesture-detection API (single/double click, swipe) may not map cleanly onto a discrete 4-directional "d-pad" feel expected for Pacman. | Medium — poor controls hurt playability. | Medium | Prototype raw touch-position-based direction detection early in Board Bring-Up as a fallback to gesture-based detection. |
| R-004 | Full-frame SPI push to the 128×128 1bpp memory LCD (2 KB/frame) may not sustain the target refresh rate (NFR-002) once FreeRTOS task scheduling overhead is included. | Medium | Low | Measure achievable frame rate during Board Bring-Up; revisit NFR-002 if unreachable. |
| R-005 | The Click Shield has per-socket 3.3V/5V logic-level switches; an incorrect switch position at first power-on risks damaging a Click board. | Medium | Low | Verify switch positions against Click board voltage requirements before first power-on; add as a checklist item in [[08-Troubleshooting-Guide]]. |
| R-006 | Behaviour of the menu screen immediately after a game over (e.g., whether/how a "new high score" is indicated) is not yet specified beyond FR-008/FR-009. | Low | Low | Revisit during Pacman Development if it turns out to matter for playability. |

## 5.2 Assumptions

| ID | Assumption | Depends on / affects |
|---|---|---|
| A-001 | A 3-second loading screen (NFR-001) is an acceptable default; not explicitly specified by the project owner. | [[02-System-Requirements-and-Architecture\|NFR-001]] |
| A-002 | A minimum of 10 FPS (NFR-002) is sufficient for a playable Pacman on a 128×128 monochrome display; not explicitly specified. | [[02-System-Requirements-and-Architecture\|NFR-002]], R-004 |
| A-003 | A 100 ms input-to-render latency budget (NFR-003) is a reasonable default for responsive controls; not explicitly specified. | [[02-System-Requirements-and-Architecture\|NFR-003]] |
| A-004 | The single high-score entry can be stored using the STM32G431's internal flash (no external EEPROM required); to be confirmed once the exact NVM strategy is chosen during Board Bring-Up. | FR-008, FR-009 |

## 5.3 Dependencies

| ID | Dependency | Notes |
|---|---|---|
| D-001 | [c-code-style](https://github.com/MaxLell/c-code-style) coding standard repository | External repo; any fixes to the standard itself require a separate PR against that repo, with the project owner's prior approval before touching it. |
| D-002 | Ceedling / Unity toolchain | Must be installed on the host development machine for [[03-Software-Requirements-and-Architecture\|Milestone 3 unit tests]]. |
| D-003 | STM32 toolchain (e.g. STM32CubeIDE/CubeMX or equivalent) + a FreeRTOS port for the STM32G4 series | Required for target firmware builds. |
| D-004 | Physical hardware: STM32G431RB Nucleo-64, Click Shield for Nucleo-64, LCD Mono Click, Touchpad Click, USB-C cable | Required from Board Bring-Up onward; lead time if not already on hand. |
| D-005 | SDL2 library | Required for the host build's View (CON-103). |

## 5.4 Related Documents

- [[06-Verification-and-Validation]] — tests that close out R-001 through R-005.
- [[08-Troubleshooting-Guide]] — playbook entries seeded from these risks.
