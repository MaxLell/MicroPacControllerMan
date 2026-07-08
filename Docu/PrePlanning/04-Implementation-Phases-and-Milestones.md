# 4 Implementation Phases / Milestones

[← Back to Index](Index.md)

This restates the phased roadmap from the original idea capture as a structured milestone table with entry/exit criteria. More phases may be inserted later if a milestone turns out to need splitting. Test IDs link to [06 Verification & Validation](06-Verification-and-Validation.md).

| # | Milestone | Entry Criteria | Exit Criteria |
|---|---|---|---|
| 0 | Pre-Planning | Idea captured (`Docu/Idea.md`). | This document set (docs 1–10) exists, is internally consistent, and is reviewed/merged. Becomes the project's source of truth. |
| 1 | Toolchain Bring-Up | Milestone 0 exit met. | Blinky LED runs on the target; the OTT CLI framework (FR-106/FR-107) exists and VT-INT-005 runs as an automatic OTT (`ott blinky`) over the STLINK V3 serial console, drivable by the Python harness ([VT-INT-005](06-Verification-and-Validation.md)). The retained-RAM/reset mechanism ([doc 09](09-OTT-Mechanism-and-Reset-Flow.md)) — originally planned for M2 — was brought forward and built/validated here, so `ott <name>` already schedules via `.noinit`, resets, and reports PASS/FAIL on the next boot. |
| 2 | Board Bring-Up | Milestone 1 exit met. | MCU, display, and touchpad each individually verified with reproducible tests ([VT-INT-001..004](06-Verification-and-Validation.md), VT-INT-006, VT-INT-007). Exact mikroBUS pin mapping ([R-001](05-Risks-Assumptions-and-Dependencies.md#51-risks)) confirmed and documented back into [02 Requirements](02-Requirements.md). The OTT framework (including the retained-RAM/reset mechanism already built in M1, [doc 09](09-OTT-Mechanism-and-Reset-Flow.md)) gains display/touchpad OTT commands; the external Python test harness ([doc 6 §6.3](06-Verification-and-Validation.md#63-test-harness-python)) drives every **Automatic** integration test. |
| 3 | Pacman Development (Host) | [03 Architecture](03-Architecture.md) and [10 Pacman Game Design](10-Pacman-Game-Design.md) approved. Can proceed in parallel with Milestone 2, since it targets the host build only. | Model/Control (including maze, pellet, ghost and frightened-mode rules) covered by unit tests ([VT-UNIT-001..005](06-Verification-and-Validation.md)); host build launches and is playable via SDL ([VT-INT-008](06-Verification-and-Validation.md)); game-logic E2E, single-life game-over, and level-clear scenarios pass ([VT-INT-010](06-Verification-and-Validation.md), VT-INT-014, VT-INT-017). |
| 4 | System Integration (Target) | Milestones 2 and 3 exit met. | All remaining integration tests pass on the physical target ([VT-INT-009](06-Verification-and-Validation.md), VT-INT-011..013, VT-INT-015..016). |

## 4.1 Notes

- Milestones 2 and 3 have no hard dependency on each other and are expected to run concurrently to shorten the overall timeline.
- Each milestone's exit criteria are verification tests defined in [06 Verification & Validation](06-Verification-and-Validation.md) and traced in [07 Traceability Matrix](07-Traceability-Matrix.md) — a milestone is not "done" until its linked tests pass.
- Per the project's git workflow, each milestone's work is expected to land as its own reviewed PR against `main`.
