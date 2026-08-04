# MicroPacControllerMan — Pre-Planning Documentation

This is the entry point into the Pre-Planning documentation set for the MicroPacControllerMan project (Pacman on an STM32U545RE-Q Nucleo board with a 240 × 320 colour display and a joystick).

This document set is the **source of truth** for the project going forward. The original idea capture, [Idea.md](../Idea.md), remains only as historical context for how the project started.

## Documents

1. [01 System Overview & Context](01-System-Overview-and-Context.md) — What the system is, its boundary, external interfaces, and context.
2. [02 Requirements](02-Requirements.md) — All requirements (Functional / Non-Functional / Constraints), grouped by feature.
3. [03 Architecture](03-Architecture.md) — MVP architecture, the queue-based message broker and where it is used, message definitions, the cooperative execution model (§3.4), the Active-Object module template, the Pacman sub-application, the OTT test framework, and the layered firmware source-tree layout (§3.9).
4. [04 Implementation Phases & Milestones](04-Implementation-Phases-and-Milestones.md) — The phased delivery plan, with entry/exit criteria per phase.
5. [05 Risks, Assumptions & Dependencies](05-Risks-Assumptions-and-Dependencies.md) — Known risks, working assumptions, and external dependencies.
6. [06 Verification & Validation](06-Verification-and-Validation.md) — The catalog of verification tests (Unit and Integration), each with a unique ID.
7. [07 Traceability Matrix](07-Traceability-Matrix.md) — Cross-reference between every requirement and the test(s) that verify it.
8. [08 Troubleshooting Guide](08-Troubleshooting-Guide.md) — Known-issue playbook, seeded from the risks captured in doc 5 and grown during later phases.
9. [09 OTT Mechanism & Reset Flow](09-OTT-Mechanism-and-Reset-Flow.md) — How the OTT retained-RAM/reset mechanism actually works in the reference firmware, and the corrected flow this project should adopt.
10. [10 Pacman Game Design](10-Pacman-Game-Design.md) — The concrete game rules for implementation: maze layout, movement/tick model, ghost behaviour, power pellets, scoring, and end conditions.
11. [11 Decisions & As-Built](11-Decisions-and-As-Built.md) — Implementation decisions actually taken and where the built firmware deviates from the intended design (as-built vs. planned).

### Design documents (one per milestone)

The Pre-Planning set says *what* the system must do. Each milestone gets its own design
document under [`Docu/Design/`](../Design/) carrying the *how* — pin assignments, clock
settings, transfer budgets, tool choices, and the questions that milestone must answer.

- [M2 Board Bring-Up](../Design/M2-Board-Bring-Up.md) — display and joystick on the X-NUCLEO-GFX01M2.

### See also (not part of the spec set)

- [Refactoring Backlog](../Refactoring-Backlog.md) — **closed.** The record of work deliberately not done: noticed in passing, deferred by decision, or blocked. It was a living work list while the project ran, which is why it sits outside the numbered set.

## ID Conventions

| Prefix | Meaning |
|---|---|
| `FR-xxx` | Functional Requirement ([02 Requirements](02-Requirements.md)). The `0xx` / `1xx` split is a historical identifier only and no longer implies a system-vs-software distinction. |
| `NFR-xxx` | Non-Functional Requirement ([02 Requirements](02-Requirements.md)) |
| `CON-xxx` | Constraint ([02 Requirements](02-Requirements.md)) |
| `R-0xx` | Risk ([05](05-Risks-Assumptions-and-Dependencies.md)) |
| `A-0xx` | Assumption ([05](05-Risks-Assumptions-and-Dependencies.md)) |
| `D-0xx` | Dependency ([05](05-Risks-Assumptions-and-Dependencies.md)) |
| `VT-UNIT-0xx` | Verification Test — unit test (small scope, host-side) |
| `VT-INT-0xx` | Verification Test — integration test (large scope, target and/or host) |
| `DEC-0xx` | Implementation decision / as-built record ([11](11-Decisions-and-As-Built.md)) |
| `RF-0xx` | Refactoring-backlog item ([Refactoring Backlog](../Refactoring-Backlog.md)) — outside the spec set |

## Status

**Development is finished (2026-08-04, [DEC-028](11-Decisions-and-As-Built.md)).** This set is
closed: it describes a firmware that is built, plays on the board, and will not change. Where a
document names the earlier STM32G431RB or the monochrome LS013B7DH03 panel, it is deliberate
history — the pivot to the STM32U545RE-Q and the X-NUCLEO-GFX01M2 is recorded in
[11 Decisions & As-Built](11-Decisions-and-As-Built.md), which is where superseded choices belong.

Read the close-out first if you are arriving cold:
[04 §4.2](04-Implementation-Phases-and-Milestones.md#42-close-out) says which milestones were met,
which two catalogued tests were never built, and which requirement (NFR-003, input latency) ends
unmet. The [Refactoring Backlog](../Refactoring-Backlog.md) is closed alongside it and names every
wart the project chose to live with.
