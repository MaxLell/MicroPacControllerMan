# 2 Requirements

[← Back to Index](Index.md) · See also [01 System Overview & Context](01-System-Overview-and-Context.md)

Requirements use the [EARS](https://alistairmavin.com/ears/) notation (Ubiquitous / Event-driven / State-driven / Unwanted-behaviour / Optional-feature templates) and are grouped by feature/concern. This project does **not** separate "system" from "software" requirements — they all live here together. The `FR-0xx` / `FR-1xx` (and `NFR` / `CON`) numbers are stable identifiers only; the numbering carries no system-vs-software meaning.

## 2.1 Functional Requirements

### 2.1.1 Boot & Menu

| Unique-ID | Name | Description |
|---|---|---|
| FR-001 | Loading Screen | Upon power-on, the system shall display a loading screen containing the Pacman logo. *(timing — see NFR-005)* |
| FR-002 | High Score Menu | After the loading screen completes, the system shall display a menu screen showing the current high score. |
| FR-003 | Game Start | While the menu screen is displayed, when the user presses the Nucleo board user button, the system shall start a new Pacman game. |

### 2.1.2 Player Control & Rendering

| Unique-ID | Name | Description |
|---|---|---|
| FR-004 | Directional Control | While a game is in progress, when the user touches one of the four quadrants of an imaginary "Game-Control-Cross" overlaid on the touchpad surface (north/south/east/west of center), the system shall move Pacman in the corresponding direction. |
| FR-005 | Game Rendering | While a game is in progress, the system shall render the current game state on the monochrome display. |

FR-004 is based on raw touch position, not the Touchpad Click's built-in gesture-detection API — see [R-003](05-Risks-Assumptions-and-Dependencies.md#51-risks). The touch surface is divided into four quadrants around its center point; a touch landing in a quadrant maps directly to that quadrant's direction. Exact quadrant boundaries (e.g. dead-zone size near the center) are to be tuned during Board Bring-Up.

### 2.1.3 Maze, Pellets & Movement

| Unique-ID | Name | Description |
|---|---|---|
| FR-010 | Maze Confinement | While a game is in progress, the system shall confine Pacman and the ghosts to the open paths of a single fixed maze, preventing any movement through walls. |
| FR-011 | Pellet Consumption | While a game is in progress, when Pacman enters a maze cell containing a pellet, the system shall remove that pellet and add its point value to the current score. |
| FR-012 | Tunnel Wrap-Around | While a game is in progress, when Pacman or a ghost exits the maze through a side tunnel opening, the system shall re-enter it through the opposite tunnel opening on the same row. |
| FR-022 | Playfield Size | The maze shall be a reduced, display-fit layout with a fixed grid size that fits within the 128×128 display at a legible tile size. *(reduced maze decided — see [R-008](05-Risks-Assumptions-and-Dependencies.md#51-risks); exact dimensions set during Pacman Development)* |

### 2.1.4 Ghosts

| Unique-ID | Name | Description |
|---|---|---|
| FR-013 | Ghost Presence | While a game is in progress, the system shall move four ghosts through the maze concurrently with Pacman. |
| FR-014 | Distinct Ghost Behaviors | While a game is in progress and frightened mode (FR-018) is not active, the system shall drive each of the four ghosts by its own distinct targeting behavior, in the style of the classic Blinky/Pinky/Inky/Clyde ghosts. |
| FR-015 | Scatter / Chase Alternation | While a game is in progress, the system shall periodically alternate the ghosts between a chase phase (pursuing their individual targets) and a scatter phase (retreating toward fixed maze corners). |
| FR-016 | Ghost Collision | While a game is in progress and frightened mode (FR-018) is not active, when a ghost occupies the same maze cell as Pacman, the system shall treat Pacman as caught and reduce the remaining lives (see FR-006 / FR-007). |

### 2.1.5 Power Pellets & Frightened Mode

| Unique-ID | Name | Description |
|---|---|---|
| FR-017 | Power Pellet Consumption | While a game is in progress, when Pacman enters a maze cell containing a power pellet, the system shall remove it, add its point value to the score, and activate frightened mode. |
| FR-018 | Frightened Ghosts | While frightened mode is active, the system shall render all ghosts in a visibly frightened state and make them flee from Pacman. |
| FR-019 | Eating Frightened Ghosts | While frightened mode is active, when Pacman occupies the same maze cell as a frightened ghost, the system shall add a ghost-eaten bonus to the score and return that ghost to the ghost pen to regenerate, without reducing Pacman's lives. |
| FR-020 | Frightened Timeout | While frightened mode is active, when its bounded duration elapses, the system shall end frightened mode and return all ghosts to their normal behavior. |

### 2.1.6 Lives, Game Over & Level Clear

| Unique-ID | Name | Description |
|---|---|---|
| FR-006 | Single Life | The system shall start each game with exactly one Pacman life. |
| FR-007 | Game Over | When Pacman's remaining lives reach zero, the system shall end the game and return to the menu screen. |
| FR-021 | Level Clear | While a game is in progress, when Pacman has consumed the last remaining pellet and power pellet in the maze, the system shall end the game as completed (won) and return to the menu screen, with the final score eligible for the high score per FR-008. |

### 2.1.7 High Score & Persistence

| Unique-ID | Name | Description |
|---|---|---|
| FR-008 | High Score Update | If the final score of a completed game exceeds the stored high score, then the system shall store the new value as the high score in NVM. |
| FR-009 | High Score Persistence | The system shall retain the high score value in NVM across power cycles. |

The concrete rules and values that realise these requirements — the maze layout, movement/tick model, ghost targeting algorithms, power-pellet/frightened behaviour, scoring and end conditions — are specified in [10 Pacman Game Design](10-Pacman-Game-Design.md). The numeric constants there remain tunable for game feel (see [A-006](05-Risks-Assumptions-and-Dependencies.md#52-assumptions)).

### 2.1.8 Software Structure & Messaging

See [03 Architecture](03-Architecture.md) for how these are realized.

| Unique-ID | Name | Description |
|---|---|---|
| FR-101 | MVP Separation | The software shall implement the game using Model-View-Control (MVP) architecture, separating game state (Model), rendering (View), and game logic (Control) into distinct components. |
| FR-102 | Stateless Control | The Control component shall not persist game state between invocations; all game state shall reside exclusively in the Model. |
| FR-103 | Message-Based Communication | Firmware modules shall exchange data exclusively through published/subscribed messages on the message bus, not through direct cross-module function calls. |
| FR-104 | Host Buildability | The Model and Control components shall be buildable and executable unmodified on the host computer, using SDL to implement the View. |
| FR-105 | FreeRTOS Task Separation | The firmware shall execute input handling, rendering, game-logic ticking, and persistence as separate FreeRTOS tasks. |
| FR-108 | Dedicated Message-Broker Task | The firmware shall route inter-module messages through a single dedicated message-broker task whose sole responsibility is to move each published message from the broker's input queue into the output queue of every module subscribed to that message's topic. |
| FR-109 | Active-Object Modules | Each firmware module shall be structured as an active object: exactly one owning FreeRTOS task, a single inbound message queue as its only external input, run-to-completion handling of each received message, and no application state shared directly with other modules. |
| FR-110 | Pacman Internal Message Bus | The Pacman application shall exchange messages between its Model, View, and Control modules over its own internal message-bus instance, separate from the system-level message bus. |

### 2.1.9 On-Target Test (OTT) Framework

| Unique-ID | Name | Description |
|---|---|---|
| FR-106 | On-Target Test (OTT) Framework | The firmware shall expose each on-target test named in [06 Verification & Validation](06-Verification-and-Validation.md) as an individual command over the serial console CLI, following an OTT pattern: a setup step that parses/validates the command's arguments, and a run step that performs the action and asserts the expected outcome internally. See the [OTT CLI Framework](03-Architecture.md#37-on-target-test-ott-cli-framework) for the mechanism. |
| FR-107 | OTT Result Reporting | After executing an on-target test, the firmware shall report PASS or FAIL (with reason on failure) to the serial console, without requiring a debugger to be attached. |

### 2.1.10 Error Handling

| Unique-ID | Name | Description |
|---|---|---|
| FR-111 | Fatal-Error Halt | When the firmware detects a fatal, unrecoverable error, its error handler shall halt execution at a debugger breakpoint (`bkpt`) so the fault can be inspected. *(minimal handling for now; may be extended in later phases)* |

## 2.2 Non-Functional Requirements

### 2.2.1 Timing & Performance

| Unique-ID | Name | Description |
|---|---|---|
| NFR-001 | Loading Screen Duration | The loading screen shall be displayed for no more than 3 seconds before the menu is shown. *(default value — see [A-001](05-Risks-Assumptions-and-Dependencies.md#52-assumptions))* |
| NFR-002 | Rendering Rate | While a game is in progress, the system shall refresh the display at a minimum of 30 frames per second. *(default value — see [A-002](05-Risks-Assumptions-and-Dependencies.md#52-assumptions))* |
| NFR-003 | Input Latency | When a touch gesture occurs, the system shall reflect the corresponding movement on the display within 30 ms. *(default value — see [A-003](05-Risks-Assumptions-and-Dependencies.md#52-assumptions))* |
| NFR-005 | Logo Display Delay | Upon power-on, the system shall wait 200 ms before displaying the Pacman logo of the loading screen (FR-001). |

### 2.2.2 Persistence

| Unique-ID | Name | Description |
|---|---|---|
| NFR-004 | NVM Write Frequency | The system shall write to NVM only when the high score changes, in order to minimize flash wear. |

### 2.2.3 Quality & Testability

| Unique-ID | Name | Description |
|---|---|---|
| NFR-101 | Unit Testability | The Model and Control components shall be unit-testable with Ceedling/Unity without requiring target hardware or FreeRTOS. |
| NFR-102 | Coding Standard Compliance | All C source code shall conform to the [c-code-style](https://github.com/MaxLell/c-code-style) coding standard. |
| NFR-103 | No Runtime Heap Allocation | The message bus implementation shall not perform dynamic heap allocation at runtime on target. |
| NFR-105 | Message-Bus Backpressure | The message broker shall provide an API for a publisher to query the free capacity of the broker input queue, and a publish attempt on a full queue shall return a status (bounded, non-blocking) rather than blocking the publisher indefinitely. |

### 2.2.4 Test Harness

| Unique-ID | Name | Description |
|---|---|---|
| NFR-104 | OTT Harness Compatibility | Every on-target test classified as automatable in [06 Verification & Validation](06-Verification-and-Validation.md) shall be triggerable via the OTT CLI (FR-106) and have its PASS/FAIL result parseable by an external test-runner script, without manual intervention beyond flashing. |

## 2.3 Constraints

### 2.3.1 Hardware

| Unique-ID | Name | Description |
|---|---|---|
| CON-001 | Target Hardware | The system shall run on the STM32G431RB Nucleo-64 board. |
| CON-002 | Display Hardware | The system shall use the LCD Mono Click (Sharp LS013B7DH03, 128×128 monochrome memory LCD) as its display. |
| CON-003 | Input Hardware | The system shall use the Touchpad Click (Microchip MTCH6102 capacitive touch controller) as its directional input device. |
| CON-004 | Carrier Hardware | The system shall use the MikroE Click Shield for Nucleo-64 to connect the display (mikroBUS slot 1) and touchpad (mikroBUS slot 2) to the Nucleo board. *(exact pin mapping pending — see [R-001](05-Risks-Assumptions-and-Dependencies.md#51-risks))* |
| CON-005 | Debug Interface | The system shall use the on-board STLINK V3 for debugging (SWD) and serial console output. |

### 2.3.2 Software & Toolchain

| Unique-ID | Name | Description |
|---|---|---|
| CON-101 | Language | The firmware and game logic shall be written in C. |
| CON-102 | Test Framework | Unit tests shall run under Ceedling/Unity. |
| CON-103 | Host View Library | The host build's View shall use SDL. |
| CON-104 | RTOS | The target firmware shall run on FreeRTOS. |
