# 6 Verification & Validation

[← Back to Index](Index.md)

Each test below has a unique ID so it can be cross-referenced against requirements in [07 Traceability Matrix](07-Traceability-Matrix.md). Tests fall into exactly two categories: **Unit Tests** (small scope) and **Integration Tests** (large scope). There is no separate "hardware"/"smoke"/"acceptance" split — those distinctions are captured instead by each Integration Test's **Scope** (Host/Target) and **Automation** columns.

## 6.1 Unit Tests (small scope)

Host-side, run under Ceedling/Unity, no target hardware required.

| ID | Name | Description |
|---|---|---|
| VT-UNIT-001 | Message Broker Publish/Subscribe | A message published into the broker input queue is fanned out to every subscriber of its topic and to no subscriber of another topic (FR-103, FR-108, FR-110); a full subscriber output queue does not stall the broker, and a publisher can detect a full input queue via the backpressure API instead of blocking (NFR-105). |
| VT-UNIT-002 | Model Unit Tests | Model initialization and state-update accessor functions behave correctly. |
| VT-UNIT-003 | Control Statelessness | Calling a Control function twice with identical Model + input produces identical output both times (no hidden internal state). |
| VT-UNIT-004 | Maze & Movement Rules | Wall cells block movement (FR-010); pellet/power-pellet entry removes the item and increments the score (FR-011, FR-017); exiting a side tunnel wraps to the opposite opening on the same row (FR-012). |
| VT-UNIT-005 | Ghost & Frightened Logic | Each ghost's targeting is deterministic for its behavior (FR-014) and alternates between chase and scatter phases (FR-015); consuming a power pellet activates frightened mode, eating a frightened ghost scores a bonus and returns it to the pen without costing a life (FR-018, FR-019), and frightened mode ends after its duration (FR-020). |
| VT-UNIT-006 | Level & Difficulty Progression | The per-level difficulty parameters follow the defined monotonic progression (ghosts faster, frightened shorter, scatter less — FR-026); clearing a level advances the level and applies the next level's parameters and maze while carrying over score and lives (FR-021, FR-025), and clearing the final level completes the game (FR-027). |

## 6.2 Integration Tests (large scope)

Each on-target test is exposed as its own OTT CLI command (FR-106/FR-107, see [OTT CLI Framework](03-Architecture.md#37-on-target-test-ott-cli-framework)), modeled on the pattern used in [BareMetalHollowClockFw](https://github.com/MaxLell/BareMetalHollowClockFw). **Automation** values:
- **Automatic** — triggerable and verifiable by the external Python test harness with no human involved.
- **Manual, button-confirmed** — the firmware displays/logs the expected result and waits for the user to press the Nucleo user button to confirm; the test fails if the button isn't pressed within a timeout. This replaces plain "look at the screen" inspection with a pass/fail signal the firmware can still report.
- **Manual, one-time bring-up** — a physical procedure (e.g. continuity check with a multimeter/logic analyzer) that isn't repeatable via CLI; done once during Board Bring-Up, not part of the regression harness.

| ID | Name | Scope | Automation | Description |
|---|---|---|---|---|
| VT-INT-001 | Power-On & Enumeration | Host tool + Target | Automatic | The Nucleo board powers on and the STLINK V3 enumerates as a debug + virtual COM port device over USB. |
| VT-INT-002 | Serial Console Output | Target | Automatic | The STLINK V3 virtual COM port produces a known, readable boot banner from the target. |
| VT-INT-003 | Display SPI Pin Mapping | Target | Manual, one-time bring-up | Confirm which STM32G431 GPIOs the mikroBUS slot-1 socket's SCK/MOSI/CS/EXTMODE/EXTCOMIN/DISP signals are wired to for the LCD Mono Click. Closes [R-001](05-Risks-Assumptions-and-Dependencies.md#51-risks). |
| VT-INT-004 | Touchpad I2C Pin Mapping | Target | Manual, one-time bring-up | Confirm which STM32G431 GPIOs the mikroBUS slot-2 socket's SCL/SDA/INT/RST signals are wired to for the Touchpad Click. Closes [R-001](05-Risks-Assumptions-and-Dependencies.md#51-risks). |
| VT-INT-005 | Blinky | Target | Automatic | OTT command toggles the on-board LED at the expected rate and asserts the resulting pin state; confirms the toolchain bring-up. |
| VT-INT-006 | Display Init | Target | Manual, button-confirmed | OTT command clears then fills the LCD Mono Click with a known test pattern; the display prompts "press user button to confirm pattern is visible", failing on timeout. |
| VT-INT-007 | Touchpad Read & Confirm | Target | Manual, button-confirmed | The current X/Y touch coordinates from the Touchpad Click are shown live on the display. The display then prompts the user to press the user button to confirm the reading is correct; if the button isn't pressed within a timeout, the test fails. |
| VT-INT-008 | Host Build Launch | Host | Automatic | The host-built Pacman binary launches, opens an SDL window, and renders at least one frame. |
| VT-INT-009 | FreeRTOS Task Startup | Target | Automatic | OTT command dumps the FreeRTOS task list over the serial console; the harness parses it and confirms every task from [03 Architecture](03-Architecture.md) is present and running/ready. |
| VT-INT-010 | Game Logic Playthrough (E2E) | Host | Automatic | A scripted sequence of directional inputs is fed through the pub-sub bus into Control; the resulting Model state — Pacman position, pellet/power-pellet consumption, score, ghost positions, and frightened mode — matches the expected outcome after each move. |
| VT-INT-011 | Boot Sequence | Target | Automatic | The harness parses serial-logged state-transition messages to confirm the loading screen (with logo, after the NFR-005 delay) is followed by the menu screen (with current high score) within NFR-001. |
| VT-INT-012 | Game Start via Button | Target | Manual, button-confirmed | From the menu screen, the harness prompts the user to press the Nucleo user button; a new game starting is confirmed via the resulting serial log message within a timeout. |
| VT-INT-013 | Directional Movement | Target | Manual, button-confirmed (latency: Automatic) | The user is prompted to touch each Game-Control-Cross quadrant (FR-004) in turn and confirm correct movement via the button; input-to-render latency (NFR-003) is measured automatically from serial-logged input/render timestamps. |
| VT-INT-014 | Game Over (Lives Exhausted) | Host | Automatic | A scripted input sequence causes Pacman to be caught repeatedly; each non-final catch decrements lives and respawns Pacman/ghosts to continue the level (FR-024); after the last life the game ends, the final score is shown (score screen, FR-023), and the Model returns to the menu state. |
| VT-INT-015 | High Score Update & Persistence | Target | Automatic | An OTT command forces a score above the stored high score, triggers a reset, then reads the NVM value back over the CLI to confirm it persisted; a lower score is confirmed not to overwrite the stored value. |
| VT-INT-016 | Rendering Smoothness | Target | Automatic | The harness parses serial-logged frame-render timestamps captured during a scripted gameplay run and confirms the achieved rate meets NFR-002. |
| VT-INT-017 | Level Progression & Completion (E2E) | Host | Automatic | A scripted run clears each level's maze in turn: clearing a non-final level advances to the next maze and difficulty while carrying score and lives (FR-021, FR-025, FR-026); clearing the 5th level completes the game as won (FR-027), shows the score screen (FR-023), and returns to the menu. |

## 6.3 Test Harness (Python)

An external Python script drives every **Automatic** test above sequentially over the serial console (sending the OTT command, reading back PASS/FAIL) and reports a summary. **Manual, button-confirmed** and **Manual, one-time bring-up** tests are excluded from this script's run list — they still have their own OTT command/procedure but require a human in the loop. Building this script is scoped to [Board Bring-Up](04-Implementation-Phases-and-Milestones.md), once the OTT CLI actually exists on target; see [OTT CLI Framework](03-Architecture.md#37-on-target-test-ott-cli-framework) for the design.
