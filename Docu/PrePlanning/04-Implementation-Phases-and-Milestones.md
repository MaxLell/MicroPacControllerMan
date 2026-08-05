# 4 Implementation Phases / Milestones

[← Back to Index](Index.md)

> **Development was closed on 2026-08-04 ([DEC-028](11-Decisions-and-As-Built.md)) and reopened
> the same day ([DEC-029](11-Decisions-and-As-Built.md))** when the owner asked for randomly
> generated mazes. Milestones 0–3 are met; **Milestone 4 is met**, with one catalogued test never
> built ([§4.2](#42-close-out) says which, and why nothing is owed for it); **Milestone 5
> delivered the generated mazes** ([§4.3](#43-milestone-5--random-mazes)). **Milestone 6 — a
> Pacman AI — was asked for on 2026-08-05 and is in progress**
> ([DEC-038](11-Decisions-and-As-Built.md), [§4.4](#44-milestone-6--pacman-ai)): requirements and
> design agreed, no code yet.

This restates the phased roadmap from the original idea capture as a structured milestone table with entry/exit criteria. Test IDs link to [06 Verification & Validation](06-Verification-and-Validation.md).

| # | Milestone | Entry Criteria | Exit Criteria |
|---|---|---|---|
| 0 | Pre-Planning | Idea captured (`Docu/Idea.md`). | This document set (docs 1–10) exists, is internally consistent, and is reviewed/merged. Becomes the project's source of truth. |
| 1 | Toolchain Bring-Up | Milestone 0 exit met. | The OTT CLI framework (FR-106/FR-107) runs on the target over the ST-LINK V3 serial console, drivable by the Python harness ([VT-INT-001](06-Verification-and-Validation.md), VT-INT-002). The retained-RAM/reset mechanism ([doc 09](09-OTT-Mechanism-and-Reset-Flow.md)) — originally planned for M2 — was brought forward and built/validated here, so `ott <name>` already schedules via `.noinit`, resets, and reports PASS/FAIL on the next boot. **Met on the STM32U545RE-Q.** |
| 2 | Board Bring-Up | Milestone 1 exit met. | The MCU, the X-NUCLEO-GFX01M2's display and its joystick each verified by a reproducible OTT ([VT-INT-018](06-Verification-and-Validation.md), VT-INT-006, VT-INT-019), **and the two verified against each other** (VT-INT-020) — separately-passing halves do not prove their coordinate systems agree. The shield pin map confirmed on hardware and documented in [M2 Board Bring-Up §1](../Design/M2-Board-Bring-Up.md), which is where hardware detail belongs; [02 Requirements](02-Requirements.md) carries only the constraint that the shield is used (CON-002..004). The timing budgets settled by measurement rather than assumption: the frame rate chosen against VT-INT-021, and what the input path actually costs known. The external Python harness ([doc 6 §6.3](06-Verification-and-Validation.md#63-test-harness-python)) drives every **Automatic** integration test. |
| 3 | Pacman Development (Host) | [03 Architecture](03-Architecture.md) and [10 Pacman Game Design](10-Pacman-Game-Design.md) approved. Can proceed in parallel with Milestone 2, since it targets the host build only. | Model/Control (including maze, pellet, ghost and frightened-mode rules) covered by unit tests ([VT-UNIT-001..005](06-Verification-and-Validation.md)); host build launches and is playable via SDL ([VT-INT-008](06-Verification-and-Validation.md)); game-logic E2E, single-life game-over, and level-clear scenarios pass ([VT-INT-010](06-Verification-and-Validation.md), VT-INT-014, VT-INT-017). |
| 4 | System Integration (Target) | Milestones 2 and 3 exit met. | All remaining integration tests pass on the physical target ([VT-INT-011..013](06-Verification-and-Validation.md), VT-INT-015, VT-INT-022). What was left for this milestone was the measurement M3 did not make automatic: input latency and the frame rate under a real run — both withdrawn with the requirements they served ([DEC-036](11-Decisions-and-As-Built.md)). **Partially met at close-out — see [§4.2](#42-close-out).** |

| 5 | Random Mazes | Milestone 4 in substance; asked for after the project had been closed. | Every level plays a maze generated for it (FR-029), with the properties that requirement lists checked over many seeds; the maze's appearance derived from its walls rather than written down beside them; the whole thing playable on the target. **Met** — see [§4.3](#43-milestone-5--random-mazes). |
| 6 | Pacman AI | Milestone 5 met; asked for on 2026-08-05 ([DEC-038](11-Decisions-and-As-Built.md)). | An agent trained on the host reaches FR-037's score on generated mazes ([VT-UNIT-010](06-Verification-and-Validation.md)); the player can hand over to it and take back control on the board, with the HUD saying so and the run locked out of the high-score table (FR-030..034, VT-INT-023); and the ported inference chooses the same direction as the host over a recorded state set (FR-039, VT-INT-024). **In progress** — see [§4.4](#44-milestone-6--pacman-ai). |

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

**Milestone 4: met.** Verified on the target: **VT-INT-011** (boot sequence) unattended in
`run_ott.py --suite`, **VT-INT-012**'s flow automatically via the `start` console command,
**VT-INT-015** (high-score round trip in real flash) unattended, and **VT-INT-022** (`ott pacman` —
the rules, the view, the panel, the stick and the frame budget all real at once) with an operator at
the board.

**VT-INT-013** (Directional Movement) has no scenario of its own; that directional control works is
covered by M2's `ott joystick` and `joystick_dot`, the host's VT-INT-010, and playing `ott pacman`.
Its automatic latency half, and VT-INT-016 entirely, went with the two non-functional requirements
they existed to measure — a rendering rate and an input latency the owner judged irrelevant to this
game ([DEC-036](11-Decisions-and-As-Built.md)).

Work knowingly left undone is in the [Refactoring Backlog](../Refactoring-Backlog.md).

## 4.3 Milestone 5 — Random Mazes

Asked for on **2026-08-04**, after the close-out above and on the same day: a generated maze
instead of the arcade's one layout ([DEC-029](11-Decisions-and-As-Built.md)). Delivered, with
the *how* in [M4 Random Mazes](../Design/M4-Random-Mazes.md).

**Met.** `App/maze_gen` is a faithful port of the tetris-stacking generator from
[shaunlebron/pacman-mazegen](https://github.com/shaunlebron/pacman-mazegen), verified against the
original by running both under the same seeded PRNG and comparing output **byte for byte over 300
seeds**. `game_view` derives the maze's appearance from its walls
([DEC-030](11-Decisions-and-As-Built.md)), and by the end of the milestone draws them as geometry
rather than from a tile alphabet ([DEC-034](11-Decisions-and-As-Built.md)) — checked by rebuilding
the picture and measuring it: **every pellet within a pixel of its corridor's centre, every tunnel
mouth exactly a corridor's gap wide**. 371 host unit tests pass, 11 of them new and
asserting FR-029's properties over 100 seeds each. Verified on hardware: loading → menu → game,
the ghosts hunt in a generated maze, the console still answers, and the frame cost is unchanged
at 8 ms of 16.

**What this milestone measured and did not change:** the achieved frame rate is 58–59 fps, and it
was before this work too. The cause is the 16 ms frame period re-armed inside its own callback, so a period
is nearer 16.9 ms; the 175 fps in the M2 documents is the *unpaced* ceiling and a different
measurement. Nothing is measured against it any more — the requirement that asked for 60 is
withdrawn ([DEC-036](11-Decisions-and-As-Built.md)) — and the period is left alone because changing
it would have changed what the before/after frame-cost comparison was comparing.

## 4.4 Milestone 6 — Pacman AI

Asked for on **2026-08-05**, after Milestone 5 ([DEC-038](11-Decisions-and-As-Built.md)): an agent
that learns to play, trained on the host, ported to the board, and switched on and off by the
player. The *how* is in [M6 Pacman AI](../Design/M6-Pacman-AI.md).

**In progress.** The requirements ([02 §2.1.11](02-Requirements.md)) and the design are agreed;
no code is written yet.

What is settled, and why it is worth stating before any of it is built:

- **The training environment is the shipped game** (FR-112), because `game_t` already is one.
  Measured: no file-scope state, so environments are an array of structs; time is injected, so
  nothing paces to real time; no `rand()` outside the seeded `maze_gen`, so an episode replays;
  and **15,429 steps/s on one core** with the host library unoptimised.
- **Neuroevolution (NEAT)**, not gradient RL — the owner's resources point there twice over, one
  genome is one independent episode so FR-113 falls out of the algorithm, and the networks it
  grows are three orders of magnitude inside NFR-007. Budget: about **12 s per generation on 8
  cores**, so a Code-Bullet-length run of 73 generations is a quarter of an hour.
- **Relative observations and relative actions** — forward/left/right/back rather than the
  compass. This is the decision the milestone stands or falls on: every level's maze is generated
  (FR-029), so a policy that has learned absolute cell positions has learned nothing
  transferable.
- **The two halves are tied together by measurement, not trust** (FR-039), the same way the maze
  generator was checked against its original. Two traps are designed out rather than debugged:
  the host's promotion of `float` to `double`, and transcendental activations that host libm and
  newlib need not agree on to the last bit.

**FR-037's bar is anchored on a measurement**: the mean score of a uniform-random policy is
**464.3 points** (median 440, best 1,440, over 329 episodes), so the required 4,600 is ten times
what flailing achieves — and, for scale, a cleared level 1 is worth about 2,600 before a single
ghost is eaten.
