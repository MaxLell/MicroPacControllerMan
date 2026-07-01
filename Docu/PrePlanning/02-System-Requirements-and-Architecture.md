---
title: System Requirements & System Architecture
---

# 2 System Requirements & System Architecture

[[Index|← Back to Index]] · See also [[01-System-Overview-and-Context]]

Requirements below use the [EARS](https://alistairmavin.com/ears/) notation (Ubiquitous / Event-driven / State-driven / Unwanted-behaviour / Optional-feature templates).

## 2.1 Functional Requirements

| Unique-ID | Name | Description |
|---|---|---|
| FR-001 | Loading Screen | Upon power-on, the system shall display a loading screen containing the Pacman logo. |
| FR-002 | High Score Menu | After the loading screen completes, the system shall display a menu screen showing the current high score. |
| FR-003 | Game Start | While the menu screen is displayed, when the user presses the Nucleo board user button, the system shall start a new Pacman game. |
| FR-004 | Directional Control | While a game is in progress, when the user performs a directional gesture on the touchpad, the system shall move Pacman in the corresponding direction. |
| FR-005 | Game Rendering | While a game is in progress, the system shall render the current game state on the monochrome display. |
| FR-006 | Single Life | The system shall start each game with exactly one Pacman life. |
| FR-007 | Game Over | When Pacman's remaining lives reach zero, the system shall end the game and return to the menu screen. |
| FR-008 | High Score Update | If the final score of a completed game exceeds the stored high score, then the system shall store the new value as the high score in NVM. |
| FR-009 | High Score Persistence | The system shall retain the high score value in NVM across power cycles. |

## 2.2 Non-Functional Requirements

| Unique-ID | Name | Description |
|---|---|---|
| NFR-001 | Loading Screen Duration | The loading screen shall be displayed for no more than 3 seconds before the menu is shown. *(default value — see [[05-Risks-Assumptions-and-Dependencies\|A-001]])* |
| NFR-002 | Rendering Rate | While a game is in progress, the system shall refresh the display at a minimum of 10 frames per second. *(default value — see [[05-Risks-Assumptions-and-Dependencies\|A-002]])* |
| NFR-003 | Input Latency | When a touch gesture occurs, the system shall reflect the corresponding movement on the display within 100 ms. *(default value — see [[05-Risks-Assumptions-and-Dependencies\|A-003]])* |
| NFR-004 | NVM Write Frequency | The system shall write to NVM only when the high score changes, in order to minimize flash wear. |

## 2.3 Constraints

| Unique-ID | Name | Description |
|---|---|---|
| CON-001 | Target Hardware | The system shall run on the STM32G431RB Nucleo-64 board. |
| CON-002 | Display Hardware | The system shall use the LCD Mono Click (Sharp LS013B7DH03, 128×128 monochrome memory LCD) as its display. |
| CON-003 | Input Hardware | The system shall use the Touchpad Click (Microchip MTCH6102 capacitive touch controller) as its directional input device. |
| CON-004 | Carrier Hardware | The system shall use the MikroE Click Shield for Nucleo-64 to connect the display (mikroBUS slot 1) and touchpad (mikroBUS slot 2) to the Nucleo board. |
| CON-005 | Debug Interface | The system shall use the on-board STLINK V3 for debugging (SWD) and serial console output. |

## 2.4 System Architecture

```
+-----------------------------------------------------------------+
|                     STM32G431RB Nucleo-64 + Shield               |
|                                                                   |
|  Touchpad Click ---I2C(SCL/SDA)+INT/RST---+                      |
|  (MTCH6102, slot 2)                       |                      |
|                                            v                      |
|                                    +----------------+            |
|  User Button ---GPIO------------->|   Firmware      |            |
|                                    |  (see doc 3 for |            |
|  STLINK V3 <---SWD + UART--------->|  MVP/pub-sub/   |            |
|                                    |  FreeRTOS tasks)|            |
|                                    +--------+-------+            |
|                                             |                     |
|                                             v                     |
|  LCD Mono Click <---3-wire SPI (SCK/MOSI/CS)+EXTMODE/EXTCOMIN/DISP|
|  (LS013B7DH03, slot 1)                                            |
|                                                                    |
|  NVM (on-chip flash) <---read/write high score--------------------|
+--------------------------------------------------------------------+
```

Exact mikroBUS-socket-to-MCU-GPIO pin assignments are **not yet confirmed** from public documentation alone — see [[05-Risks-Assumptions-and-Dependencies|R-001]]. This diagram documents interface *types* (SPI, I2C, GPIO), not final pin names; those will be confirmed during [[04-Implementation-Phases-and-Milestones|Board Bring-Up]] and recorded there once verified.

## 2.5 Related Documents

- Software-level breakdown of how the firmware satisfies these system requirements: [[03-Software-Requirements-and-Architecture]]
- Verification of these requirements: [[06-Verification-and-Validation]], cross-referenced in [[07-Traceability-Matrix]]
