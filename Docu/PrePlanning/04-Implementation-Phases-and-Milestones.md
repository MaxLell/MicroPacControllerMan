# 4 Implementation Phases / Milestones

[← Back to Index](Index.md)

> **Development was closed on 2026-08-04 ([DEC-028](11-Decisions-and-As-Built.md)) and reopened
> the same day ([DEC-029](11-Decisions-and-As-Built.md))** when the owner asked for randomly
> generated mazes. Milestones 0–3 are met; **Milestone 4 is met in substance but not in full**
> ([§4.2](#42-close-out) says what was left unbuilt and which requirement is unmet); **Milestone
> 5 delivered the generated mazes** ([§4.3](#43-milestone-5--random-mazes)).

This restates the phased roadmap from the original idea capture as a structured milestone table with entry/exit criteria. Test IDs link to [06 Verification & Validation](06-Verification-and-Validation.md).

| # | Milestone | Entry Criteria | Exit Criteria |
|---|---|---|---|
| 0 | Pre-Planning | Idea captured (`Docu/Idea.md`). | This document set (docs 1–10) exists, is internally consistent, and is reviewed/merged. Becomes the project's source of truth. |
| 1 | Toolchain Bring-Up | Milestone 0 exit met. | The OTT CLI framework (FR-106/FR-107) runs on the target over the ST-LINK V3 serial console, drivable by the Python harness ([VT-INT-001](06-Verification-and-Validation.md), VT-INT-002). The retained-RAM/reset mechanism ([doc 09](09-OTT-Mechanism-and-Reset-Flow.md)) — originally planned for M2 — was brought forward and built/validated here, so `ott <name>` already schedules via `.noinit`, resets, and reports PASS/FAIL on the next boot. **Met on the STM32U545RE-Q.** |
| 2 | Board Bring-Up | Milestone 1 exit met. | The MCU, the X-NUCLEO-GFX01M2's display and its joystick each verified by a reproducible OTT ([VT-INT-018](06-Verification-and-Validation.md), VT-INT-006, VT-INT-019), **and the two verified against each other** (VT-INT-020) — separately-passing halves do not prove their coordinate systems agree. The shield pin map confirmed on hardware and documented in [M2 Board Bring-Up §1](../Design/M2-Board-Bring-Up.md), which is where hardware detail belongs; [02 Requirements](02-Requirements.md) carries only the constraint that the shield is used (CON-002..004). The timing budgets settled by measurement rather than assumption: NFR-002's rate chosen against VT-INT-021, and what NFR-003 actually costs known. The external Python harness ([doc 6 §6.3](06-Verification-and-Validation.md#63-test-harness-python)) drives every **Automatic** integration test. |
| 3 | Pacman Development (Host) | [03 Architecture](03-Architecture.md) and [10 Pacman Game Design](10-Pacman-Game-Design.md) approved. Can proceed in parallel with Milestone 2, since it targets the host build only. | Model/Control (including maze, pellet, ghost and frightened-mode rules) covered by unit tests ([VT-UNIT-001..005](06-Verification-and-Validation.md)); host build launches and is playable via SDL ([VT-INT-008](06-Verification-and-Validation.md)); game-logic E2E, single-life game-over, and level-clear scenarios pass ([VT-INT-010](06-Verification-and-Validation.md), VT-INT-014, VT-INT-017). |
| 4 | System Integration (Target) | Milestones 2 and 3 exit met. | All remaining integration tests pass on the physical target ([VT-INT-011..013](06-Verification-and-Validation.md), VT-INT-015, VT-INT-016, VT-INT-022). What is left for this milestone is the measurement M3 did not make automatic: input latency (NFR-003) and the frame rate under a real run (NFR-002). **Partially met at close-out — see [§4.2](#42-close-out).** |

| 5 | Random Mazes | Milestone 4 in substance; asked for after the project had been closed. | Every level plays a maze generated for it (FR-029), with the properties that requirement lists checked over many seeds; the maze's appearance derived from its walls rather than written down beside them; the whole thing playable on the target. **Met** — see [§4.3](#43-milestone-5--random-mazes). |

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

Everything else knowingly left undone is in the [Refactoring Backlog](../Refactoring-Backlog.md).

## 4.3 Milestone 5 — Random Mazes

Asked for on **2026-08-04**, after the close-out above and on the same day: a generated maze
instead of the arcade's one layout ([DEC-029](11-Decisions-and-As-Built.md)). Delivered, with
the *how* in [M4 Random Mazes](../Design/M4-Random-Mazes.md).

**Met.** `App/maze_gen` is a faithful port of the tetris-stacking generator from
[shaunlebron/pacman-mazegen](https://github.com/shaunlebron/pacman-mazegen), verified against the
original by running both under the same seeded PRNG and comparing output **byte for byte over 300
seeds**. `game_view` derives the maze's appearance from its walls
([DEC-030](11-Decisions-and-As-Built.md)) — checked against the arcade's own hand-drawn tile map,
**764 of 764 cells outside the tunnel masses**. 370 host unit tests pass, 11 of them new and
asserting FR-029's properties over 100 seeds each. Verified on hardware: loading → menu → game,
the ghosts hunt in a generated maze, the console still answers, and the frame cost is unchanged
at 8 ms of 16.

**What this milestone did not change**, and says so rather than letting the close-out above go
stale: NFR-003 (input latency) is still unmet, VT-INT-013's latency measurement and VT-INT-016
are still unbuilt. It did measure one thing the close-out had asserted too comfortably —
**the achieved frame rate is 59 fps, not the 60 NFR-002 asks for**, and it was 59 before this
milestone too. The cause is the 16 ms frame period re-armed inside its own callback, so a period
is nearer 16.9 ms; the 175 fps in the M2 documents is the *unpaced* ceiling and a different
measurement. Not fixed here, because changing the frame period would have changed what the
before/after frame-cost comparison was comparing.
