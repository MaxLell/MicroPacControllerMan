# 2 Requirements

[← Back to Index](Index.md) · See also [01 System Overview & Context](01-System-Overview-and-Context.md)

Requirements use the [EARS](https://alistairmavin.com/ears/) notation (Ubiquitous / Event-driven / State-driven / Unwanted-behaviour / Optional-feature templates) and are grouped by feature/concern. This project does *not* separate "system" from "software" requirements — they all live here together. The `FR-0xx` / `FR-1xx` (and `NFR` / `CON`) numbers are stable identifiers only; the numbering carries no system-vs-software meaning.

## 2.1 Functional Requirements

### 2.1.1 Boot & Menu

| Unique-ID | Name | Description |
|---|---|---|
| FR-001 | Loading Screen | Upon power-on, the system shall display a loading screen containing the Pacman logo. *(timing — see NFR-005)* |
| FR-002 | High Score Menu | After the loading screen completes, the system shall display a menu screen showing the games that can be started (FR-040) and the high scores of the selected one (FR-041), and nothing else. *(Amended by [DEC-045](11-Decisions-and-As-Built.md): it used to carry the title and the row of actors as well, which the loading screen has just shown. Amended by [DEC-046](11-Decisions-and-As-Built.md): the scores shown are the selected game's.)* |
| FR-003 | Game Start | While the menu screen is displayed, when the user presses the Nucleo board user button or the joystick's centre key, the system shall start the selected game (FR-040). |
| FR-040 | Game Selection | While the menu screen is displayed, the system shall offer **who plays** — the player or the AI — and **which maze** — the arcade's own layout ("classic") or a maze generated for each level ("random", FR-029) — as two independently settable choices, and shall start the combination that is selected. *(Amended by [DEC-055](11-Decisions-and-As-Built.md), at the owner's request. It was three fixed games — normal maze, Pac-Man AI, random maze — which conflated the two questions: "the AI" implied the arcade's maze, and a generated maze could not be played by the AI at all. Two axes give four combinations and the missing one is the interesting one.)* |
| FR-041 | High Scores per Game | The system shall keep a separate table of three high scores for **each combination of FR-040** — four in all — shall show the selected combination's table on the menu screen, and shall offer a finished run's score only to the table of the combination it was played as. *(Amended by [DEC-055](11-Decisions-and-As-Built.md): three tables became four with the axes. The numbers are not comparable across either axis — the arcade's maze can be learned and a generated one cannot, and a run the machine played is not a run anybody played — which is why they are separate tables and not one. The stored layout version rose with the count, so the scores from before the change are discarded rather than misread.)* |
| FR-042 | Autonomous AI Game | While a run the AI was selected for is in progress, the system shall take Pacman's direction from the AI from the first frame to the end of the run, and shall provide no way for the player to take over. *(Amended by [DEC-054](11-Decisions-and-As-Built.md) — the AI is the look-ahead search, not a trained network — and by [DEC-055](11-Decisions-and-As-Built.md), which made "the AI game" a *choice on the menu* rather than one of three fixed games, so it now applies to either maze.)* |
| FR-044 | Varied Timings | While a game is in progress, the system shall vary the ghost house release counts and every ghost timing — the scatter/chase phase durations, the frightened window and the house idle limit — from run to run by a bounded random amount, bounded both by 2 seconds and by half the nominal value, so that no timing can reach zero or double. |
| FR-045 | Random Source | On the target, every random value the firmware uses shall come from the MCU's hardware random number generator. The maze generator shall keep its own reproducible algorithm (FR-029) and take only its seed from that source. |
| FR-043 | Endless Mode | While the menu screen is displayed **and the AI is selected to play**, the system shall offer an endless mode; while it is on, the system shall start a further run when one ends instead of returning to the menu screen, and shall indicate the mode in the HUD. *(Amended by [DEC-055](11-Decisions-and-As-Built.md), at the owner's request: it was a board-button toggle *during* an AI run, and it is a row on the menu now — chosen before the run like everything else about it. The row is only offered while the AI plays, because a person's run has nothing to loop. That left the board button with nothing to do during a run at all, which is where [DEC-054](11-Decisions-and-As-Built.md) had already left the other half of it.)* |

### 2.1.2 Player Control & Rendering

| Unique-ID | Name | Description |
|---|---|---|
| FR-004 | Directional Control | While a game is in progress, when the user presses one of the four directional keys of the joystick on the display shield, the system shall move Pacman in the corresponding direction. |
| FR-005 | Game Rendering | While a game is in progress, the system shall render the current game state on the colour display. |
| FR-028 | Colour Rendering | While a game is in progress, the system shall render walls, pellets, Pacman and the ghosts in distinguishable colours, with each of the four ghosts visually distinct from the other three. |

### 2.1.3 Maze, Pellets & Movement

| Unique-ID | Name | Description |
|---|---|---|
| FR-010 | Maze Confinement | While a game is in progress, the system shall confine Pacman and the ghosts to the open paths of a single fixed maze, preventing any movement through walls. |
| FR-011 | Pellet Consumption | While a game is in progress, when Pacman enters a maze cell containing a pellet, the system shall remove that pellet and add its point value to the current score. |
| FR-012 | Tunnel Wrap-Around | While a game is in progress, when Pacman or a ghost exits the maze through a tunnel opening at a maze edge, the system shall re-enter it through the opposite tunnel opening on the same row (horizontal tunnel) or same column (vertical tunnel). |
| FR-022 | Playfield Size | The maze shall use a 28 × 31 cell grid rendered at 8 × 8 pixels per cell, in portrait orientation, leaving the remaining screen area for the HUD. |

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

### 2.1.6 Lives, Levels & End of Game

| Unique-ID | Name | Description |
|---|---|---|
| FR-006 | Starting Lives | The system shall start each game run with a fixed number of Pacman lives (default 3 — *tunable*). |
| FR-024 | Life Lost & Respawn | When Pacman is caught and at least one life remains, the system shall decrement the lives, reset Pacman and the ghosts to their level start positions, and continue the current level. |
| FR-007 | Game Over | When Pacman's remaining lives reach zero, the system shall end the game and return to the menu screen. |
| FR-025 | Level Count | The game shall consist of 21 levels differing in difficulty (FR-026), each played on the arcade's own maze or on a maze generated for it, according to the game selected (FR-040). *(Amended by [DEC-029](11-Decisions-and-As-Built.md), then by [DEC-045](11-Decisions-and-As-Built.md): every level used to play the same maze, then every level played a generated one, and now the player chooses.)* |
| FR-021 | Level Clear | While a game is in progress, when Pacman has consumed the last pellet and power pellet of the maze and it is not the final level, the system shall advance to the next level — restoring the pellets and applying that level's difficulty — while keeping the accumulated score and remaining lives. |
| FR-026 | Difficulty Scaling | No level shall be easier than the one before it, and the difficulty of each level — the speeds of Pacman and of each ghost, the frightened duration, the scatter/chase schedule, and the thresholds at which Blinky accelerates as the maze empties — shall follow the progression in [10 Pacman Game Design](10-Pacman-Game-Design.md). |
| FR-027 | Game Completion | When Pacman clears the final (21st) level, the system shall end the game as fully completed (won). |
| FR-029 | Randomly Generated Maze | When a level of a **random-maze** game (FR-040) begins, the system shall play a maze generated for that level rather than a fixed layout. Each generated maze shall be left-right symmetric, enclosed by wall except where a tunnel crosses it, have every pellet reachable from Pacman's starting cell counting the tunnel wrap, hold four power pellets, and place the ghost house, its gate, the four ghost starting cells and Pacman's starting cell at the same coordinates every time. Given the same seed the system shall generate the same maze. |
| FR-023 | End-of-Game Score Screen | When a game run ends (all lives lost, or the final level cleared), the system shall display the final score on its own screen for 2 seconds before returning to the menu screen. *(2 s default — tunable)* |

### 2.1.7 High Score & Persistence

| Unique-ID | Name | Description |
|---|---|---|
| FR-008 | High Score Update | If the final score of a completed game exceeds a stored high score of the game it was played as (FR-041), then the system shall store the new value in NVM. |
| FR-009 | High Score Persistence | The system shall retain the three highest scores of each game (FR-041) in NVM across power cycles, and shall provide a console command to show them and to clear them. |

The concrete rules and values that realise the gameplay requirements — the maze layout, movement/tick model, ghost targeting algorithms, power-pellet/frightened behaviour, scoring and end conditions — are specified in [10 Pacman Game Design](10-Pacman-Game-Design.md). The numeric constants there remain tunable for game feel (see [A-006](05-Risks-Assumptions-and-Dependencies.md#52-assumptions)).

### 2.1.8 Software Structure & Messaging

See [03 Architecture](03-Architecture.md) for how these are realized.

| Unique-ID | Name | Description |
|---|---|---|
| FR-101 | MVP Separation | The software shall implement the game using Model-View-Control (MVP) architecture, separating game state (Model), rendering (View), and game logic (Control) into distinct components. |
| FR-102 | Stateless Control | The Control component shall not persist game state between invocations; all game state shall reside exclusively in the Model. |
| FR-103 | Value-Only Module Interfaces | Firmware modules shall not share mutable state. Everything that crosses a module boundary shall be a fixed-size value copied by value — a declared message type — and no such value shall carry a pointer into another module's memory. |
| FR-104 | Host Buildability | The Model and Control components shall be buildable and executable unmodified on the host computer, using SDL to implement the View. |
| FR-105 | Cooperative Execution | The firmware shall run as a single cooperative loop with no preemptive scheduler. Periodic work shall be driven either from the 1 kHz system tick (input sampling and console reception) or from a software timer serviced by that loop (the frame), and no handler shall block the loop for longer than one frame. |
| FR-108 | Message Delivery to Subscribers | The message broker shall take each published message from its input queue and copy it into the output queue of every module subscribed to that message's topic — one copy per subscriber, so no two modules read the same stored message. Delivery shall be performed by the loop that owns the broker, when that loop asks for it, and not by a thread of the broker's own. |
| FR-110 | Pacman Internal Message Bus | The Pacman application shall carry its game events — pellet eaten, ghost eaten, frightened started — between its own modules on an internal message-bus instance that it owns, so that a module reacting to an event is not called by the module that caused it. |

### 2.1.9 On-Target Test (OTT) Framework

| Unique-ID | Name | Description |
|---|---|---|
| FR-106 | On-Target Test (OTT) Framework | The firmware shall expose each on-target test named in [06 Verification & Validation](06-Verification-and-Validation.md) as an individual command over the serial console CLI, following an OTT pattern: a setup step that parses/validates the command's arguments, and a run step that performs the action and asserts the expected outcome internally. See the [OTT CLI Framework](03-Architecture.md#37-on-target-test-ott-cli-framework) for the mechanism. |
| FR-107 | OTT Result Reporting | After executing an on-target test, the firmware shall report PASS or FAIL (with reason on failure) to the serial console, without requiring a debugger to be attached. |

### 2.1.10 Error Handling

| Unique-ID | Name | Description |
|---|---|---|
| FR-111 | Fatal-Error Halt | When the firmware detects a fatal, unrecoverable error, its error handler shall halt execution at a debugger breakpoint (`bkpt`) so the fault can be inspected. *(minimal handling for now; may be extended in later phases)* |

### 2.1.11 Pacman AI

A machine that plays the game, offered as a game of its own. **It is a search, not a trained
network** — it decides by cloning the run in progress and playing it forward through the game's own
rules ([DEC-050](11-Decisions-and-As-Built.md)).

**Most of this section used to be about training**, and went on 2026-08-17 with the trained network
itself ([DEC-054](11-Decisions-and-As-Built.md)): what the agent observes (FR-035), how it is trained
(FR-036), that the target only evaluates weights (FR-038), that host and target agree on an inference
(FR-039), the training environment (FR-112, FR-113) and the model's footprint (NFR-006..008). A
search has no weights, no training and nothing to port, so none of them had a subject any more. The
mid-run takeover went with them (FR-030, FR-032, FR-033): who plays is chosen on the menu.

Requirements about *how* the search works — its depth, its budget, what a leaf is worth — belong in
the design document, not here.

| Unique-ID | Name | Description |
|---|---|---|
| FR-031 | AI Control Exclusivity | While the AI is playing, the system shall take Pacman's direction from the AI and shall ignore the joystick's directional keys (FR-004). |
| FR-034 | AI Run Not Recorded | If AI control was active at any point during a run of a game a person plays (FR-040: normal maze, random maze), then the system shall not store that run's score in NVM (FR-008), however high the score. This shall not apply to the Pac-Man AI game, whose table is the AI's own (FR-041/FR-042). *(Amended by [DEC-046](11-Decisions-and-As-Built.md); before it there was one table and every AI-touched run was refused.)* |
| FR-114 | Reproducible Episodes | Given the same seeds and the same sequence of chosen directions, the host build shall replay identical episodes. *(The reason changed with [DEC-054](11-Decisions-and-As-Built.md) and the requirement did not: it was written for the trainer, and what needs it now is `Training/fit_lookahead.py`, which fits the search's evaluation weights against fixed seeds, and the unit tests, which assert exact timings. A score that moved under a fixed policy would be measuring the generator.)* |

## 2.2 Non-Functional Requirements

### 2.2.1 Timing & Performance

| Unique-ID | Name | Description |
|---|---|---|
| NFR-001 | Loading Screen Duration | The loading screen shall be displayed for no more than 3 seconds before the menu is shown. *(default value — see [A-001](05-Risks-Assumptions-and-Dependencies.md#52-assumptions))* |
| NFR-005 | Logo Display Delay | Upon power-on, the system shall wait 200 ms before displaying the Pacman logo of the loading screen (FR-001). |

### 2.2.2 Persistence

| Unique-ID | Name | Description |
|---|---|---|
| NFR-004 | NVM Write Frequency | The system shall write to NVM only when the high score changes, in order to minimize flash wear. |

### 2.2.3 Quality & Testability

| Unique-ID | Name | Description |
|---|---|---|
| NFR-101 | Unit Testability | The Model and Control components shall be unit-testable with Ceedling/Unity without requiring target hardware. |
| NFR-102 | Coding Standard Compliance | All C source code shall conform to the [c-code-style](https://github.com/MaxLell/c-code-style) coding standard. |
| NFR-103 | No Runtime Heap Allocation | The message bus implementation shall not perform dynamic heap allocation at runtime on target. |
| NFR-105 | Message-Bus Backpressure | The message broker shall provide an API for a publisher to query the free capacity of the broker input queue, and a publish attempt on a full queue shall return a status (bounded, non-blocking) rather than blocking the publisher indefinitely. |

### 2.2.4 Test Harness

| Unique-ID | Name | Description |
|---|---|---|
| NFR-104 | OTT Harness Compatibility | Every on-target test classified as automatable in [06 Verification & Validation](06-Verification-and-Validation.md) shall be triggerable via the OTT CLI (FR-106) and have its PASS/FAIL result parseable by an external test-runner script, without manual intervention beyond flashing. |

### 2.2.5 AI Model

| Unique-ID | Name | Description |
|---|---|---|

## 2.3 Constraints

### 2.3.1 Hardware

| Unique-ID | Name | Description |
|---|---|---|
| CON-001 | Target Hardware | The system shall run on the STM32U545RE-Q Nucleo-64 board. |
| CON-002 | Display Hardware | The system shall use the display of the X-NUCLEO-GFX01M2 expansion board (2.2" colour TFT, 240 × 320, over SPI) as its display. |
| CON-003 | Input Hardware | The system shall use the joystick of the X-NUCLEO-GFX01M2 — four directional keys plus a centre key — as its directional input device. |
| CON-004 | Carrier Hardware | The display shield shall connect to the Nucleo board through the ST-Morpho headers; no separate carrier board is used. |
| CON-005 | Debug Interface | The system shall use the on-board ST-LINK for debugging (SWD) and serial console output. |
| CON-006 | Non-Volatile Storage | The high score shall be stored in the microcontroller's internal flash. |

### 2.3.2 Software & Toolchain

| Unique-ID | Name | Description |
|---|---|---|
| CON-101 | Language | The firmware and game logic shall be written in C. |
| CON-102 | Test Framework | Unit tests shall run under Ceedling/Unity. |
| CON-103 | Host View Library | The host build's View shall use SDL. |
