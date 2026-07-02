---
title: Software Requirements & Software Architecture
---

# 3 Software Requirements & Software Architecture

[[Index|← Back to Index]] · See also [[02-System-Requirements]]

## 3.1 Functional Requirements

| Unique-ID | Name | Description |
|---|---|---|
| FR-101 | MVP Separation | The software shall implement the game using Model-View-Control (MVP) architecture, separating game state (Model), rendering (View), and game logic (Control) into distinct components. |
| FR-102 | Stateless Control | The Control component shall not persist game state between invocations; all game state shall reside exclusively in the Model. |
| FR-103 | Pub-Sub Communication | Firmware modules shall exchange data exclusively through published/subscribed messages on the message bus, not through direct cross-module function calls. |
| FR-104 | Host Buildability | The Model and Control components shall be buildable and executable unmodified on the host computer, using SDL to implement the View. |
| FR-105 | FreeRTOS Task Separation | The firmware shall execute input handling, rendering, game-logic ticking, and persistence as separate FreeRTOS tasks. |
| FR-106 | On-Target Test (OTT) Framework | The firmware shall expose each on-target test named in [[06-Verification-and-Validation]] as an individual command over the serial console CLI, following an OTT (On-Target Test) pattern: a setup step that parses/validates the command's arguments, and a run step that performs the action and asserts the expected outcome internally. |
| FR-107 | OTT Result Reporting | After executing an on-target test, the firmware shall report PASS or FAIL (with reason on failure) to the serial console, without requiring a debugger to be attached. |

## 3.2 Non-Functional Requirements

| Unique-ID | Name | Description |
|---|---|---|
| NFR-101 | Unit Testability | The Model and Control components shall be unit-testable with Ceedling/Unity without requiring target hardware or FreeRTOS. |
| NFR-102 | Coding Standard Compliance | All C source code shall conform to the [c-code-style](https://github.com/MaxLell/c-code-style) coding standard. |
| NFR-103 | No Runtime Heap Allocation | The message bus implementation shall not perform dynamic heap allocation at runtime on target. |
| NFR-104 | OTT Harness Compatibility | Every on-target test classified as automatable in [[06-Verification-and-Validation]] shall be triggerable via the OTT CLI (FR-106) and have its PASS/FAIL result parseable by an external test-runner script, without manual intervention beyond flashing. |

## 3.3 Constraints

| Unique-ID | Name | Description |
|---|---|---|
| CON-101 | Language | The firmware and game logic shall be written in C. |
| CON-102 | Test Framework | Unit tests shall run under Ceedling/Unity. |
| CON-103 | Host View Library | The host build's View shall use SDL. |
| CON-104 | RTOS | The target firmware shall run on FreeRTOS. |

## 3.4 MVP Architecture

- **Model** — owns the entire game state: maze layout, Pacman position/direction, score, lives remaining, high score (in-memory copy of the NVM value). Pure data plus small accessor functions; no I/O.
- **View** — takes a read-only snapshot of the Model and renders it. Two implementations behind the same interface:
  - Target: draws to the LCD Mono Click over SPI.
  - Host: draws to an SDL window (per CON-103 / FR-104).
- **Control** — the game rules: given the current Model and an input event (or a tick), produces the next Model state. Implemented as pure functions (per FR-102) so it is trivially unit-testable (NFR-101) and identical on host and target.

## 3.5 Pub-Sub Message Bus

A small custom message bus (no external library), consistent with [the coding standard](https://github.com/MaxLell/c-code-style) and NFR-103:

- Topics are a fixed, compile-time `enum` (e.g. `MSG_INPUT_DIRECTION`, `MSG_INPUT_BUTTON`, `MSG_GAME_STATE_CHANGED`, `MSG_GAME_OVER`, `MSG_GAME_SCORE_UPDATED`, `MSG_SYSTEM_SHOW_LOADING`, `MSG_SYSTEM_SHOW_MENU`, `MSG_SYSTEM_START_GAME`, `MSG_PERSISTENCE_HIGHSCORE_LOADED`).
- Each message carries a topic ID and a small fixed-size payload (union of per-topic structs) — no dynamic allocation (NFR-103).
- A static subscription table (built at compile time) maps each topic to the list of subscriber queues.
- **Target**: `publish()` posts the message to each subscriber's FreeRTOS queue (`xQueueSend`, non-blocking or short timeout; a full queue is logged and the message dropped rather than blocking the publisher).
- **Host**: the same `publish()`/`subscribe()` API is backed by a plain in-process dispatch loop (no FreeRTOS dependency), so Model/Control code and any module using the bus is identical on both platforms (FR-104).

## 3.6 FreeRTOS Task Breakdown

| Task | Responsibility | Subscribes to | Publishes |
|---|---|---|---|
| `InputTask` | Polls/reacts to Touchpad Click (I2C) and the user button (GPIO/EXTI); debounces and classifies gestures. | — | `MSG_INPUT_DIRECTION`, `MSG_INPUT_BUTTON` |
| `SystemTask` | Orchestrates boot sequence and screen state (loading → menu → game → game over → menu). | `MSG_INPUT_BUTTON`, `MSG_GAME_OVER`, `MSG_PERSISTENCE_HIGHSCORE_LOADED` | `MSG_SYSTEM_SHOW_LOADING`, `MSG_SYSTEM_SHOW_MENU`, `MSG_SYSTEM_START_GAME` |
| `GameLogicTask` | Owns the Model; runs Control on each tick and on input events. | `MSG_INPUT_DIRECTION`, `MSG_SYSTEM_START_GAME` | `MSG_GAME_STATE_CHANGED`, `MSG_GAME_OVER`, `MSG_GAME_SCORE_UPDATED` |
| `RenderTask` | Draws the current screen (loading/menu/game) to the LCD Mono Click via the View. | `MSG_SYSTEM_SHOW_LOADING`, `MSG_SYSTEM_SHOW_MENU`, `MSG_GAME_STATE_CHANGED` | — |
| `PersistenceTask` | Reads the high score from NVM at boot; writes it back only when it changes (NFR-004 / NFR-103). | `MSG_GAME_SCORE_UPDATED`, `MSG_GAME_OVER` | `MSG_PERSISTENCE_HIGHSCORE_LOADED` |
| `ConsoleTask` | Serves the serial-console CLI: general log output plus OTT on-target test commands (FR-106/FR-107). | — | *(varies — an OTT command may publish/inject any topic to drive the test it's checking)* |

On the host build, these same responsibilities run as plain functions/loops driven by the SDL event loop instead of FreeRTOS tasks — see [[04-Implementation-Phases-and-Milestones|Milestone 3.4]].

## 3.7 On-Target Test (OTT) CLI Framework

FR-106/FR-107 follow the OTT pattern used in the project owner's [BareMetalHollowClockFw](https://github.com/MaxLell/BareMetalHollowClockFw) project (`Test/Target/ott.c`), adapted from J-Link RTT to this project's STLINK V3 serial console:

- Each testable capability (e.g. display, touchpad, LED/blinky) gets its own OTT command, e.g. `ott <name> <args...>`, implemented as a `<name>_setup()` (argument parsing/validation) and a `<name>_run()` (performs the action, asserts the outcome internally).
- A test reports `OTT PASSED [<name>]` or `OTT FAILED [<name>]: <reason>` on the serial console — no debugger required to read the result.
- Tests whose pass/fail cannot be determined by firmware alone (e.g. "does the display visibly show X") are still exposed as OTT commands, but require a human to confirm the outcome — see [[06-Verification-and-Validation]] for which tests fall into this category and how confirmation is signalled (typically: display the expected result, then wait for the user button as confirmation, failing on timeout).
- An external Python script drives the automatable subset of these commands sequentially over the serial console and collects PASS/FAIL results, acting as a repeatable regression harness. Building this script is scoped to [[04-Implementation-Phases-and-Milestones|Board Bring-Up]], once the CLI itself exists on target; it is out of scope for Pre-Planning.
