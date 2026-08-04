# 1 System Overview & System Context

[← Back to Index](Index.md)

## 1.1 Purpose

MicroPacControllerMan is a standalone embedded Pacman game. A player controls Pacman with a five-key joystick; the game state is rendered on a 240 × 320 colour display. The system boots into a loading screen, then a menu showing the persisted high score, and starts a new game when the on-board user button is pressed.

Beyond delivering a playable game, the primary motivation for this project is to explore the capabilities and boundaries of AI-assisted software development: how far an AI coding agent can carry an embedded project like this — from requirements through firmware — and where its limits are.

## 1.2 System Boundary

**In scope:**
- Firmware running on the STM32U545RE-Q Nucleo-64 board.
- Rendering to the X-NUCLEO-GFX01M2's display (colour, 240 × 320, RGB565).
- Reading directional input from that shield's five-key joystick.
- Reading the Nucleo user button (game start).
- Persisting a single high-score value in non-volatile memory (NVM).
- Core Pacman gameplay across **21 levels of increasing difficulty on one maze**, following the arcade's own progression; pellets and power pellets; four distinct ghosts; a frightened mode; tunnel wrap-around; multiple lives; and a final win after the last level (see [FR-006..FR-027](02-Requirements.md)).
- A host-computer build of the game logic (Model + Control) with an SDL-based View, for development and unit testing purposes ([Milestone 3.4](04-Implementation-Phases-and-Milestones.md)).

**Out of scope (for now):**
- Bonus fruit / bonus items, sound, multiplayer, wireless connectivity, multiple high-score entries, difficulty progression beyond the 5 defined levels.

## 1.3 External Interfaces

| Interface | Direction | Description |
|---|---|---|
| GFX01M2 joystick (5 GPIOs) | Input | Four directional keys plus a centre key, active low, translated into up/down/left/right movement intent. |
| Nucleo user button | Input | Starts a new game from the menu screen. |
| GFX01M2 display (ST7789V, SPI) | Output | Renders loading screen, menu, and live game state. |
| NVM (on-chip flash or equivalent) | Output/Input | Persists the single high-score entry across power cycles. |
| STLINK V3 (on-board) | Output | Debugging (SWD) and serial console (log output) during development. |

## 1.4 Context Diagrams

Two views: the physical hardware and the top-level software structure. Design choices such as the execution model are intentionally left out here — see [03 Architecture](03-Architecture.md).

### 1.4.1 Hardware Block Diagram

Blocks are physical components; each arrow is labelled with the electrical interface.

```mermaid
flowchart LR
    JOY["Joystick<br/>(5 keys, on the shield)"] -->|Digital Input| MCU["STM32U545RE-Q<br/>Nucleo-64 + X-NUCLEO-GFX01M2"]
    BTN["User Button"] -->|Digital Input| MCU
    MCU -->|SPI| LCD["Display<br/>(ST7789V, 240x320)"]
    MCU -->|on-chip flash| NVM["NVM<br/>(high-score storage)"]
    MCU <-->|SWD + Serial| STL["STLINK V3<br/>(debug + console)"]
```

### 1.4.2 Software Block Diagram

Blocks are the top-level software modules; they communicate only through the message broker (never directly). See [03 Architecture §3.2](03-Architecture.md#32-message-broker) for detail.

```mermaid
flowchart TB
    IN[Input]
    SY[System]
    GA["Game (Pacman)"]
    BROKER{{"Message Broker"}}
    RE[Render]
    NV[NVM]
    CO[Console]
    IN --> BROKER
    SY --> BROKER
    GA --> BROKER
    BROKER --> RE
    BROKER --> NV
    BROKER --> CO
```
