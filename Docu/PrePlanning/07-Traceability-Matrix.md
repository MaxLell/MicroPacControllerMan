# 7 Traceability Matrix

[← Back to Index](Index.md)

Every requirement from [02 Requirements](02-Requirements.md) is listed below with the verification test(s) from [06 Verification & Validation](06-Verification-and-Validation.md) that verify it. The two tables group requirements only by ID band (`0xx` vs `1xx`); the numbering carries no system-vs-software meaning.

Three items are structural/process requirements rather than runtime behaviour and are verified by build tooling or code review rather than a dynamic `VT-*` test: CON-101 (language, enforced by the compiler), NFR-102 (coding standard), and the non-observable aspects of FR-109 (run-to-completion, no shared state). This is called out explicitly rather than forcing a fabricated test ID, and is itself flagged as a gap to close with an automated check (e.g. a linter run in CI) if that is ever added.

## 7.1 Product Requirements (FR-0xx / NFR-0xx / CON-0xx)

| Requirement | Name | Verified by |
|---|---|---|
| FR-001 | Loading Screen | VT-INT-011 |
| FR-002 | High Score Menu | VT-INT-011 |
| FR-003 | Game Start | VT-INT-012 |
| FR-004 | Directional Control | VT-INT-007, VT-INT-013, VT-INT-010 |
| FR-005 | Game Rendering | VT-INT-006, VT-INT-016 |
| FR-006 | Single Life | VT-INT-014 |
| FR-007 | Game Over | VT-INT-014 |
| FR-008 | High Score Update | VT-INT-015 |
| FR-009 | High Score Persistence | VT-INT-015 |
| FR-010 | Maze Confinement | VT-UNIT-004, VT-INT-010 |
| FR-011 | Pellet Consumption | VT-UNIT-004, VT-INT-010 |
| FR-012 | Tunnel Wrap-Around | VT-UNIT-004, VT-INT-010 |
| FR-013 | Ghost Presence | VT-INT-010 |
| FR-014 | Distinct Ghost Behaviors | VT-UNIT-005, VT-INT-010 |
| FR-015 | Scatter / Chase Alternation | VT-UNIT-005, VT-INT-010 |
| FR-016 | Ghost Collision | VT-UNIT-005, VT-INT-014 |
| FR-017 | Power Pellet Consumption | VT-UNIT-004, VT-UNIT-005, VT-INT-010 |
| FR-018 | Frightened Ghosts | VT-UNIT-005, VT-INT-010 |
| FR-019 | Eating Frightened Ghosts | VT-UNIT-005, VT-INT-010 |
| FR-020 | Frightened Timeout | VT-UNIT-005, VT-INT-010 |
| FR-021 | Level Clear | VT-INT-017 |
| NFR-001 | Loading Screen Duration | VT-INT-011 |
| NFR-002 | Rendering Rate | VT-INT-016 |
| NFR-003 | Input Latency | VT-INT-013 |
| NFR-004 | NVM Write Frequency | VT-INT-015 |
| NFR-005 | Logo Display Delay | VT-INT-011 |
| CON-001 | Target Hardware | VT-INT-001, VT-INT-005 |
| CON-002 | Display Hardware | VT-INT-003, VT-INT-006 |
| CON-003 | Input Hardware | VT-INT-004, VT-INT-007 |
| CON-004 | Carrier Hardware | VT-INT-003, VT-INT-004 |
| CON-005 | Debug Interface | VT-INT-001, VT-INT-002 |

## 7.2 Software & Test Requirements (FR-1xx / NFR-1xx / CON-1xx)

| Requirement | Name | Verified by |
|---|---|---|
| FR-101 | MVP Separation | VT-UNIT-002, VT-UNIT-003 |
| FR-102 | Stateless Control | VT-UNIT-003 |
| FR-103 | Message-Based Communication | VT-UNIT-001 |
| FR-104 | Host Buildability | VT-INT-008 |
| FR-105 | FreeRTOS Task Separation | VT-INT-009 |
| FR-106 | On-Target Test (OTT) Framework | VT-INT-005, VT-INT-009, VT-INT-015 (each exercises an OTT command) |
| FR-107 | OTT Result Reporting | VT-INT-005, VT-INT-009, VT-INT-015 |
| FR-108 | Dedicated Message-Broker Task | VT-UNIT-001, VT-INT-009 |
| FR-109 | Active-Object Modules | VT-INT-009 (task-per-module observable); run-to-completion / no-shared-state by code review — see gap note above |
| FR-110 | Pacman Internal Message Bus | VT-UNIT-001, VT-INT-010 |
| NFR-101 | Unit Testability | VT-UNIT-002 |
| NFR-102 | Coding Standard Compliance | Code review against c-code-style (no automated `VT-*` test yet — see gap note above) |
| NFR-103 | No Runtime Heap Allocation | VT-UNIT-001 |
| NFR-104 | OTT Harness Compatibility | VT-INT-005, VT-INT-009, VT-INT-015 (harness successfully drives each Automatic test) |
| NFR-105 | Message-Bus Backpressure | VT-UNIT-001 |
| CON-101 | Language | Enforced by the C toolchain at build time (no dedicated `VT-*` test) |
| CON-102 | Test Framework | VT-UNIT-002, VT-UNIT-003 |
| CON-103 | Host View Library | VT-INT-008 |
| CON-104 | RTOS | VT-INT-009 |

## 7.3 Coverage Check

- Every `FR-*` / `NFR-*` requirement has at least one verifying test. No orphaned requirements found.
- Every `VT-*` test in [06 Verification & Validation](06-Verification-and-Validation.md) traces back to at least one requirement above. No orphaned tests found.
- Known gap: CON-101, NFR-102, and the non-observable aspects of FR-109 rely on manual/tooling enforcement rather than an automated `VT-*` test — tracked here rather than hidden.
