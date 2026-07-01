---
title: Pre-Planning Index
---

# MicroPacControllerMan — Pre-Planning Documentation

This is the entry point into the Pre-Planning documentation set for the MicroPacControllerMan project (Pacman on an STM32G431RB Nucleo board with a monochrome Click display and a capacitive touchpad).

This document set is the **source of truth** for the project going forward. The original idea capture, [Idea.md](../Idea.md), remains only as historical context for how the project started.

## Documents

1. [[01-System-Overview-and-Context]] — What the system is, its boundary, external interfaces, and stakeholders.
2. [[02-System-Requirements-and-Architecture]] — System-level requirements (Functional / Non-Functional / Constraints) and the system architecture.
3. [[03-Software-Requirements-and-Architecture]] — Software-level requirements, the MVP architecture, the pub-sub message bus, and the FreeRTOS task breakdown.
4. [[04-Implementation-Phases-and-Milestones]] — The phased delivery plan, with entry/exit criteria per phase.
5. [[05-Risks-Assumptions-and-Dependencies]] — Known risks, working assumptions, and external dependencies.
6. [[06-Verification-and-Validation]] — The catalog of verification tests (hardware, helper, smoke, acceptance), each with a unique ID.
7. [[07-Traceability-Matrix]] — Cross-reference between every requirement and the test(s) that verify it.
8. [[08-Troubleshooting-Guide]] — Known-issue playbook, seeded from the risks captured in doc 5 and grown during later phases.

## ID Conventions

| Prefix | Meaning |
|---|---|
| `FR-0xx` | Functional Requirement, system-level (doc 2) |
| `FR-1xx` | Functional Requirement, software-level (doc 3) |
| `NFR-0xx` / `NFR-1xx` | Non-Functional Requirement, system / software level |
| `CON-0xx` / `CON-1xx` | Constraint, system / software level |
| `R-0xx` | Risk (doc 5) |
| `A-0xx` | Assumption (doc 5) |
| `D-0xx` | Dependency (doc 5) |
| `VT-HW-0xx` | Verification Test — hardware itself |
| `VT-HELP-0xx` | Verification Test — helper / unit tests |
| `VT-SMOKE-0xx` | Verification Test — smoke tests |
| `VT-ACC-0xx` | Verification Test — acceptance tests |

## Status

Produced during Milestone 3.1 (Pre-Planning) of [[04-Implementation-Phases-and-Milestones]]. Not yet validated against real hardware — several items are explicitly flagged as open risks/assumptions in [[05-Risks-Assumptions-and-Dependencies]] pending Board Bring-Up.
