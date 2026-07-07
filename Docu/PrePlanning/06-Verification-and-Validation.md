---
title: Verification & Validation
---

# 6 Verification & Validation

[[Index|← Back to Index]]

Each test below has a unique ID so it can be cross-referenced against requirements in [[07-Traceability-Matrix]]. Tests fall into exactly two categories: **Unit Tests** (small scope) and **Integration Tests** (large scope). There is no separate "hardware"/"smoke"/"acceptance" split — those distinctions are captured instead by each Integration Test's **Scope** (Host/Target) and **Automation** columns.

## 6.1 Unit Tests (small scope)

Host-side, run under Ceedling/Unity, no target hardware required.

| ID | Name | Description |
|---|---|---|
| VT-UNIT-001 | Message Bus Publish/Subscribe | A published message reaches every subscriber of its topic and no subscriber of another topic; a full subscriber queue does not block the publisher. |
| VT-UNIT-002 | Model Unit Tests | Model initialization and state-update accessor functions behave correctly. |
| VT-UNIT-003 | Control Statelessness | Calling a Control function twice with identical Model + input produces identical output both times (no hidden internal state). |
| VT-UNIT-004 | Maze & Movement Rules | Wall cells block movement (FR-010); pellet/power-pellet entry removes the item and increments the score (FR-011, FR-017); exiting a side tunnel wraps to the opposite opening on the same row (FR-012). |
| VT-UNIT-005 | Ghost & Frightened Logic | Each ghost's targeting is deterministic for its behavior (FR-014) and alternates between chase and scatter phases (FR-015); consuming a power pellet activates frightened mode, eating a frightened ghost scores a bonus and returns it to the pen without costing a life (FR-018, FR-019), and frightened mode ends after its duration (FR-020). |

## 6.2 Integration Tests (large scope)

Each on-target test is exposed as its own OTT CLI command (FR-106/FR-107, see [[03-Software-Requirements-and-Architecture#3.7 On-Target Test (OTT) CLI Framework|OTT CLI Framework]]), modeled on the pattern used in [BareMetalHollowClockFw](https://github.com/MaxLell/BareMetalHollowClockFw). **Automation** values:
- **Automatic** — triggerable and verifiable by the external Python test harness with no human involved.
- **Manual, button-confirmed** — the firmware displays/logs the expected result and waits for the user to press the Nucleo user button to confirm; the test fails if the button isn't pressed within a timeout. This replaces plain "look at the screen" inspection with a pass/fail signal the firmware can still report.
- **Manual, one-time bring-up** — a physical procedure (e.g. continuity check with a multimeter/logic analyzer) that isn't repeatable via CLI; done once during Board Bring-Up, not part of the regression harness.

| ID | Name | Scope | Automation | Description |
|---|---|---|---|---|
| VT-INT-001 | Power-On & Enumeration | Host tool + Target | Automatic | The Nucleo board powers on and the STLINK V3 enumerates as a debug + virtual COM port device over USB. |
| VT-INT-002 | Serial Console Output | Target | Automatic | The STLINK V3 virtual COM port produces a known, readable boot banner from the target. |
| VT-INT-003 | Display SPI Pin Mapping | Target | Manual, one-time bring-up | Confirm which STM32G431 GPIOs the mikroBUS slot-1 socket's SCK/MOSI/CS/EXTMODE/EXTCOMIN/DISP signals are wired to for the LCD Mono Click. Closes [[05-Risks-Assumptions-and-Dependencies\|R-001]]. |
| VT-INT-004 | Touchpad I2C Pin Mapping | Target | Manual, one-time bring-up | Confirm which STM32G431 GPIOs the mikroBUS slot-2 socket's SCL/SDA/INT/RST signals are wired to for the Touchpad Click. Closes [[05-Risks-Assumptions-and-Dependencies\|R-001]]. |
| VT-INT-005 | Blinky | Target | Automatic | OTT command toggles the on-board LED at the expected rate and asserts the resulting pin state; confirms the toolchain bring-up. |
| VT-INT-006 | Display Init | Target | Manual, button-confirmed | OTT command clears then fills the LCD Mono Click with a known test pattern; the display prompts "press user button to confirm pattern is visible", failing on timeout. |
| VT-INT-007 | Touchpad Read & Confirm | Target | Manual, button-confirmed | The current X/Y touch coordinates from the Touchpad Click are shown live on the display. The display then prompts the user to press the user button to confirm the reading is correct; if the button isn't pressed within a timeout, the test fails. |
| VT-INT-008 | Host Build Launch | Host | Automatic | The host-built Pacman binary launches, opens an SDL window, and renders at least one frame. |
| VT-INT-009 | FreeRTOS Task Startup | Target | Automatic | OTT command dumps the FreeRTOS task list over the serial console; the harness parses it and confirms every task from [[03-Software-Requirements-and-Architecture]] is present and running/ready. |
| VT-INT-010 | Game Logic Playthrough (E2E) | Host | Automatic | A scripted sequence of directional inputs is fed through the pub-sub bus into Control; the resulting Model state — Pacman position, pellet/power-pellet consumption, score, ghost positions, and frightened mode — matches the expected outcome after each move. |
| VT-INT-011 | Boot Sequence | Target | Automatic | The harness parses serial-logged state-transition messages to confirm the loading screen (with logo, after the NFR-005 delay) is followed by the menu screen (with current high score) within NFR-001. |
| VT-INT-012 | Game Start via Button | Target | Manual, button-confirmed | From the menu screen, the harness prompts the user to press the Nucleo user button; a new game starting is confirmed via the resulting serial log message within a timeout. |
| VT-INT-013 | Directional Movement | Target | Manual, button-confirmed (latency: Automatic) | The user is prompted to touch each Game-Control-Cross quadrant (FR-004) in turn and confirm correct movement via the button; input-to-render latency (NFR-003) is measured automatically from serial-logged input/render timestamps. |
| VT-INT-014 | Single-Life Game Over | Host | Automatic | A scripted input sequence causes Pacman to be caught once; the game ends immediately (no extra lives) and the Model returns to the menu state. |
| VT-INT-015 | High Score Update & Persistence | Target | Automatic | An OTT command forces a score above the stored high score, triggers a reset, then reads the NVM value back over the CLI to confirm it persisted; a lower score is confirmed not to overwrite the stored value. |
| VT-INT-016 | Rendering Smoothness | Target | Automatic | The harness parses serial-logged frame-render timestamps captured during a scripted gameplay run and confirms the achieved rate meets NFR-002. |
| VT-INT-017 | Level Clear (E2E) | Host | Automatic | A scripted input sequence consumes every pellet and power pellet in the maze; the game then ends as won (FR-021) and the Model returns to the menu state, with the final score offered for high-score comparison. |

## 6.3 Test Harness (Python)

An external Python script drives every **Automatic** test above sequentially over the serial console (sending the OTT command, reading back PASS/FAIL) and reports a summary. **Manual, button-confirmed** and **Manual, one-time bring-up** tests are excluded from this script's run list — they still have their own OTT command/procedure but require a human in the loop. Building this script is scoped to [[04-Implementation-Phases-and-Milestones|Board Bring-Up]], once the OTT CLI actually exists on target; see [[03-Software-Requirements-and-Architecture#3.7 On-Target Test (OTT) CLI Framework|OTT CLI Framework]] for the design.
