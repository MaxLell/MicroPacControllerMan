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
  4. the nominal super-loop — today just the OTT console; later the FreeRTOS scheduler.

**What goes here next** — one folder per application module,
`App/<module>/<module>.c`/`.h` (mirroring the reference's `App/app/`,
`App/clockwork/`). The Pacman game modules land here during
[Milestone 3](../../Docu/PrePlanning/04-Implementation-Phases-and-Milestones.md).

Note that the `app_main()` call in the generated `main.c` is one of the two things a
CubeMX re-generation drops and that must be re-applied by hand (the other is the
`.noinit` section in the linker script). See the
[firmware README](../README.md).

**Depends on:** `Drivers/`, `Services/`, `Bsp/`. **Never** included by lower layers.
