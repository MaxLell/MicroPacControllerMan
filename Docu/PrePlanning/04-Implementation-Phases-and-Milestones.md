---
title: Implementation Phases & Milestones
---

# 4 Implementation Phases / Milestones

[[Index|← Back to Index]]

This restates the phased roadmap from the original idea capture as a structured milestone table with entry/exit criteria. More phases may be inserted later if a milestone turns out to need splitting.

| # | Milestone | Entry Criteria | Exit Criteria |
|---|---|---|---|
| 0 | Pre-Planning | Idea captured (`Docu/Idea.md`). | This document set (docs 1–8) exists, is internally consistent, and is reviewed/merged. Becomes the project's source of truth. |
| 1 | Toolchain Bring-Up | Milestone 0 exit met. | Blinky LED runs on the target and its behaviour is confirmed over the STLINK V3 serial console ([[06-Verification-and-Validation\|VT-INT-005]]). |
| 2 | Board Bring-Up | Milestone 1 exit met. | MCU, display, and touchpad each individually verified with reproducible tests ([[06-Verification-and-Validation\|VT-INT-001..004]], [[06-Verification-and-Validation\|VT-INT-006]], [[06-Verification-and-Validation\|VT-INT-007]]). Exact mikroBUS pin mapping ([[05-Risks-Assumptions-and-Dependencies\|R-001]]) confirmed and documented back into [[02-System-Requirements]]. The OTT CLI (FR-106/FR-107) exists on target and the external Python test harness ([[06-Verification-and-Validation#6.3 Test Harness (Python)|doc 6]]) can drive every **Automatic** integration test. |
| 3 | Pacman Development (Host) | [[03-Software-Requirements-and-Architecture]] approved. Can proceed in parallel with Milestone 2, since it targets the host build only. | Model/Control (including maze, pellet, ghost and frightened-mode rules) covered by unit tests ([[06-Verification-and-Validation\|VT-UNIT-001..005]]); host build launches and is playable via SDL ([[06-Verification-and-Validation\|VT-INT-008]]); game-logic E2E, single-life game-over, and level-clear scenarios pass ([[06-Verification-and-Validation\|VT-INT-010]], [[06-Verification-and-Validation\|VT-INT-014]], [[06-Verification-and-Validation\|VT-INT-017]]). |
| 4 | System Integration (Target) | Milestones 2 and 3 exit met. | All remaining integration tests pass on the physical target ([[06-Verification-and-Validation\|VT-INT-009]], [[06-Verification-and-Validation\|VT-INT-011..013]], [[06-Verification-and-Validation\|VT-INT-015..016]]). |

## 4.1 Notes

- Milestones 2 and 3 have no hard dependency on each other and are expected to run concurrently to shorten the overall timeline.
- Each milestone's exit criteria are verification tests defined in [[06-Verification-and-Validation]] and traced in [[07-Traceability-Matrix]] — a milestone is not "done" until its linked tests pass.
- Per the project's git workflow, each milestone's work is expected to land as its own reviewed PR against `main`.
