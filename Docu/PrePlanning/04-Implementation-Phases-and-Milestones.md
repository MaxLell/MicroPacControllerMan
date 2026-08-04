# 4 Implementation Phases / Milestones

[← Back to Index](Index.md)

> **Development is finished (2026-08-04, [DEC-028](11-Decisions-and-As-Built.md)).** No further
> milestone is planned and no phase will be split. Milestones 0–3 are met; **Milestone 4 is
> met in substance but not in full** — see [§4.2](#42-close-out) for exactly what was left
> unbuilt and which requirement ends unmet.

This restates the phased roadmap from the original idea capture as a structured milestone table with entry/exit criteria. Test IDs link to [06 Verification & Validation](06-Verification-and-Validation.md).

| # | Milestone | Entry Criteria | Exit Criteria |
|---|---|---|---|
| 0 | Pre-Planning | Idea captured (`Docu/Idea.md`). | This document set (docs 1–10) exists, is internally consistent, and is reviewed/merged. Becomes the project's source of truth. |
| 1 | Toolchain Bring-Up | Milestone 0 exit met. | The OTT CLI framework (FR-106/FR-107) runs on the target over the ST-LINK V3 serial console, drivable by the Python harness ([VT-INT-001](06-Verification-and-Validation.md), VT-INT-002). The retained-RAM/reset mechanism ([doc 09](09-OTT-Mechanism-and-Reset-Flow.md)) — originally planned for M2 — was brought forward and built/validated here, so `ott <name>` already schedules via `.noinit`, resets, and reports PASS/FAIL on the next boot. **Met on the STM32U545RE-Q.** |
| 2 | Board Bring-Up | Milestone 1 exit met. | The MCU, the X-NUCLEO-GFX01M2's display and its joystick each verified by a reproducible OTT ([VT-INT-018](06-Verification-and-Validation.md), VT-INT-006, VT-INT-019), **and the two verified against each other** (VT-INT-020) — separately-passing halves do not prove their coordinate systems agree. The shield pin map confirmed on hardware and documented in [M2 Board Bring-Up §1](../Design/M2-Board-Bring-Up.md), which is where hardware detail belongs; [02 Requirements](02-Requirements.md) carries only the constraint that the shield is used (CON-002..004). The timing budgets settled by measurement rather than assumption: NFR-002's rate chosen against VT-INT-021, and what NFR-003 actually costs known. The external Python harness ([doc 6 §6.3](06-Verification-and-Validation.md#63-test-harness-python)) drives every **Automatic** integration test. |
| 3 | Pacman Development (Host) | [03 Architecture](03-Architecture.md) and [10 Pacman Game Design](10-Pacman-Game-Design.md) approved. Can proceed in parallel with Milestone 2, since it targets the host build only. | Model/Control (including maze, pellet, ghost and frightened-mode rules) covered by unit tests ([VT-UNIT-001..005](06-Verification-and-Validation.md)); host build launches and is playable via SDL ([VT-INT-008](06-Verification-and-Validation.md)); game-logic E2E, single-life game-over, and level-clear scenarios pass ([VT-INT-010](06-Verification-and-Validation.md), VT-INT-014, VT-INT-017). |
| 4 | System Integration (Target) | Milestones 2 and 3 exit met. | All remaining integration tests pass on the physical target ([VT-INT-011..013](06-Verification-and-Validation.md), VT-INT-015, VT-INT-016, VT-INT-022). What is left for this milestone is the measurement M3 did not make automatic: input latency (NFR-003) and the frame rate under a real run (NFR-002). **Partially met at close-out — see [§4.2](#42-close-out).** |

## 4.1 Notes

- Milestones 2 and 3 had no hard dependency on each other and ran concurrently.
- Each milestone's exit criteria are verification tests defined in [06 Verification & Validation](06-Verification-and-Validation.md) and traced in [07 Traceability Matrix](07-Traceability-Matrix.md) — a milestone was not "done" until its linked tests passed.
- Per the project's git workflow, each milestone's work landed as its own reviewed PR against `main`.

## 4.2 Close-out

Development ended on **2026-08-04** by the owner's decision ([DEC-028](11-Decisions-and-As-Built.md)).
The game is complete and plays on the board: the shell's loading screen, menu, run and score
screen; the arcade's 28 × 31 maze, its per-level speed progression to level 21, its ghosts and
its 1980 sprite ROMs; three high scores in a linker-reserved flash page. RAM **67.3 %** (176,428 of
256 kB), flash **17.3 %** (89,496 of the 504 kB the linker leaves the firmware after reserving the
high-score page).

**Milestones 0–3: met.** Every exit criterion listed above is satisfied.

**Milestone 4: met in substance, not in full.** Verified on the target: **VT-INT-011** (boot
sequence) unattended in `run_ott.py --suite`, **VT-INT-012**'s flow automatically via the `start`
console command, **VT-INT-015** (high-score round trip in real flash) unattended, and
**VT-INT-022** (`ott pacman` — the rules, the view, the panel, the stick and the frame budget all
real at once) with an operator at the board. Two of the catalogued tests were **never built**:

- **VT-INT-013** (Directional Movement) has no scenario of its own. That directional control
  works is covered elsewhere — M2's `ott joystick` and `joystick_dot`, the host's VT-INT-010,
  and playing `ott pacman` — but its **latency measurement** was the M4 deliverable, and no
  serial-timestamp instrumentation exists, so input latency was never measured against the
  finished game loop. The only figure the project has is M2's, against `joystick_dot`, and that
  figure is the problem below.
- **VT-INT-016** (NFR-002 under a scripted run) was never implemented. `ott pacman` reports frame
  cost with an operator present, and M2's VT-INT-021 measured the unpaced ceiling at 175 fps
  against a 60 fps requirement, so the margin is known to be large — it is simply not asserted
  by a script.

**One requirement ends unmet: NFR-003.** The input path is ~34 ms against a 30 ms budget — 32 ms
of it the shared debounce window ([RF-014](../Refactoring-Backlog.md#rf-014)), which was left at
the `uint32_t` width of the history register rather than the conventional 8 samples that would
have brought the path to ~10 ms. The overshoot is 13 %, it is a chosen deferral rather than a
discovered defect, and nobody playing the game reported input lag — but the requirement as
written is not satisfied, and closing the project does not satisfy it.

Everything else knowingly left undone is in the [Refactoring Backlog](../Refactoring-Backlog.md),
now closed with each item marked as what it is.
