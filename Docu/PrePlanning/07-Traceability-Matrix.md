---
title: Traceability Matrix
---

# 7 Traceability Matrix

[[Index|← Back to Index]]

Every requirement from [[02-System-Requirements-and-Architecture]] and [[03-Software-Requirements-and-Architecture]] is listed below with the verification test(s) from [[06-Verification-and-Validation]] that verify it.

Two requirements (CON-101, NFR-102) are structural/process constraints rather than runtime behaviour — they are verified by build tooling or code review rather than a dynamic `VT-*` test. This is called out explicitly rather than forcing a fabricated test ID, and is itself flagged as a gap to close with an automated check (e.g. a linter run in CI) if that is ever added.

## 7.1 System Requirements

| Requirement | Name | Verified by |
|---|---|---|
| FR-001 | Loading Screen | VT-ACC-001 |
| FR-002 | High Score Menu | VT-ACC-001 |
| FR-003 | Game Start | VT-ACC-002 |
| FR-004 | Directional Control | VT-ACC-003 |
| FR-005 | Game Rendering | VT-SMOKE-002, VT-ACC-006 |
| FR-006 | Single Life | VT-ACC-004 |
| FR-007 | Game Over | VT-ACC-004 |
| FR-008 | High Score Update | VT-ACC-005 |
| FR-009 | High Score Persistence | VT-ACC-005 |
| NFR-001 | Loading Screen Duration | VT-ACC-001 |
| NFR-002 | Rendering Rate | VT-ACC-006 |
| NFR-003 | Input Latency | VT-ACC-003 |
| NFR-004 | NVM Write Frequency | VT-ACC-005 |
| CON-001 | Target Hardware | VT-HW-001, VT-SMOKE-001 |
| CON-002 | Display Hardware | VT-HW-002, VT-SMOKE-002 |
| CON-003 | Input Hardware | VT-HW-003, VT-SMOKE-003 |
| CON-004 | Carrier Hardware | VT-HW-002, VT-HW-003 |
| CON-005 | Debug Interface | VT-HW-001, VT-HW-004, VT-SMOKE-003 |

## 7.2 Software Requirements

| Requirement | Name | Verified by |
|---|---|---|
| FR-101 | MVP Separation | VT-HELP-002, VT-HELP-003 |
| FR-102 | Stateless Control | VT-HELP-003 |
| FR-103 | Pub-Sub Communication | VT-HELP-001 |
| FR-104 | Host Buildability | VT-SMOKE-004 |
| FR-105 | FreeRTOS Task Separation | VT-SMOKE-005 |
| NFR-101 | Unit Testability | VT-HELP-002 |
| NFR-102 | Coding Standard Compliance | Code review against c-code-style (no automated `VT-*` test yet — see gap note above) |
| NFR-103 | No Runtime Heap Allocation | VT-HELP-001 |
| CON-101 | Language | Enforced by the C toolchain at build time (no dedicated `VT-*` test) |
| CON-102 | Test Framework | VT-HELP-002, VT-HELP-003 |
| CON-103 | Host View Library | VT-SMOKE-004 |
| CON-104 | RTOS | VT-SMOKE-005 |

## 7.3 Coverage Check

- Every `FR-*`/`NFR-*` requirement has at least one verifying test. No orphaned requirements found.
- Every `VT-*` test in [[06-Verification-and-Validation]] traces back to at least one requirement above. No orphaned tests found.
- Known gap: NFR-102 and CON-101 rely on manual/tooling enforcement rather than an automated `VT-*` test — tracked here rather than hidden.

## 7.4 Related Documents

- [[06-Verification-and-Validation]]
- [[05-Risks-Assumptions-and-Dependencies]]
