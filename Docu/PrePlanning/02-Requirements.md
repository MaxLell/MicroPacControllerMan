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
| FR-040 | Game Selection | While the menu screen is displayed, the system shall offer a choice of three games — the arcade's own maze ("normal"), the same maze played by the AI alone ("Pac-Man AI", FR-042), and generated mazes ("random", FR-029) — shall move the choice with the joystick's up and down keys, shall mark the selected one, and shall play the selected one when the game starts. The choice shall not change while a game is in progress. *(Amended by [DEC-046](11-Decisions-and-As-Built.md): it offered two.)* |
| FR-041 | High Scores per Game | The system shall keep a separate table of three high scores for each game of FR-040, shall show the selected game's table on the menu screen, and shall offer a finished run only to the table of the game it was played as. |
| FR-042 | Autonomous AI Game | While the Pac-Man AI game is in progress, the system shall take Pacman's direction from the trained AI from the first frame to the end of the run, and shall provide no means for the user to take control. If the trained weights cannot be evaluated, then the system shall not start the game rather than start it under the player's control. |
| FR-044 | Varied Timings | While a game is in progress, the system shall vary the ghost house release counts and every ghost timing — the scatter/chase phase durations, the frightened window and the house idle limit — from run to run by a bounded random amount, bounded both by 2 seconds and by half the nominal value, so that no timing can reach zero or double. |
| FR-045 | Random Source | On the target, every random value the firmware uses shall come from the MCU's hardware random number generator. The maze generator shall keep its own reproducible algorithm (FR-029) and take only its seed from that source. |
| FR-043 | Endless Mode | While a Pac-Man AI game is in progress, when the user presses the Nucleo board user button, the system shall toggle an endless mode; while it is on, the system shall start a new run when one ends instead of returning to the menu screen, and shall indicate on the display that it is on. Starting a game from the menu screen shall leave the endless mode off. |

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

A trained agent that can take over from the player. The **training happens on the host and only
there**; the target evaluates fixed weights and never learns. The two halves are therefore held
together by an equivalence requirement (FR-039) rather than by hope — the same trick that made
the maze generator checkable ([DEC-029](11-Decisions-and-As-Built.md)).

Requirements about *how* the agent is built — what it observes, its architecture, the reward and
the export format — belong in the design document, not here.

| Unique-ID | Name | Description |
|---|---|---|
| FR-030 | AI Takeover Toggle | While a **normal-maze** game (FR-040) is in progress, when the user presses the Nucleo board user button, the system shall toggle Pacman's control between the player and the trained AI. In a random-maze game the system shall refuse the takeover. *(Amended by [DEC-045](11-Decisions-and-As-Built.md): the AI is trained on the normal maze and is offered there only.)* |
| FR-031 | AI Control Exclusivity | While AI control is active, the system shall take Pacman's direction from the AI and shall ignore the joystick's directional keys (FR-004). |
| FR-032 | AI Takeover Indication | While AI control is active, the system shall indicate in the HUD that the AI has taken over. |
| FR-033 | AI Control Persistence | While a run is in progress, the system shall preserve the AI control state across a level change (FR-021) and across the loss of a life (FR-024). Every new run shall begin under player control. |
| FR-034 | AI Run Not Recorded | If AI control was active at any point during a run of a game a person plays (FR-040: normal maze, random maze), then the system shall not store that run's score in NVM (FR-008), however high the score. This shall not apply to the Pac-Man AI game, whose table is the AI's own (FR-041/FR-042). *(Amended by [DEC-046](11-Decisions-and-As-Built.md); before it there was one table and every AI-touched run was refused.)* |
| FR-035 | Observation Bounded by the Display | The AI shall decide from no information beyond what the display shows the player: the maze, the remaining pellets, Pacman, the four ghosts and which of them are frightened, the score, the level and the remaining lives. |
| FR-036 | The AI's Objective | The AI shall be trained to maximise the score a run reaches, across levels rather than within one, with two additions: eating a ghost shall be worth more to the training objective than the score it pays, and a training episode shall end when a life is lost. *(Amended by [DEC-047](11-Decisions-and-As-Built.md). The training objective is therefore no longer the score, which is measured separately and, since [DEC-053](11-Decisions-and-As-Built.md), is not a requirement. A flat penalty per life was considered and rejected: a run ends **because** its lives are gone, so the penalty would be a near-constant, and on a run cut short by the idle rule it would reward the idling.)* |
| FR-038 | Inference Only on the Target | The target shall evaluate the trained weights only. No training, weight update or exploration shall run on the target. |
| FR-039 | Host / Target Inference Equivalence | Given the same game state and the same weights, the target shall choose the same direction as the host build, over a recorded set of states covering ordinary play, frightened mode, the tunnels and a life just lost. |
| FR-112 | Training on the Shipped Game | The training environment shall be the firmware's own game modules built for the host — the same sources the target runs — and not a re-implementation of the game. |
| FR-113 | Headless Parallel Training Sessions | The training harness shall advance multiple independent game sessions concurrently, with no rendering and no display. |
| FR-114 | Reproducible Episodes | Given the same seeds and the same sequence of chosen directions, the training environment shall replay an identical batch of episodes. *(Amended by [DEC-047](11-Decisions-and-As-Built.md): the seed now also seeds the game's timing jitter (FR-044), and a batch stepped in lockstep interleaves the draws — so the guarantee is per batch. Training scores one episode per batch, where the two are the same thing.)* |

## 2.2 Non-Functional Requirements

### 2.2.1 Timing & Performance

| Unique-ID | Name | Description |
|---|---|---|
| NFR-001 | Loading Screen Duration | The loading screen shall be displayed for no more than 3 seconds before the menu is shown. *(default value — see [A-001](05-Risks-Assumptions-and-Dependencies.md#52-assumptions))* |
| NFR-005 | Logo Display Delay | Upon power-on, the system shall wait 200 ms before displaying the Pacman logo of the loading screen (FR-001). |
| NFR-006 | AI Inference Budget | While AI control is active, one inference shall complete within 2 ms, so that it fits inside the frame alongside the simulation and the drawing. *(default — the frame is 16 ms and about 8 ms of it is currently unused)* |

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
| NFR-007 | Model Footprint | The trained weights and every buffer inference needs shall fit the target's remaining memory: at most 300 kB of flash for the weights and at most 40 kB of RAM for the inference buffers. *(measured headroom when this was written: ~410 kB flash and ~82 kB RAM free; the limits leave the firmware room to keep growing)* |
| NFR-008 | No Heap for Inference | Inference shall not allocate memory at runtime. The weights shall be `const` data in flash and every intermediate buffer shall be reserved statically. |

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
| CON-105 | Training Toolchain | The training harness may depend on third-party Python packages, unlike the OTT harness of NFR-104, which is standard-library only. The trained weights shall be exported as a C source file, so that neither the firmware build nor the unit tests depend on Python or on a machine-learning framework. |
