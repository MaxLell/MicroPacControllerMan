# App — Application Layer

Top of the layer stack. Holds the firmware entry point and the application
itself; nothing else may depend on `App/`.

**What goes here**
- `main.c` — the entry point: run any pending OTT, initialise the layers below,
  then start the application / super-loop (later: the FreeRTOS scheduler).
- One folder per application module, e.g. `App/<module>/<module>.c`/`.h`
  (mirrors the reference's `App/app/`, `App/clockwork/`). The Pacman game
  modules land here during [Milestone 3](../../Docu/PrePlanning/04-Implementation-Phases-and-Milestones.md).

**Depends on:** `Drivers/`, `Services/`, `Bsp/`. **Never** included by lower layers.
