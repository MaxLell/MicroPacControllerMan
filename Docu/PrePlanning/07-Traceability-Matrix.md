# 7 Traceability Matrix

[← Back to Index](Index.md)

Every requirement from [02 Requirements](02-Requirements.md) is listed below with the verification test(s) from [06 Verification & Validation](06-Verification-and-Validation.md) that verify it. The two tables group requirements only by ID band (`0xx` vs `1xx`); the numbering carries no system-vs-software meaning.

A few items are structural/process requirements rather than automatically observable runtime behaviour and are verified by build tooling, manual fault-injection, or code review rather than a dynamic `VT-*` test: CON-101 (language, enforced by the compiler), NFR-102 (coding standard), the non-observable aspects of FR-109 (run-to-completion, no shared state), and FR-111 (fatal-error halt). This is called out explicitly rather than forcing a fabricated test ID, and is itself flagged as a gap to close with an automated check (e.g. a linter run in CI) if that is ever added.

## 7.1 Product Requirements (FR-0xx / NFR-0xx / CON-0xx)

| Requirement | Name | Verified by |
|---|---|---|
| FR-001 | Loading Screen | VT-INT-011 |
| FR-002 | High Score Menu | VT-INT-011 |
| FR-003 | Game Start | VT-INT-012 |
| FR-004 | Directional Control | VT-INT-019, VT-INT-020, VT-INT-013, VT-INT-010 |
| FR-005 | Game Rendering | VT-INT-006, VT-INT-016 |
| FR-006 | Starting Lives | VT-INT-014 |
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
| FR-022 | Playfield Size | VT-UNIT-004, VT-INT-010 |
| FR-023 | End-of-Game Score Screen | VT-INT-014 |
| FR-024 | Life Lost & Respawn | VT-INT-014 |
| FR-025 | Level Count & Mazes | VT-UNIT-006, VT-INT-017 |
| FR-026 | Difficulty Scaling | VT-UNIT-006, VT-INT-017 |
| FR-027 | Game Completion | VT-INT-017 |
| NFR-001 | Loading Screen Duration | VT-INT-011 |
| NFR-002 | Rendering Rate | VT-INT-021 (the figure was chosen against it), VT-INT-016 (the game keeps it) |
| NFR-003 | Input Latency | VT-INT-020 (the drawing half), VT-INT-013 (the whole path) |
| NFR-004 | NVM Write Frequency | VT-INT-015 |
| NFR-005 | Logo Display Delay | VT-INT-011 |
| CON-001 | Target Hardware | VT-INT-001, VT-INT-002 |
| CON-002 | Display Hardware | VT-INT-018, VT-INT-006 |
| CON-003 | Input Hardware | VT-INT-019, VT-INT-020 |
| CON-004 | Carrier Hardware | VT-INT-018, VT-INT-019 |
| CON-005 | Debug Interface | VT-INT-001, VT-INT-002 |

## 7.2 Software & Test Requirements (FR-1xx / NFR-1xx / CON-1xx)

| Requirement | Name | Verified by |
|---|---|---|
| FR-101 | MVP Separation | VT-UNIT-002, VT-UNIT-003 |
| FR-102 | Stateless Control | VT-UNIT-003 |
| FR-103 | Value-Only Module Interfaces | VT-UNIT-001, VT-UNIT-002 (every payload is a value; no module holds a pointer into another's) |
| FR-104 | Host Buildability | VT-INT-008 |
| FR-105 | Cooperative Execution | VT-INT-011 (each screen reached inside its budget), VT-INT-016, VT-INT-022 (a frame stays inside NFR-002 with the loop also serving the console) |
| FR-106 | On-Target Test (OTT) Framework | VT-INT-018, VT-INT-015, VT-INT-022 (each exercises an OTT command) |
| FR-107 | OTT Result Reporting | VT-INT-018, VT-INT-015, VT-INT-022 |
| FR-108 | Message Delivery to Subscribers | VT-UNIT-001 |
| FR-110 | Pacman Internal Message Bus | VT-UNIT-001, VT-INT-010 |
| FR-111 | Fatal-Error Halt | Manual fault-injection / code review (no automated `VT-*` test — see gap note above) |
| NFR-101 | Unit Testability | VT-UNIT-002 |
| NFR-102 | Coding Standard Compliance | Code review against c-code-style (no automated `VT-*` test yet — see gap note above) |
| NFR-103 | No Runtime Heap Allocation | VT-UNIT-001 |
| NFR-104 | OTT Harness Compatibility | VT-INT-018, VT-INT-015, VT-INT-011 (harness successfully drives each Automatic test) |
| NFR-105 | Message-Bus Backpressure | VT-UNIT-001 |
| CON-101 | Language | Enforced by the C toolchain at build time (no dedicated `VT-*` test) |
| CON-102 | Test Framework | VT-UNIT-002, VT-UNIT-003 |
| CON-103 | Host View Library | VT-INT-008 |

## 7.3 Coverage Check

- Every `FR-*` / `NFR-*` requirement has at least one verifying test. No orphaned requirements found.
- Every `VT-*` test in [06 Verification & Validation](06-Verification-and-Validation.md) traces back to at least one requirement above. No orphaned tests found.
- Known gap: CON-101, NFR-102 and FR-111 rely on manual/tooling enforcement rather than an automated `VT-*` test — tracked here rather than hidden.

## 7.4 Status at close-out (2026-08-04)

The matrix above says which test *would* verify each requirement. Development ended before
two of them were built, so it is not the same as a pass list — the close-out in
[04 §4.2](04-Implementation-Phases-and-Milestones.md#42-close-out) is. In short:

- **NFR-003 (Input Latency) is not met.** ~34 ms against a 30 ms budget, 32 ms of it the shared
  debounce window ([RF-014](../Refactoring-Backlog.md#rf-014)). The figure is M2's, taken against
  `joystick_dot` rather than the finished game loop, because —
- **VT-INT-013's automatic latency measurement was never built**, so NFR-003's "whole path" column
  above names a measurement that does not exist. Directional control itself is covered
  (`ott joystick`, VT-INT-010, playing `ott pacman`); only the number is missing.
- **VT-INT-016 was never built**, so NFR-002's "the game keeps it" column is carried by
  VT-INT-021's measured 175 fps ceiling and by `ott pacman`'s operator-present frame cost rather
  than by a scripted assertion. The margin over 60 fps is large; it is simply not asserted.

Everything else in the catalogue passes: 357 host unit tests, the automatic OTT suite
(VT-INT-001/002/011/015) and the manual OTTs, both builds warning-free.
