---
title: System Requirements
---

# 2 System Requirements

[[Index|← Back to Index]] · See also [[01-System-Overview-and-Context]]

Requirements below use the [EARS](https://alistairmavin.com/ears/) notation (Ubiquitous / Event-driven / State-driven / Unwanted-behaviour / Optional-feature templates).

## 2.1 Functional Requirements

Requirements are grouped by feature area. IDs stay globally unique across groups.

### 2.1.1 Boot & Menu

| Unique-ID | Name | Description |
|---|---|---|
| FR-001 | Loading Screen | Upon power-on, the system shall display a loading screen containing the Pacman logo. *(timing — see NFR-005)* |
| FR-002 | High Score Menu | After the loading screen completes, the system shall display a menu screen showing the current high score. |
| FR-003 | Game Start | While the menu screen is displayed, when the user presses the Nucleo board user button, the system shall start a new Pacman game. |

### 2.1.2 Gameplay

| Unique-ID | Name | Description |
|---|---|---|
| FR-004 | Directional Control | While a game is in progress, when the user touches one of the four quadrants of an imaginary "Game-Control-Cross" overlaid on the touchpad surface (north/south/east/west of center), the system shall move Pacman in the corresponding direction. |
| FR-005 | Game Rendering | While a game is in progress, the system shall render the current game state on the monochrome display. |
| FR-006 | Single Life | The system shall start each game with exactly one Pacman life. |
| FR-007 | Game Over | When Pacman's remaining lives reach zero, the system shall end the game and return to the menu screen. |

### 2.1.3 High Score & Persistence

| Unique-ID | Name | Description |
|---|---|---|
| FR-008 | High Score Update | If the final score of a completed game exceeds the stored high score, then the system shall store the new value as the high score in NVM. |
| FR-009 | High Score Persistence | The system shall retain the high score value in NVM across power cycles. |

FR-004 is based on raw touch position, not the Touchpad Click's built-in gesture-detection API — see [[05-Risks-Assumptions-and-Dependencies|R-003]]. The touch surface is divided into four quadrants around its center point; a touch landing in a quadrant maps directly to that quadrant's direction. Exact quadrant boundaries (e.g. dead-zone size near the center) are to be tuned during Board Bring-Up.

## 2.2 Non-Functional Requirements

| Unique-ID | Name | Description |
|---|---|---|
| NFR-001 | Loading Screen Duration | The loading screen shall be displayed for no more than 3 seconds before the menu is shown. *(default value — see [[05-Risks-Assumptions-and-Dependencies\|A-001]])* |
| NFR-002 | Rendering Rate | While a game is in progress, the system shall refresh the display at a minimum of 30 frames per second. *(default value — see [[05-Risks-Assumptions-and-Dependencies\|A-002]])* |
| NFR-003 | Input Latency | When a touch gesture occurs, the system shall reflect the corresponding movement on the display within 30 ms. *(default value — see [[05-Risks-Assumptions-and-Dependencies\|A-003]])* |
| NFR-004 | NVM Write Frequency | The system shall write to NVM only when the high score changes, in order to minimize flash wear. |
| NFR-005 | Logo Display Delay | Upon power-on, the system shall wait 200 ms before displaying the Pacman logo of the loading screen (FR-001). |

## 2.3 Constraints

| Unique-ID | Name | Description |
|---|---|---|
| CON-001 | Target Hardware | The system shall run on the STM32G431RB Nucleo-64 board. |
| CON-002 | Display Hardware | The system shall use the LCD Mono Click (Sharp LS013B7DH03, 128×128 monochrome memory LCD) as its display. |
| CON-003 | Input Hardware | The system shall use the Touchpad Click (Microchip MTCH6102 capacitive touch controller) as its directional input device. |
| CON-004 | Carrier Hardware | The system shall use the MikroE Click Shield for Nucleo-64 to connect the display (mikroBUS slot 1) and touchpad (mikroBUS slot 2) to the Nucleo board. *(exact pin mapping pending — see [[05-Risks-Assumptions-and-Dependencies\|R-001]])* |
| CON-005 | Debug Interface | The system shall use the on-board STLINK V3 for debugging (SWD) and serial console output. |
