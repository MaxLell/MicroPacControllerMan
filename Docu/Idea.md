# 1 Idea
I want to be able to play Pacman on a display. I have a touchpad (which should be used to implement the "directional cross" control) and a black-and-white display, on which Pacman should be rendered.

At system startup, I want a short loading screen to be shown (displaying the Pacman logo). Then I want to briefly see a menu showing the current high score. (Only a single entry, which should be stored in NVM). For now, Pacman only needs a single life.
Then, when the user button on the Nucleo board is pressed, the game should start.

# 2 Constraints
## 2.1 Coding Standard
- When writing C code, use this coding standard. If you find issues in this coding standard, please open a PR against its main branch with your fixes: https://github.com/MaxLell/c-code-style
- The code should be written in C
- Unit tests should be runnable with Ceedling / Unity.
## 2.2 Requirements
- For writing the requirements, I want:
    - Use the EARS standard for creating the requirements: https://alistairmavin.com/ears/
    - Organize the requirements into
        - Functional
        - Non-Functional
        - Constraints
    - The requirements should be structured in a table with the following columns:
        - Unique-ID
        - Name
        - Description
## 2.3 Coding Workflow
- During development, you must work with Git. Once we get there, I will give you the ability to push directly into a defined Git repo. If you want to change other repos (of mine), please let me know beforehand. I don't want any overspill into other repos here. If anything is unclear, please let me know.
### 2.3.1 Software Architecture
- I want the software modules in the firmware to communicate with each other via a pub-sub messaging framework.
- I want Pacman to work with
    - MVP Architecture.
        - Model: Contains the entire game state
        - View: This is the format that is then rendered. On the host, SDL should be used. On the target, you are free to use whatever you want.
        - Control: This is where the actual game logic resides. This should be as "stateless" as possible.
- On the microcontroller, multiple threads should be set up in FreeRTOS, in which the different aspects of the firmware then run.
## 2.4 Hardware
- The platform I am using for this is the STM32G431RB Nucleo board
- I have a Carrier Shield from MikroE: https://www.mikroe.com/click-shield-for-nucleo-64. Be careful with the pinout here. You need to make sure whether the pinout documented here actually matches the Nucleo board.
    - The following display is seated in the first slot: https://www.mikroe.com/lcd-mono-click
    - The following touchpad input is seated in the second slot: https://www.mikroe.com/touchpad-click
- The assembled hardware has an STLINK V3, which can be used for debugging as well as for outputting the serial console output. Use this for your iterations.

# 3 Milestones / Phases
The whole project should not be developed all at once, but rather in phases. (There may also be more phases than what is listed here.)
## 3.1 Pre-Planning
I want to be able to use this later in Obsidian, so I want the elements to be cleanly linked to each other
1. I want you to produce Functional Description Documentation. This should consist of several documents:
    1. System Overview & System Context
    2. System Requirements & System Architecture
    3. Software Requirements & Software Architecture
    4. Implementation Phases / Milestones
    5. Risks, Assumptions & Dependencies
    6. Verification & Validation - these are the verification tests. These should also have their own IDs, so that they can later be linked to the requirement IDs in the traceability matrix.
        1. For the hardware itself
        2. Helper tests
        3. Smoke tests
        4. Acceptance tests
        5. ....
    7. Traceability Matrix
    8. Troubleshooting Guide

The output of this phase should be the Pre-Planning information mentioned above. This should then serve as the foundation for all further phases and the next steps. This should be the "source of truth" for the project - not this idea document here.
## 3.2 Toolchain Bring Up
Get the blinky LED on the board running. I want to see the output.
## 3.3 Board Bring Up
- Get the hardware running. I want reproducible tests here, with which I can perform the hardware tests.
- Here I want the microcontroller, the display, and the touch input to be running.
## 3.4 Pacman Development
- Here it's about developing Pacman. Most of the development here should be covered by unit tests.
- Pacman should be runnable on the host computer at this stage
## 3.5 System Integration - Pacman Game on Hardware
- Here the Pacman game should be integrated onto the hardware.
