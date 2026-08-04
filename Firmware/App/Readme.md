# App — Application Layer

Top of the layer stack. Holds the firmware entry point and the application itself;
nothing else may depend on `App/`.

**What is here**

- `app_main.c` / `app_main.h` — the entry point. It is *not* `main()`: the STM32CubeMX
  export owns `main()` (`ThirdParty/STM32_U545RE_HAL/Core/Src/main.c`), which does
  `HAL_Init()`, the clock setup and the `MX_*_Init()` peripheral bring-up, then calls
  `app_main()` from a USER CODE block. `app_main()` therefore starts from
  already-configured hardware and owns the software side:

  1. `prv_init_platform()` — initialise the BSP and service modules, and register
     `user_button_poll()` as the 1 ms tick callback so debouncing runs at a steady
     rate rather than at main-loop speed.
  2. `ott_execute_pending()` — run an on-target test scheduled before the last reset.
  3. print the boot banner (the harness looks for it, VT-INT-002).
  4. the nominal loop — the shell's frame and the OTT console, cooperatively
     ([03 §3.4](../../Docu/PrePlanning/03-Architecture.md#34-execution-model)). There is
     no scheduler and none is planned (DEC-027).

**The game** — one folder per module, all of it pure logic with no hardware behind it,
which is what lets the whole of it be unit-tested on the host.

| Module | What |
|---|---|
| `playfield/` | The maze: static walls per level plus which pellets are left. Walkability is a *query*, which is why it is a plain module and not an Active Object — routing it through a queue would make the movement code unwritable. |
| `agent/` | The shared base for anything that moves: a cell, a facing, and a step that respects walls and tunnels. Pacman and the ghosts *are* agents and differ only in **how they choose** a direction, so the wall and tunnel rules exist once. |
| `pacman/` | Current direction plus a *queued* one, applied at the moment of a step (§10.1). |
| `ghost_path/` | Choosing a direction towards a target cell, without reversing. |
| `ghost/` | The four personalities and their targets (§10.4), scatter/chase/frightened. |
| `score/` | Points and the ghost-eaten chain. The one Active Object here: it reacts to events on the game's internal bus (FR-110) rather than being asked. |
| `game/` | The orchestrator: the tick, collisions, lives, levels, and the state it hands to the view. |
| `game_view/` | Cells become pixels: the tile size, where the maze sits, interpolation between two cells, and which drawing and palette each actor gets. Owns the screen layout, so every layout decision is reachable by a unit test. |
| `render/` | The frame buffer, and the only module that decides **what to transfer**. Erases by save-under — 4 kB, against the 300 kB a second frame buffer would need. Knows nothing about the maze or the layout. |
| `host_main.c` | The SDL application (CON-103 / FR-104): a window, the keyboard standing in for the joystick, and the same game/view/render path the target runs. The only file allowed to talk to SDL. |
| `sprite_set/` | The drawings and their palettes. *This game's* art, which is why it is here and not in `Services/sprite` — that module knows how to draw any sprite, this one is the one set we have. Not inside the render port either: SDL on the host must draw the same figures as the panel. |

`game` owns the one broker instance and is also the boundary around it (FR-110):
game-internal events — pellet eaten, ghost eaten, frightened started — stay on the
internal bus and are fanned out at the end of each tick, and only results leave. What
the view gets is `msg_game_state_t`, **by value**, built fresh on each call so the
between-cell progress it carries is current
([DEC-016](../../Docu/PrePlanning/11-Decisions-and-As-Built.md)).

`shell/` and `game_session/` sit above that table: the shell owns the screen flow and
`game_session` owns the one frame the target, the host application and the `pacman` OTT
all run ([03 §3.2.2](../../Docu/PrePlanning/03-Architecture.md)).

Note that the `app_main()` call in the generated `main.c` is one of the two things a
CubeMX re-generation drops and that must be re-applied by hand (the other is the
`.noinit` section in the linker script). See the
[firmware README](../README.md).

**Depends on:** `Drivers/`, `Services/`, `Bsp/`. **Never** included by lower layers.
