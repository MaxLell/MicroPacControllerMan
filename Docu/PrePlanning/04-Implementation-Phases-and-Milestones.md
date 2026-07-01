---
title: Implementation Phases & Milestones
---

# 4 Implementation Phases / Milestones

[[Index|← Back to Index]]

This restates the phased roadmap from the original idea capture as a structured milestone table with entry/exit criteria. More phases may be inserted later if a milestone turns out to need splitting.

| # | Milestone | Entry Criteria | Exit Criteria |
|---|---|---|---|
| 0 | Pre-Planning | Idea captured (`Docu/Idea.md`). | This document set (docs 1–8) exists, is internally consistent, and is reviewed/merged. Becomes the project's source of truth. |
| 1 | Toolchain Bring-Up | Milestone 0 exit met. | Blinky LED runs on the target and its behaviour is confirmed over the STLINK V3 serial console ([[06-Verification-and-Validation\|VT-SMOKE-001]]). |
| 2 | Board Bring-Up | Milestone 1 exit met. | MCU, display, and touchpad each individually verified with reproducible tests ([[06-Verification-and-Validation\|VT-HW-001..004]], [[06-Verification-and-Validation\|VT-SMOKE-002]], [[06-Verification-and-Validation\|VT-SMOKE-003]]). Exact mikroBUS pin mapping ([[05-Risks-Assumptions-and-Dependencies\|R-001]]) confirmed and documented back into [[02-System-Requirements-and-Architecture]]. |
| 3 | Pacman Development (Host) | [[03-Software-Requirements-and-Architecture]] approved. Can proceed in parallel with Milestone 2, since it targets the host build only. | Model/Control covered by unit tests ([[06-Verification-and-Validation\|VT-HELP-001..003]]); host build launches and is playable via SDL ([[06-Verification-and-Validation\|VT-SMOKE-004]]). |
| 4 | System Integration (Target) | Milestones 2 and 3 exit met. | All acceptance tests pass on the physical target ([[06-Verification-and-Validation\|VT-ACC-001..006]]). |

## 4.1 Notes

- Milestones 2 and 3 have no hard dependency on each other and are expected to run concurrently to shorten the overall timeline.
- Each milestone's exit criteria are verification tests defined in [[06-Verification-and-Validation]] and traced in [[07-Traceability-Matrix]] — a milestone is not "done" until its linked tests pass.
- Per the project's git workflow, each milestone's work is expected to land as its own reviewed PR against `main`.

## 4.2 Related Documents

- [[05-Risks-Assumptions-and-Dependencies]] — items that could block a milestone's exit criteria.
- [[08-Troubleshooting-Guide]] — grows as each milestone surfaces real issues.
