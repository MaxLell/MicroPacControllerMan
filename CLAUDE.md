# MicroPacControllerMan — agent guide

Standalone embedded **Pacman** on an **STM32U545RE-Q Nucleo-64**: joystick input,
240×320 colour LCD, single NVM high score. Secondary goal: probe how far an
AI agent can carry a disciplined embedded project. See `Docu/Idea.md` for origin.

> **Closed on 2026-08-04 (DEC-028), reopened the same day (DEC-029)** when the owner asked
> for randomly generated mazes — Milestone 5, delivered. **Every requirement in the spec now
> has a passing test.** The two that used to be unmet — a 60 fps rendering rate and a 30 ms
> input latency — are **withdrawn, not satisfied** (DEC-036): the owner judged both irrelevant
> to this game, so NFR-002 and NFR-003 are deleted along with VT-INT-016 and the automatic
> latency half of VT-INT-013. Both figures survive as *design* figures — 60 fps is why the
> frame period is 16 ms, and 30 ms is why RF-014 noticed the 32 ms debounce window — but
> nothing is measured against them. The close-out is
> [04 §4.2](Docu/PrePlanning/04-Implementation-Phases-and-Milestones.md#42-close-out) and
> what came after it is
> [04 §4.3](Docu/PrePlanning/04-Implementation-Phases-and-Milestones.md#43-milestone-5--random-mazes).

## Source of truth — read before coding

The **Pre-Planning doc set is authoritative**, not this file and not the code.
Start at **[`Docu/PrePlanning/Index.md`](Docu/PrePlanning/Index.md)**. Most-used:

- **[02 Requirements](Docu/PrePlanning/02-Requirements.md)** — EARS FR/NFR/constraints (the "what").
- **[03 Architecture](Docu/PrePlanning/03-Architecture.md)** — MVP, pub-sub broker, tasks, and **§3.9 the required folder layout**.
- **[04 Milestones](Docu/PrePlanning/04-Implementation-Phases-and-Milestones.md)** — phases + entry/exit criteria.
- **[06 Verification & Validation](Docu/PrePlanning/06-Verification-and-Validation.md)** — the VT-UNIT / VT-INT test IDs a milestone must pass.
- **[09 OTT Mechanism](Docu/PrePlanning/09-OTT-Mechanism-and-Reset-Flow.md)** — the reset-based on-target-test flow.
- **[11 Decisions & As-Built](Docu/PrePlanning/11-Decisions-and-As-Built.md)** — what was actually built and why (deviations from the intended design).

Each milestone also gets its own **design document** under **[`Docu/Design/`](Docu/Design/)**
carrying the *how* — pin assignments, clock settings, transfer budgets, tool choices. The
requirements deliberately carry none of that: keep hardware detail out of `02` and put it there.

Firmware specifics live in **[`Firmware/README.md`](Firmware/README.md)**.

Known work deliberately left undone is tracked in
**[`Docu/Refactoring-Backlog.md`](Docu/Refactoring-Backlog.md)** (`RF-xxx`) — check it
before "fixing" something that was a conscious deferral, and add to it rather than
silently working around a wart.

## Status

- **M1 Toolchain Bring-Up on the U545RE — done, verified on hardware.** Build → flash →
  boot → console, with the OTT CLI answering and `ott user_button` passing. Flash 38.9 kB
  (7.4 %), RAM 3.5 kB (1.3 %), `.noinit` present for the retained-RAM reset flow.
- **The doc set is re-baselined onto what is built** (DEC-027, 2026-08-04). **There is no
  RTOS and none is planned:** CON-104 is deleted, FR-105 is now *cooperative execution*
  (one loop plus the 1 kHz tick, [03 §3.4](Docu/PrePlanning/03-Architecture.md#34-execution-model)),
  FR-108 keeps the broker delivering to its subscribers but without a task of its own.
  FR-103 asks for what the firmware guarantees — no shared mutable state, everything
  crossing a boundary a fixed-size value copied by value — rather than for the
  system-level bus that was never built. One broker instance exists: the game's own,
  carrying its events to `score`.
- **M2 Board Bring-Up — done, merged (PR #14).** ST7789V display + joystick on the GFX01M2. The pin
  map is **measured, not assumed**: the joystick keys were confirmed by the `joystick` OTT
  and the display by `display_id`, which got the controller to answer. Chip select turned out
  **active LOW**, not the active high UM2750 claims. The ST7789V driver, the RGB565 frame
  buffer and partial updates are in (3 fps whole-frame becomes 290 fps for what a game
  actually changes), and `joystick_dot` and `animation` put input and display together.
  **The frame-rate target became 60 FPS, not 30** — measured: five moving actors cost 5.26 ms
  of a 16.7 ms frame, the unpaced ceiling is 175 fps, and the panel itself refreshes at 60 Hz.
  Open, and deliberately so: the 32 ms debounce window is a whole 30 ms input budget
  (RF-014), to be chosen against a real game loop. Both figures were requirements at the
  time and are design figures now (DEC-036).
  See [M2 Board Bring-Up](Docu/Design/M2-Board-Bring-Up.md).
- **M3 Game — done, playable on the board and on the host.** `game` publishes a
  246-byte state, `game_view` turns cells into pixels and interpolates between simulation
  steps, `render` owns the one frame buffer and erases by save-under, and `game_session`
  is the frame all three callers run — the target's `app_main`, the SDL window
  (`./build-host/pacman_host_app`) and `ott pacman`, so the window is evidence about the
  firmware rather than about a lookalike. No Data-Pool — every
  module talks through messages only, and no message carries a pointer (DEC-016). PR #10
  is closed; `host_main.c` was the last thing salvaged from it.
  The playfield is now the **arcade's own 28 × 31 maze at 8 px per cell**, one maze for
  every level, and the difficulty is the arcade's own progression in `App/difficulty` —
  per-level speeds, the tunnel crawl, the shrinking frightened window and **Cruise Elroy**
  (DEC-017). The run ends at level 21, where that table stops changing. The figures and the
  wall tiles are the **1980 ROMs**, decoded offline into `sprite_set`, so the maze is thin
  blue outlines and 244 pellets rather than filled blocks (DEC-018/019). Interpolation
  measures the step already taken rather than guessing the next one — which is what made
  corners stutter. The maze is written down twice on purpose, rules in `playfield` and
  appearance in `game_view`, with a unit test holding the two together. The **HUD** is in
  (DEC-020): score, level and lives in the arcade's own font, sent slot by slot so only the
  digit that moved travels. The **double frame buffer is resolved** (DEC-021): `render`
  owns the one buffer, `ott_framebuffer` borrows it, and `render` is in the target build.
- **M3 runs on the board.** `app_main` starts the game at power-on and polls the console in
  the same loop; `ott <name>` reboots into the test and `reset` returns to the game
  (DEC-022). Playing it is a test in its own right: `ott pacman` (VT-INT-022) starts a run
  with no menu in front of it and reports what a frame costs. Measured on the target:
  **RAM 67.3 %, flash 17.3 %** (176,428 of 256 kB; 89,496 of the 504 kB the linker
  leaves the firmware). Wiring it in broke
  `run_ott.py` — the UART receive register holds one character with no FIFO, and a loop
  that now spends milliseconds inside a frame drops most of a command line; the console
  samples it from the 1 ms tick into a ring buffer instead (RF-016 for the interrupt).
- **The game sits inside a shell** (DEC-026): loading screen, menu, the run, the score screen, back
  to the menu (FR-001/002/003/023) — and since DEC-055 the menu is where who plays and which maze is
  chosen. The title is set in the tile ROM's own font, plus **one glyph that was drawn and not
  decoded**: the hyphen of `PAC-MAN`, because the ROM extract is letters, digits and a space.
  `start` on the console presses the start key, so the whole flow is walkable from `run_ott.py`
  (VT-INT-011 is now automatic).
- **The high scores are in flash** (DEC-025, DEC-046): three tables of three, one per game the menu
  offers, behind a magic word, a version and a CRC, in one 8 KB page the **linker** reserves so the
  firmware cannot grow into it.
  `highscore` on the console prints them, `highscore reset` clears them, and `ott high_score`
  proves the round trip on real silicon — which is how the ICACHE was caught answering
  reads with what the page used to hold.
- **The ghosts are the Dossier's** (DEC-023): straight-line distance, the arcade's look-aheads
  and shy radius, a ghost house nobody may
  re-enter and Pacman may never enter, arcade spawn positions and dot-counter release, and
  the scatter targets in the unreachable dead space with the corners assigned the right way
  round. Seeking is the arcade's own **one greedy cell at a time**, tie-broken up-left-down-right:
  DEC-023 had replaced it with a breadth-first route search at the owner's request, and **DEC-049
  rolled that back** because a route search costs 300 µs a ghost step against the greedy rule's 10,
  which is the difference between 1 and 312 cells of simulated future in a frame's spare 13 ms. What
  it buys back is the arcade's shortsightedness — a ghost walks at the wall between it and its
  target, which is why its ghosts can be baited. **One deliberate departure remains, asked for by
  the owner: a ghost never turns around** — the
  arcade's forced reversal on a mode change is gone (DEC-037), so a mode change moves a ghost's
  target and takes effect from the next junction. Only a dead-end stub still forces the way
  back, and no generated maze has one. Measured either side: 86 reversals over 25 runs became 0.

- **M5 Random Mazes — done, verified on hardware** (DEC-029/030, 2026-08-04). Every level of a
  **random-maze** game plays a maze **generated** for it (FR-029); since DEC-045 the arcade's one
  layout is the other game the menu offers rather than a retired fixture. `App/maze_gen` is a
  faithful port of the tetris-stacking generator from
  [shaunlebron/pacman-mazegen](https://github.com/shaunlebron/pacman-mazegen) — 9 × 5 grid of
  stacked pieces, upscaled by three, mirrored, which *is* 28 × 31. Faithful on purpose, JavaScript
  accidents included, because that is what made it checkable: the original and the port were run
  under the same seeded PRNG and their output compared **byte for byte over 300 seeds**. The seed
  is **the tick at the moment start was pressed**, and `ott pacman` prints it so a maze can be
  reproduced. The ghost house, its gate, the four ghost starts and Pacman's start stay at the
  arcade's coordinates — the generator's own grid already puts them there — so release, revival,
  the gate rule and the scatter targets keep their meaning. **Tunnels are shorter** than the
  arcade's six cells (one or two), so the tunnel is a weaker escape.
  **The maze is no longer written down twice**: `game_view` derives the appearance from the walls
  (DEC-030), which reproduces the arcade's hand-drawn tile map for **764 of 764 cells** outside
  the two tunnel masses; the 64 inside them are excluded and asserted. Two tiles were added as
  vertical mirrors of the top tees — the 1980 ROM has no bottom-edge tee and 62 % of generated
  mazes need one. **The outer frame is 6 px thick, not the arcade's 2** (DEC-031): the owner asked
  for it to match the weight a two-cell inner wall already renders at, knowing that at 6 px in an
  8 px cell the screen corners become solid right angles. The ghost house keeps the arcade's 2 px
  wall and has four tiles of its own for it. **A wall's outline is a 6 px stroke set 5 px in
  whatever its thickness** (DEC-032), drawn across two cell rings — the same rule one cell further
  in, turned half a turn, so no new art was needed; a wall exactly three cells thick has no room
  for a hole and is drawn solid, which is most generated boxes. A **tunnel mouth is 18 px**, the
  same gap a corridor leaves between two wall lines. **The outer wall has a second cell in the
  panel's margin** (DEC-033) and is therefore an ordinary two-cell wall: the whole frame tile
  family and its rules are gone, pellets are centred to 0.0 px everywhere, and flash went *down*.
  The ghost house has no margin to borrow, so it keeps a one-cell wall with the 6 px band centred
  in it. **And then the tile alphabet went away** (DEC-034): the maze is drawn as geometry now —
  a pixel is ink when its distance to the wall's nearest edge is in `[inset, inset + 6)`, inset
  being half the spare depth capped at 5 — so width and setback are arithmetic rather than a
  choice from 24 ROM tiles that only composed at one thickness. The pixels travel in the display
  list (`DISPLAY_ITEM_WALL`). The arcade tile comparison is gone with them; what replaces it are
  two unit tests that rebuild the picture and measure it — every pellet within 1 px of its
  corridor's centre, every tunnel mouth exactly a corridor's gap wide. Every wall stroke's centre
  lands on a cell boundary — including the ghost house's, which needed a second drawing cell to get
  there (a one-cell ring can have the 6 px stroke, the grid, or a roomy inside: any two, and the
  owner chose the grid).
  RAM 68.0 %, flash 18.7 %; frame cost unchanged at 8 ms of 16.
  See [M4 Random Mazes](Docu/Design/M4-Random-Mazes.md).

- **M6 Pacman AI — done, and the trained network is gone (DEC-038..054).** M6 was an agent evolved
  on the host with **NEAT**, ported to the target as `const` weights. **All of it was deleted on
  2026-08-17 at the owner's request (DEC-054)**: `App/pacman_ai`, `Services/neural_net`, the whole
  training stack under `Firmware/Training/`, the recorded equivalence states, `ott ai_equivalence`
  and `ott ai_frame_cost`. The machine that plays is the look-ahead search below.
  - **The measurements had made the case before the instruction did.** The trained agent settled at
    **3,531** over a fortnight of campaigns; four separate ideas were measured *not* to help — a
    per-ghost bonus, a level-completion bonus, a bigger population, and more capacity (32 hidden
    units scored 2,634, *below* the 16-unit baseline). The search reaches **21,870** in the same
    frame budget. Keeping both meant keeping a Python dependency set, a container, an export step, a
    re-recording step and an equivalence test alive for the weaker of the two.
  - **What went with it in the spec** — thirteen requirements and three tests, the largest spec change
    the project has made: FR-030/032/033 (the mid-run takeover), FR-035, FR-036, FR-038, FR-039,
    FR-112, FR-113, NFR-006/007/008, CON-105, VT-UNIT-009, VT-UNIT-011, VT-INT-024. Kept with the
    *reason* rewritten: FR-031 (the stick is dead while the machine plays), FR-034 (an AI run stays
    out of a person's table), FR-114 (reproducible episodes, now for `fit_lookahead` and the unit
    tests). **B1 no longer hands Pac-Man over**, so in a person's game it does nothing at all.
  - **It bought back headroom**: RAM **91.6 % → 88.1 %**, flash **23.4 % → 20.9 %**. That is the
    first time either has moved the helpful way, and it is what §17.3 spent capping the leaf scan.
  - **Two tests had to be rethought, both because the search plays too well to finish a run.**
    `test_shell.c` ran twelve minutes before it was killed and `ott ai_high_score` timed out at
    240 s. Both now zero the search's weights through `pacman_lookahead_set_weights`, so every
    position is worth the same and it dies quickly — the shipped code through its own public setter,
    and those tests are about which table a run reaches, not about play strength.
  - **What is still worth reading** in [M6 Pacman AI](Docu/Design/M6-Pacman-AI.md) is §15–§17, the
    search. §1–§14 are the trained agent and are history — kept because a fortnight of measured
    negative results is the most useful part of this milestone.

- **The machine that plays is the look-ahead search: 21,870 at Ø level 5.9 on the classic maze, 19,744 at 5.0 on generated ones (DEC-050..055).** `App/pacman_lookahead`
  decides by **playing the game forward**: `game_clone` copies the run, the clone is driven down
  each way out to the next junction, and the branch whose end position is worth most wins. There is
  no model of the game in it — the forward model *is* `game_tick`, so no second set of rules can
  drift. Two things made that true rather than nearly true: a `memcpy` is **not** a clone (the
  game's bus is pointers into itself, so a byte copy would score simulated pellets on the real
  player), and a simulation must **not draw** — FR-044's jitter comes from the one shared generator,
  so thinking would change the future; `game_freeze_timings` stops it and a unit test counts the
  draws and requires none.
  - **It is not FR-038's agent and does not replace it** — that asks for trained weights on the
    target. This is the reference M6 §2 kept in reserve; since DEC-051 it is *offered* as one of the
    two agents the AI's game could be played by. It is now the *only* machine, and FR-038 — which
    asked for trained weights on the target — was deleted with the network (DEC-054).
  - **What it settles, and the correction that had to be made to it** (M6 §15.4/§15.5): the 7,076
    this line used to claim is real but belongs to a **5,000-tick** search, measured before the
    board cut the allowance to 500 and left standing afterwards. **What ships scores 3,132** over
    the twenty draws 1000..1019 — *below* the 4,600 then asked for, though still above the trained agent's
    2,706 and a random policy's 518. The conclusion survives: given the time, the same code reaches
    7,076 with a best run of 16,560 at level 4, so **the score is reachable and the gap is the
    agent's, not the game's** — it now also has a price, about **three times** the thinking a frame
    pays for.
  - **It dithers, and the dithering is a symptom.** 7.0 % of decisions walk back to the cell of two
    decisions ago. Outside a 1.63-junction horizon every branch evaluates alike — 15.2 % of
    multi-branch decisions are exact ties — and nothing in the evaluation pulls toward food it
    cannot already see. Suppressing the oscillation entirely is measured and **costs score**
    (2,945), as do a coarser simulation step and pruning the reversal branch: they buy depth and
    lose fidelity. The only lever that works is **more simulated ticks of the real game**, and the
    frames to spend them in exist — 51.5 per junction-to-junction leg, worst 18, against the one
    frame a decision uses today.
  - **Both levers §15.5 named are built (DEC-052, M6 §16), and the player scores 11,652** over a
    hundred draws against 3,123 before them — 11,947 over the twenty draws 1000..1019, against 4,600
    then asked for. Neither costs a millisecond of frame time that was not already there.
    - **A stranded Pacman is noticed at once (RF-019, done).** A leg sets one direction and lets it
      ride, so a bend stranded him and the walk could only wait out the 32-tick backstop: **17.8 %
      of every simulated tick**. `pacman_is_stuck` is the rule, written as the negation of
      `pacman_advance` so the two cannot drift. Worth +25 % over a hundred draws. It is not a
      fidelity trade — the played game asks again on every cell and always hands him an open way,
      so the stalled Pacman the backstop simulated does not occur.
    - **A decision thinks across the frames of its cell, a slice at a time.** A decision belongs to
      a cell, a cell lasts 10.6 frames, and the search used to work in the first and idle through
      nine. The recursion is an explicit stack now, so it can be put down between any two branches:
      `pacman_lookahead_restart` on a new cell, `pacman_lookahead_think(350)` a frame,
      `pacman_lookahead_get_direction` every frame. **Staleness is unchanged** — a decision was
      always rooted at the cell's first frame and always took effect at its last, because
      `pacman_set_intent` queues.
    - **The root copy lives in SRAM4**, the 16 kB bank three documents called "exactly big enough
      for one clone" while nothing used it. Main RAM is unchanged at 89.9 %. It needed a `.sram4`
      section in the linker script (NON-GENERATED, beside `.noinit`) — without one the section
      becomes an orphan, lands in RAM, and main RAM goes to 94.5 % while SRAM4 reports empty.
    - **On the board:** what must fit a frame is now a **slice at 2.9 ms mean / 11 ms worst** of 13,
      where a whole decision was 11.2 / 13. Junctions reached **1.63 → 2.97 of 3**; a decision
      spends ~1,400 ticks over 10 frames instead of 500 in one. Whole automatic suite passes.
    - **A refactor is checked by sameness, not by a mean.** The rewrite was verified by diffing the
      two versions' decisions over a run, which found a real bug the scores would only have argued
      about: the recursion's entry test was being re-asked on the way *out* of a level, so a level
      that spent the last of the budget on its children had its finished answer thrown away. What
      holds it now is a test that slices of 1, 7, 64 and 350 ticks all answer what one shot answers.
    - **The dithering is better and not gone**: 18.7 % → 11.5 % of decisions, longest streak 56 →
      50. The cause §15.5 named first is untouched — nothing in the evaluation pulls toward food it
      cannot already see. Breaking ties by carrying straight on measured 3,573 against 3,123 and is
      deliberately **not** built, because it changes what the player decides rather than how much it
      may think.
  - **A leaf can see now, and the weights were fitted rather than argued (DEC-053, M6 §17).**
    The evaluation had three terms and went blind past eight cells; it has six, all of them from
    **one bounded breadth-first walk** outwards from the leaf — nearest killing ghost, nearest
    frightened one, nearest pellet, ways out of the cell — so distances are *maze* distances and a
    ghost behind a wall is as far as the way round to it. **A\* is the wrong tool** and was rejected
    for a reason worth keeping: it answers one-to-one, this is forty-five leaves against four ghosts,
    and on a unit-cost grid it degenerates to breadth-first with a heap bolted on.
    - **`Training/fit_lookahead.py` fits the six weights** with a (1+9) evolution strategy on seeds
      2000.., driving the shipped loop through `fit_lookahead.c` so what is fitted is what ships.
      1.5 h took 10,368 → 30,874, and it generalised (30,262 on the untrained 1000..1019).
    - **What the fit found matters more than the number: it stopped playing for pellets and started
      playing for ghosts.** `point` 10 → **2**, `prey` 20 → **53**. That is the game's own arithmetic
      — a four-ghost sweep is 3,000 against 2,440 for a level of pellets — and the hand-picked
      weights had it backwards. No amount of arguing finds that.
    - **The board rejected the first two versions**, and the middle step is the lesson: stopping the
      scan as soon as it has its answers cut the *mean* and left the worst frame at 23 ms of 13,
      because the worst case is exactly where there is nothing nearby to stop for. **An early exit
      fixes the common case; only a cap fixes the worst one**, and a frame budget is a worst-case
      promise. So the bound is now the *work* — at most 48 cells visited — plus a slice down from 350
      to 250 ticks. Worst frame 11 ms of 13, and it passes.
    - **That cost a third of the gain, honestly reported**: 30,262 → **21,870** at Ø level 5.90. Still
      nearly double §16 and it runs on the board, which the 30,262 does not.
    - Left undone on purpose: the weights are the best ones for the *uncapped* player, so refitting
      against the shipped configuration is the next hour of wall clock. `density` was deleted —
      weight 2, and the only term forcing a full sweep.
  - **DEC-049's arithmetic was six times too kind** and the board said so: a simulated cell costs
    **250 us**, not the 40 the four greedy ghost steps suggested, because a cell is seven
    `game_tick` calls of Pacman, four ghosts, the timers and the bus. A frame's spare 13 ms buys
    about 50 cells, not 312.
  - **The budget counts ticks, not cells**, because a branch walking into a wall spends ticks and
    reaches no cell — bounding cells left the waste unbounded, and a search keeping to 48 cells
    still took 19 ms of a 13 ms allowance. And the search **deepens iteratively**: depth-first at a
    tight budget spends everything on the first branch, and the player that produced scored *worse*
    than one walking in a straight line.
  - Ceiling 3 junctions, **2.97 of them actually reached**; a decision spends ~1,400 ticks across the
    10 frames its cell lasts, in slices of 350 that measure **2.9 ms mean, 11 ms worst** of 13 —
    verified by `ott lookahead_cost`, which measures a *frame* now rather than a decision. **RAM is
    the ceiling of the depth constant, not of the search**: a level of depth is a 12 kB `game_t` on
    this part, three of them plus the frame buffer put main RAM at **89.9 %**, and the root copy is
    the 12 kB in **SRAM4**. Until something calls the module the linker drops it whole.
    See [M6 §15](Docu/Design/M6-Pacman-AI.md) and [§16](Docu/Design/M6-Pacman-AI.md).

- **The menu is a list and a confirm, one page per question (DEC-056, FR-040..043).** The owner
  sketched it as one screen after another:
  ```
  page 1        page 2 (after PLAY)     page 2 (after AI)    page 3 (AI only)
  - PLAY        - CLASSIC               - CLASSIC            - ENDLESS OFF
  - AI          - RANDOM                - RANDOM             - ENDLESS ON
                ^ start the run         ^ on to page 3       ^ start the run
  ```
  Up and down move within a page — the highlighted option *is* the choice, so there is no separate
  commit — and the centre key leaves the page, either onward or into the run. **B1 steps back a
  page**; on page 1 and the score screen it falls through to meaning start.
  - **This is the menu's third shape in three days.** Three fixed games (DEC-045), then two settable
    axes on one screen (DEC-055), then this. The axes were rejected on sight: a screen carrying every
    choice at once reads denser than a short list asked twice, and the endless row's *conditional*
    appearance was the tell that the axes were carrying something they did not fit.
  - **The endless mode stopped needing a rule.** DEC-055 had to say when its row applied and make the
    cursor step over it. Here it is simply the last page of the AI's path, and a person's path does
    not pass through it — the shape of the walk carries the condition, so the code does not.
  - **Two high-score tables, one per maze** (FR-041), layout version **4**. The AI's tables are gone
    (DEC-056): a run nobody played is not a score anybody set, so **FR-034 is back to what it said
    originally** — an AI run reaches no table at all, and there is no exception to state.
  - **Two things the pages made necessary.** A way back, because every other key goes forwards and
    picking `AI` by accident was a trap. And a page must **reopen on what it was told**: going back
    from the maze page and forward again silently reset the maze in the first version, so
    `prv_open_page` puts the cursor on the decision that was made, and a test walks out and back in
    to hold it.
  - **The console had to learn to talk.** `start` and `button` print which page they landed on, since
    neither always starts a run any more; VT-INT-026/027 walk the three pages forwards and back over
    the serial line and would otherwise be reading silence. `prv_walk_to` in `test_shell.c` is the
    helper that made the third rewrite of these tests short — it drives the pushes rather than
    setting state, so the next change to the menu's shape rewrites one function, not twenty tests.

- **The ghosts are paced randomly, from the MCU's own generator (DEC-047, FR-044/045).** Every
  timing the ghosts are paced by — the house's dot counts, the scatter/chase phases, the frightened
  window, the idle timer — moves by up to 2 s **or half its nominal value, whichever is smaller**, so
  the arcade's twentieth-of-a-second phases at level 5 keep their character instead of being replaced.
  `Bsp/rng_bsp` is the source: the **RNG peripheral** on the target (registers, not the HAL — see the
  conventions below), a **seeded xorshift** on the host so a training episode still replays (FR-114).
  `maze_gen` keeps its own reproducible algorithm and takes only its *seed* from there, which is what
  keeps a maze replayable. `ott rng` (VT-INT-028) proves on silicon that the words are neither
  constant nor zero. The jitter is **off in the rules tests** — they assert exact timings — and has
  three tests of its own.
  - **It cost the AI its requirement, and that is the point.** The agent trained on the
    deterministic game scored **4,980 on its one episode and 2,197 over twenty jittered runs**: it
    had memorised a trajectory. The score is a mean over 20 runs again, and the agent is being retrained
    against the game it actually plays.
  - **Training's objective is no longer the score** (FR-036): **+500 a ghost** on top of it, and an
    episode ends at the **first** life lost. A flat penalty per life was rejected — a run ends
    *because* its lives are gone, so it would be a near-constant, and on a run cut short by the idle
    rule it would reward standing still.

## Build · flash · test (all from `Firmware/`)

```bash
# Build (arm-none-eabi-gcc + CMake; STM32CubeMX + STM32 HAL under ThirdParty — see 11 DEC-012).
# The cross-toolchain lives in CMakeLists.txt above project(), so no -DCMAKE_TOOLCHAIN_FILE.
cmake -B build -G "Unix Makefiles"
cmake --build build -j                                   # -> build/pacman.elf, warning-free

# Flash over ST-LINK V3E. NOT openocd — see "Hardware facts" below.
STM32_Programmer_CLI -c port=SWD -w build/pacman.elf -v -rst

# A machine with no cross-toolchain flashes the committed image instead. It is a copy, it goes
# stale silently, and Firmware/Prebuilt/README.md carries the commit it was built from.
STM32_Programmer_CLI -c port=SWD -w Prebuilt/pacman.hex -v -rst

# Run an on-target test end-to-end (schedules, resets, reports over the VCP)
python3 Test/run_ott.py --suite                          # the automatic ones, unattended
python3 Test/run_ott.py --manual                         # the ones needing you at the board
python3 Test/run_ott.py pacman --port /dev/ttyACM0        # one by name; exit 0 = PASS

# Or the umbrella, which wraps every one of these
./dev.sh check                                           # format + unit tests + both builds
./dev.sh all                                             # build + flash + both OTT suites
./dev.sh install-hook                                    # format staged files + test on commit

# Another machine, with only Docker installed (Firmware/docker/Dockerfile, Ubuntu 24.04 so the
# tool versions are the verified ones). The repo is mounted, not copied; the container runs as
# the host's user. NOT in the image: STM32CubeProgrammer (ST account) — mount the host's and set
# PROGRAMMER=. Inside the container the trainer's Python is on PATH, so no Training/.venv.
./dev.sh docker                                          # a shell in it
./dev.sh docker check                                    # any dev.sh command inside it

# Host build — no hardware, no cross-toolchain
cmake -B build-host -DPACMAN_HOST_BUILD=ON -G "Unix Makefiles" && cmake --build build-host -j
./build-host/pacman_host_app                             # play it: arrows/WASD pick the game and steer,
                                                         # space starts it, esc quits

# Host unit tests (Ceedling + Unity + CMock; needs ruby + `gem install ceedling`)
ceedling test:all

# Fit the look-ahead search's evaluation weights (host only, stdlib only — no venv needed)
cmake --build build-host -j --target pacman_lookahead_fitness
FIT_HOURS=1.5 python3 Training/fit_lookahead.py     # -> Training/lookahead_weights.json
./build-host/pacman_lookahead_fitness 1000 20 2 70791 13 53 2 10    # one candidate, by hand
```

**There is no training any more** (DEC-054). NEAT, the evolution strategy, the campaign runner,
`libpacman_env.so`, the weight export and the equivalence recorder were all deleted with the trained
network. What is left is `fit_lookahead.py`, which fits **six** numbers — the search's evaluation
weights — against whole games on fixed seeds. It writes a JSON and touches nothing in the firmware:
adopting a result means copying the numbers into `pacman_lookahead.c`'s defaults deliberately, and
then measuring on the board, because a leaf scan that fits a frame is the constraint the fit does not
know about.

Fit on seeds 2000.., and **never on 1000..1019** — those stay reserved so a reported score is
measured on draws nothing was fitted to.

- **What gets a unit test: everything above the BSP.** The BSP is the *mocking*
  boundary — mock a `Bsp/` header to test the module above it; don't unit-test the BSP
  itself. Hardware is verified by the OTTs in `Test/Target`, which are never mocked and
  never unit-tested. Details in [`Firmware/Test/Readme.md`](Firmware/Test/Readme.md).
- A **platform port** is one shared header + one `.c` per platform, selected in
  `CMakeLists.txt` — see `Bsp/systick_bsp` (`systick_bsp.c` / `systick_bsp_host.c`).
  Prefer that over `#ifdef`s inside a module.
- `ASSERT` comes from the vendored `ThirdParty/embedded_utils`; a test can verify that
  a precondition fires via `Test/support/assert_probe.h`.

Toolchain (verified): gcc-arm-none-eabi **13.2.1**, cmake **3.28**, openocd **0.12.0**
(debug only), **STM32CubeProgrammer 2.23.0** (flashing).
`sudo apt-get install -y gcc-arm-none-eabi binutils-arm-none-eabi libnewlib-arm-none-eabi cmake
openocd` — newlib is the target's C library and only a *Recommends* of the compiler, so an install
without recommends fails on `math.h` — plus
CubeProgrammer from st.com. After installing openocd, **unplug/replug the board once**
so a non-root user can reach SWD — openocd ships the udev rules even though it cannot
program this part.

## Hardware facts

- **STM32U545RE-Q Nucleo-64**, Cortex-M33, TrustZone **off** (`CORTEX_M33_NS`).
  On-board **ST-LINK V3E** = SWD debug **+** virtual COM port.
- **512 kB flash, 274 kB SRAM** — 256 kB contiguous at `0x20000000` plus 16 kB SRAM4 at
  `0x28000000`. That headroom is *why* the MCU changed: a 240×320 RGB565 frame buffer is
  153.6 kB and the G431RB had 32 kB in total.
- **SYSCLK 160 MHz** (PLL1 from **MSIS 48 MHz**: `M=3 → 16 MHz × N=10 → 160 MHz, R=1`),
  1 kHz SysTick. Owned by the CubeMX `.ioc`; see
  [M2 Board Bring-Up §2](Docu/Design/M2-Board-Bring-Up.md).
  Never express a delay as a spin count — use `Services/delay` / `Services/sw_timer`.
- Serial console: **USART1 on PA9/PA10**, **115200 8N1**, at **`/dev/ttyACM0`**.
  The VCP is on the ST-LINK side, so it **stays enumerated across a target reset** —
  that is what makes the OTT reset flow work on one serial handle.
- **User button B1 = PC13, active HIGH** (idle low). Measured, not assumed: read
  `GPIOC->IDR` over SWD with the button released. **LED LD2 = PA5** (also Arduino SPI1 SCK).
- **Flashing needs STM32CubeProgrammer, not openocd.** openocd 0.12.0 attaches fine and
  is the gdb server, but its flash driver only knows `STM32U57/U58xx` (device ID 0x482)
  while this board reports **0x455** (STM32U535/U545), so `program` fails with
  `auto_probe failed`. Reasoning is in `Firmware/openocd.cfg`.
- **Reading target registers over SWD is a cheap way to settle a hardware question**
  without anyone at the board: `openocd -f openocd.cfg -c "init; halt; puts [format 0x%08X
  [mrw 0x42020810]]; resume; shutdown"` is `GPIOC->IDR` (GPIO on AHB2, `0x42020000` +
  `0x400` per port, IDR at `+0x10`). Always take a control reading from a pin whose level
  you already know — an all-zero register looks the same as a disabled clock.
- **X-NUCLEO-GFX01M2** (**ST7789V**, 240×320 colour, 5-GPIO joystick, plus a 64-Mbit SPI
  flash we deliberately do not use). Pin map measured and recorded in
  [M2 Board Bring-Up §1](Docu/Design/M2-Board-Bring-Up.md): display SCK **PA5** / MOSI
  **PA7** / MISO **PA6** / CS **PC7** (**active LOW**) / DCX **PB10** / RESET **PA1**;
  joystick NORTH **PC0**, SOUTH **PB4**, EAST **PB0**, WEST **PC9**, CENTER **PC6**, all
  active low with the shield's own pull-ups. Two traps: UM2750 claims CS is active *high*
  and it is not, and register reads carry a one-**bit** dummy so byte-aligned reads land
  off by one bit.

## Conventions

- **Layout:** layered tree `App / Bsp / Drivers / Services / Test / ThirdParty`,
  one folder per module — the binding rules are [03 Architecture §3.9](Docu/PrePlanning/03-Architecture.md#39-firmware-source-tree-layout).
  BSP peripheral wrappers carry the **`_bsp` suffix** (`dio_bsp`, `i2c_bsp`, …),
  matching [BareMetalHollowClockFw](https://github.com/MaxLell/BareMetalHollowClockFw);
  a generic primitive and its instance are separate modules (`switch` vs. `user_button`).
- **All GPIO goes through `dio_bsp`** — name the pin in `dio_bsp_pin_e` + one row in
  `g_pin_map`. Do not call `HAL_GPIO_*` anywhere else.
- **No tick arithmetic, no `millis()`.** `Services/delay` for blocking waits,
  `Services/sw_timer` for every timeout and periodic job.
- **HAL over registers.** Direct register access needs a justifying comment; there are
  exactly two today. `uart_bsp_read_character()` reads `RDR` — the HAL has no non-blocking
  single-character read. `rng_bsp.c` drives the RNG's three registers because the HAL's RNG
  driver is **not compiled**: `HAL_RNG_MODULE_ENABLED` is commented out in the CubeMX export,
  and enabling it would be a third hand edit to generated code of the kind a regeneration
  discards (DEC-047).
- **Adding an OTT test:** new `Firmware/Test/Target/scripts/ott_<name>.c/.h` + one row
  in `ott_scenarios.c` + the source in `CMakeLists.txt`; nothing else changes.
- **Coding standard:** [c-code-style](https://github.com/MaxLell/c-code-style)
  (NFR-102), vendored as `Firmware/.clang-format`: Allman braces, 4 spaces, 120
  columns, `prv_` static functions, `g_` for globals **and file-scope statics** (including
  const lookup tables — there is no separate `k_` prefix), `in_`/`out_`/`inout_` on
  pointer parameters, named constants over literals, no abbreviations, `ASSERT` on
  public-function preconditions. Keep comments sparse — say *why*, not *what*.
  Fixes to the standard itself go via a separate PR to that repo, only with the
  owner's prior approval.
- **Verify on hardware** when a change has a runtime effect (build → flash → run
  the relevant OTT), the way M1 was verified.

## Git workflow

- Each milestone lands as its **own reviewed PR against `main`**; branch, don't
  commit to `main` directly.
- **Only push to this repo.** Never push to or modify other repos (e.g.
  c-code-style, EmbeddedCli) without explicit approval.
- After pushing to an open PR, re-check for new review comments.
