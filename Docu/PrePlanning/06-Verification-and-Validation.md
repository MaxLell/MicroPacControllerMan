---
title: Verification & Validation
---

# 6 Verification & Validation

[[Index|← Back to Index]]

Each test below has a unique ID so it can be cross-referenced against requirements in [[07-Traceability-Matrix]].

## 6.1 Hardware Tests (the hardware itself)

| ID | Name | Description |
|---|---|---|
| VT-HW-001 | Power-On Self Check | Confirm the Nucleo board powers on and the STLINK V3 enumerates as a debug + virtual COM port device over USB. |
| VT-HW-002 | Display SPI Continuity | Confirm, via continuity check or logic analyzer, which STM32G431 GPIOs the mikroBUS slot-1 socket's SCK/MOSI/CS (and EXTMODE/EXTCOMIN/DISP) signals are wired to for the LCD Mono Click. Closes [[05-Risks-Assumptions-and-Dependencies\|R-001]]. |
| VT-HW-003 | Touchpad I2C Continuity | Confirm, via continuity check or logic analyzer, which STM32G431 GPIOs the mikroBUS slot-2 socket's SCL/SDA/INT/RST signals are wired to for the Touchpad Click. Closes [[05-Risks-Assumptions-and-Dependencies\|R-001]]. |
| VT-HW-004 | Serial Console Output | Confirm the STLINK V3 virtual COM port produces readable log output from the target. |

## 6.2 Helper Tests (unit-level)

| ID | Name | Description |
|---|---|---|
| VT-HELP-001 | Message Bus Publish/Subscribe | Unit test: a published message reaches every subscriber of its topic and no subscriber of another topic; a full subscriber queue does not block the publisher. |
| VT-HELP-002 | Model Unit Tests | Unit tests covering Model initialization and state-update accessor functions run under Ceedling/Unity. |
| VT-HELP-003 | Control Statelessness | Unit test: calling a Control function twice with identical Model + input produces identical output both times (no hidden internal state). |

## 6.3 Smoke Tests

| ID | Name | Description |
|---|---|---|
| VT-SMOKE-001 | Blinky | After flashing, an on-board LED toggles at the expected rate; confirms the toolchain bring-up. |
| VT-SMOKE-002 | Display Init | On boot, the LCD Mono Click clears and then fills the screen with a known test pattern. |
| VT-SMOKE-003 | Touchpad Read | A raw touch coordinate or gesture event from the Touchpad Click is read and logged over the serial console. |
| VT-SMOKE-004 | Host Build Launch | The host-built Pacman binary launches, opens an SDL window, and renders at least one frame. |
| VT-SMOKE-005 | FreeRTOS Task Startup | All FreeRTOS tasks defined in [[03-Software-Requirements-and-Architecture]] are created and reach the running/ready state, confirmed via a task-list dump over the serial console. |

## 6.4 Acceptance Tests (end-to-end, user-facing)

| ID | Name | Description |
|---|---|---|
| VT-ACC-001 | Boot Sequence | Powering on the board shows the loading screen (with logo) followed by the menu screen showing the current high score, within the specified duration. |
| VT-ACC-002 | Game Start via Button | From the menu screen, pressing the Nucleo user button starts a new game. |
| VT-ACC-003 | Directional Movement | Touchpad gestures move Pacman correctly in all four directions with the specified latency. |
| VT-ACC-004 | Single-Life Game Over | The game ends immediately after Pacman is caught once (no extra lives) and returns to the menu screen. |
| VT-ACC-005 | High Score Update & Persistence | Achieving a new high score updates the stored value, which survives a power cycle; a lower score does not overwrite the stored high score. |
| VT-ACC-006 | Rendering Smoothness | During gameplay, the display refresh rate meets the specified minimum frame rate. |

## 6.5 Related Documents

- [[07-Traceability-Matrix]] — every requirement mapped to the test(s) above.
- [[05-Risks-Assumptions-and-Dependencies]] — risks these tests are designed to close out.
