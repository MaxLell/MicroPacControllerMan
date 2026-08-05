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
| FR-005 | Game Rendering | VT-INT-006, VT-INT-022 |
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
| FR-029 | Randomly Generated Maze | VT-UNIT-007, VT-UNIT-008 (its appearance), VT-INT-022 (played on the board) |
| NFR-001 | Loading Screen Duration | VT-INT-011 |
| NFR-004 | NVM Write Frequency | VT-INT-015 |
| NFR-005 | Logo Display Delay | VT-INT-011 |
| CON-001 | Target Hardware | VT-INT-001, VT-INT-002 |
| CON-002 | Display Hardware | VT-INT-018, VT-INT-006 |
| CON-003 | Input Hardware | VT-INT-019, VT-INT-020 |
| CON-004 | Carrier Hardware | VT-INT-018, VT-INT-019 |
| CON-005 | Debug Interface | VT-INT-001, VT-INT-002 |

| FR-030 | AI Takeover Toggle | VT-INT-023 |
| FR-031 | AI Control Exclusivity | VT-INT-023 |
| FR-032 | AI Takeover Indication | VT-INT-023 |
| FR-033 | AI Control Persistence | VT-INT-023 |
| FR-034 | AI Run Not Recorded | VT-INT-025 |
| FR-035 | Observation Bounded by the Display | VT-UNIT-009 |
| FR-036 | Score as the AI's Objective | VT-UNIT-010 |
| FR-037 | AI Play Strength | VT-UNIT-010 |
| FR-038 | Inference Only on the Target | Code review (no training code is linked into the target build — see gap note above) |
| FR-039 | Host / Target Inference Equivalence | VT-UNIT-011 (host half), VT-INT-024 (target half) |
| NFR-006 | AI Inference Budget | VT-INT-023 |
| NFR-007 | Model Footprint | Build tooling (the linker's size report against the stated limits) |
| NFR-008 | No Heap for Inference | Code review (no allocator is linked; weights are `const`) |

## 7.2 Software & Test Requirements (FR-1xx / NFR-1xx / CON-1xx)

| Requirement | Name | Verified by |
|---|---|---|
| FR-101 | MVP Separation | VT-UNIT-002, VT-UNIT-003 |
| FR-102 | Stateless Control | VT-UNIT-003 |
| FR-103 | Value-Only Module Interfaces | VT-UNIT-001, VT-UNIT-002 (every payload is a value; no module holds a pointer into another's) |
| FR-104 | Host Buildability | VT-INT-008 |
| FR-105 | Cooperative Execution | VT-INT-011 (each screen reached inside its budget), VT-INT-022 (a frame still has room with the loop also serving the console) |
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

## 7.4 Status

Every requirement in the matrix above has a test, and every one of those tests passes: **371 host
unit tests**, the automatic OTT suite (VT-INT-001/002/011/015) and the manual OTTs, both builds
warning-free.

Two non-functional requirements used to sit here as *unmet* — a rendering rate of 60 FPS and an
input latency of 30 ms. They are **withdrawn**, not satisfied: the owner judged both irrelevant to
this game ([DEC-036](11-Decisions-and-As-Built.md)). The figures behind them survive as design
figures where they still explain something — the 16 ms frame period, the interpolation, the
debounce window — but nothing is measured against them any more.
