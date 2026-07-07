# MicroPacControllerMan — Pre-Planning Documentation

This is the entry point into the Pre-Planning documentation set for the MicroPacControllerMan project (Pacman on an STM32G431RB Nucleo board with a monochrome Click display and a capacitive touchpad).

This document set is the **source of truth** for the project going forward. The original idea capture, [Idea.md](../Idea.md), remains only as historical context for how the project started.

## Documents

1. [01 System Overview & Context](01-System-Overview-and-Context.md) — What the system is, its boundary, external interfaces, and context.
2. [02 Requirements](02-Requirements.md) — All requirements (Functional / Non-Functional / Constraints), grouped by feature.
3. [03 Architecture](03-Architecture.md) — MVP architecture, the queue-based message broker, message definitions, the FreeRTOS task breakdown, the Active-Object module template, the Pacman sub-application, and the OTT test framework.
4. [04 Implementation Phases & Milestones](04-Implementation-Phases-and-Milestones.md) — The phased delivery plan, with entry/exit criteria per phase.
5. [05 Risks, Assumptions & Dependencies](05-Risks-Assumptions-and-Dependencies.md) — Known risks, working assumptions, and external dependencies.
6. [06 Verification & Validation](06-Verification-and-Validation.md) — The catalog of verification tests (Unit and Integration), each with a unique ID.
7. [07 Traceability Matrix](07-Traceability-Matrix.md) — Cross-reference between every requirement and the test(s) that verify it.
8. [08 Troubleshooting Guide](08-Troubleshooting-Guide.md) — Known-issue playbook, seeded from the risks captured in doc 5 and grown during later phases.
9. [09 OTT Mechanism & Reset Flow](09-OTT-Mechanism-and-Reset-Flow.md) — How the OTT retained-RAM/reset mechanism actually works in the reference firmware, and the corrected flow this project should adopt.

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

## Status

Produced during Milestone 3.1 (Pre-Planning) of [04 Implementation Phases & Milestones](04-Implementation-Phases-and-Milestones.md). Not yet validated against real hardware — several items are explicitly flagged as open risks/assumptions in [05 Risks, Assumptions & Dependencies](05-Risks-Assumptions-and-Dependencies.md) pending Board Bring-Up.
