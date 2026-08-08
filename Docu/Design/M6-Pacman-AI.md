# M6 — Pacman AI

[← Back to Index](../PrePlanning/Index.md) · Requirements: [02 §2.1.11](../PrePlanning/02-Requirements.md) ·
Milestone: [04 §4.4](../PrePlanning/04-Implementation-Phases-and-Milestones.md#44-milestone-6--pacman-ai)

The *how* behind FR-030..039 and FR-112..114: an agent trained on the host that plays Pacman on
the board. The requirements say what it must do; this document says how it is built, and it is
the place for every number, tool choice and trap.

> **As-built, with FR-037 outstanding again — and this time for an instructive reason.** Everything
> below is implemented and, where it touches hardware, verified on the board: `Services/neural_net`,
> `App/pacman_ai`, `Firmware/Training/`, the takeover in the game, the agent's own game with its
> endless mode, and every automatic on-target test. The play-strength figure was met, briefly, at
> **4,980 points on a single deterministic run**. Then the game's timings were randomised
> ([DEC-047](../PrePlanning/11-Decisions-and-As-Built.md), FR-044) and the same network averaged
> **2,197 over twenty runs**: it had memorised one trajectory. §14.3 is the record. Figures marked
> **measured** are real; anything still a budget says so.

The naming follows the milestone: this is Milestone 6. (The random-maze design document is
called `M4-Random-Mazes.md` although it delivered Milestone 5 — a pre-existing mismatch, left
alone rather than renamed under a feature branch.)

## 1 The shape of the thing

Three pieces, and the seam between them is what keeps this honest:

| Piece | Where it runs | What it is |
|---|---|---|
| **Environment** | host | the firmware's own `game_t`, stepped headless (FR-112) |
| **Trainer** | host | Python, evolves networks against that environment (CON-105) |
| **Inference** | host **and** target | one C implementation, compiled for both (FR-038/039) |

The environment is not a re-implementation. `game_t` turned out to be almost exactly what a
training environment wants, and that is not luck — it is the value-only interface rule (FR-103)
and the injected clock paying off:

- **measured** — `game_init` / `game_start` / `game_tick(game_t*, elapsed_ms)` carry no
  file-scope state, so N concurrent environments are an array of N structs. `sizeof(game_t)` is
  **15,000 bytes**, so a thousand of them is 15 MB.
- **measured** — time is a parameter, not a clock read. Nothing paces to real time.
- **measured** — the simulation is deterministic: outside the seeded `maze_gen` there is no
  `rand()` anywhere in `App/`, `Services/` or `Drivers/`. Same seed and same actions replay the
  same episode, which is FR-114 for free.
- **measured** — **15,429 steps/s** on one core (8 games × 200,000 steps of 16 ms in 103.7 s),
  including a 248-byte `game_get_state_message` every step, with the host library at `-g` and no
  optimisation.

`game_session` is deliberately *not* used: it is a singleton around the one frame buffer
([DEC-021](../PrePlanning/11-Decisions-and-As-Built.md)), so it could not host a second
concurrent run. Training talks to `game` directly, which is also the layer with no view in it.

## 2 Why neuroevolution and not gradient RL

The choice is **NEAT** (NeuroEvolution of Augmenting Topologies): a population of networks whose
weights *and topology* are evolved, fitness being what the run scored.

The owner's own resources point there twice independently — both linked custom implementations
([jjwarren44/Pacman-AI](https://github.com/jjwarren44/Pacman-AI),
[allenmonkey970/NEAT-Pacman](https://github.com/allenmonkey970/NEAT-Pacman)) drive `neat-python`
with a `config-feedforward.txt`, and Code Bullet's Pacman used NEAT as well. But it is also the
right fit here for reasons this project can check:

- **It wants exactly the parallelism we have.** One genome is one independent episode in its own
  `game_t`. There is no shared state to synchronise and no replay buffer to keep coherent, so
  FR-113 is satisfied by the structure of the algorithm rather than by careful locking.
- **It produces networks small enough to port without thinking about it.** Both linked configs
  start at **8 inputs → 4 outputs with 0 hidden nodes** and grow only where growth pays. Against
  NFR-007's 300 kB that is three orders of magnitude of headroom, and it stays small: NEAT's
  complexity cost is a selection pressure, not an afterthought.
- **Fitness *is* the requirement.** FR-036 says maximise the score; NEAT takes the score as
  fitness directly. There is no reward-shaping step whose bias has to be argued about before
  training can start.
- **It needs no gradients, so nothing has to be differentiable.** The observation can hold hard
  booleans and integer distances without smoothing them, and the port needs no autograd
  framework — CON-105's "export the weights as C" is a few hundred bytes of `const` data.

What it costs is sample efficiency. That is affordable here, and the budget is worth writing
down rather than hoping:

| | |
|---|---|
| measured episode length, random policy | 2,918 steps |
| assumed episode length, competent agent | 10,000 steps (lives longer — an estimate) |
| population (following the linked configs) | 150 |
| one generation | 150 × 10,000 = 1.5 M steps ≈ **97 s** on one core |
| on 8 cores | ≈ **12 s** per generation |
| Code Bullet's run finished at | generation 73 → ≈ **15 min** |
| a long run, 500 generations | ≈ **1.7 h** |

That is a coffee break, not a GPU campaign — which is the whole reason the owner does not have to
care about training this himself. The `-g` build was what got measured, so `-O2` on the
environment can only improve it.

**What it actually cost, measured.** The budget above was wrong in both directions and the errors
cancelled. A generation of 150 genomes on four mazes takes **11 s on two cores**, not 12 s on
eight — because the whole episode runs in C ([DEC-042](../PrePlanning/11-Decisions-and-As-Built.md))
and a decision costs one function call rather than one crossing of the language boundary; the
achieved rate is **6,500 decisions/s**, and a decision is on average about twelve simulation steps.
But an episode is far shorter than 10,000 steps: the trained agent's runs end after **192 to 460
decisions**, because three lives go quickly. Against that, stage 1 was promoted after **10
generations** and stage 2 after **13**. Stage 3 is where the time goes, and §14 says why.

**The rejected alternative, and why it is still useful.** The Stanford CS221 assignment in the
resources is *not* learning at all — it is minimax, alpha-beta and expectimax over an evaluation
function. It would very likely outscore a small evolved network, and it needs no training. It is
rejected because FR-038 and the owner's brief ask for trained weights ported to the target, not a
search. But it earns a place as a **reference opponent**: an expectimax agent gives FR-037 an
upper reference to compare against, and if evolution stalls it is the natural teacher for
behaviour cloning.

## 3 What the agent sees

FR-035 allows everything the player sees. That is a permission, not an instruction — and the
useful reading of it is *nothing is hidden from the agent*, rather than *the agent is handed a
bitmap*. The features below are computed from the **full** `msg_game_state_t` plus the maze, so
no information is withheld; it is compressed, not censored.

Everything is expressed **in Pacman's frame**, which is the single most important decision in
this document and the reason it is expected to generalise across generated mazes (FR-029). A
policy in compass coordinates has to learn "wall to the north" four times over; a policy in
Pacman's frame learns it once.

For each of the four relative directions — **forward, left, right, back**:

| Feature | Encoding |
|---|---|
| the next cell is walkable | 0 or 1 |
| maze distance to the nearest remaining pellet reached that way | normalised, 1 if none |
| maze distance to the nearest power pellet reached that way | normalised, 1 if none |
| maze distance to the nearest dangerous ghost reached that way | normalised, 1 if none |
| maze distance to the nearest frightened ghost reached that way | normalised, 1 if none |

plus three scalars about the run as a whole: how much of the frightened window is left, what
fraction of the level's pellets remain, and whether any ghost is edible at all. **23 inputs.**

"Maze distance" is a breadth-first distance over the walkable cells, counting the tunnel wrap —
**not** straight-line distance. A wall makes Euclidean distance lie, and lying to the agent about
which way the food is costs generations. `App/ghost_path` already does a route search over
`playfield` and is unit-tested; the feature extractor should reuse it rather than grow a second
search.

One BFS over 868 cells serves all four directions at once, because the search is seeded from
Pacman and each frontier cell remembers which of his neighbours it came from.

## 4 What the agent does

Four outputs — **forward, left, right, back** — and the largest wins.

This is the insight the owner flagged in the resources himself, and it is worth stating why it
matters beyond "others did it too". Relative actions make the policy invariant to Pacman's
heading, so a single learned rule ("wall ahead, corridor to the left → go left") covers all four
compass cases instead of being learned four times. Together with the relative observation of §3
it is what lets one policy play mazes it has never seen — which is the requirement, since every
level's maze is generated (FR-029).

The game itself does not change. `game_set_direction` takes an absolute `direction_e`, and the AI
module converts its relative choice using Pacman's current facing. Nothing in the rules learns
that an AI exists.

*Back* stays in the action set. Pacman may reverse; it is the ghosts that never do
([DEC-037](../PrePlanning/11-Decisions-and-As-Built.md)), and an agent that cannot turn around
would be trapped in every dead end — though generated mazes have none.

## 5 When it decides

**Once per cell**, not once per frame.

Pacman can only change direction at a cell boundary (§10.1), so a decision inside a cell is
thrown away. At level-1 speed a cell takes about 167 ms against a 16 ms frame, so deciding per
cell is roughly **ten times cheaper** in both training and inference and costs nothing in play
quality. It also keeps a decision aligned with the thing the reward is about — a step through the
maze — rather than with the panel's refresh.

## 6 Fitness and the curriculum

**Every episode is played on the normal maze** — the arcade's own layout — because that is the only
maze the game will hand the AI control in (FR-040, [DEC-045](../PrePlanning/11-Decisions-and-As-Built.md)).
That single sentence settles two things this section used to spend a lot of words on. A genome plays
**one** episode per generation rather than twelve, since one fixed maze and a game with nothing
random in it produce the same episode every time; and fitness is therefore a *measurement* rather
than a draw, comparable across generations as well as within one. §14 is the record of what the
noise it replaces was costing.

It also raises the stage thresholds. On the normal maze a level is 244 pellets — 2,440 points while
a power pellet is demoted to an ordinary one, 2,600 once it is not — and the old promotion bar of
1,800 is reached by the *first* generation, by wandering. Both early stages promote at **2,200**
now: almost the whole of level 1, which is what "can walk" and "can stay alive" ought to mean.

Fitness is the run's score (FR-036), with one addition the transcript in the resources is emphatic
about: **an idle agent must die.** Code Bullet's first stage killed a Pacman that ate nothing
within a time limit, and without that an agent discovers that hiding in a corner beats playing.
Our game has `dots_idle_ms`, but it only releases ghosts from the house — it does not end a run.
The **training harness** therefore ends an episode when no pellet has been eaten for a set
interval. That is a harness rule, not a game rule: nothing about the shipped firmware changes.

The transcript's other lesson is a three-stage curriculum, and it is the plan
([DEC-041](../PrePlanning/11-Decisions-and-As-Built.md)):

| Stage | Ghosts | Power pellets | What the agent is learning |
|---|---|---|---|
| 1 | inert | demoted to ordinary pellets | navigate the maze and eat |
| 2 | hunting | demoted to ordinary pellets | stay alive; a ghost is *only* death |
| 3 | hunting | as generated | a ghost is sometimes food |

The point of stage 2 is the one the transcript is emphatic about: *"sometimes I may eat a ghost
and sometimes it kills me"* is an ambiguity a fresh network cannot resolve while it is still
learning to walk. Removing the energizers removes the ambiguity rather than merely reducing the
pressure to resolve it — which is why this is the plan and **fitness shaping is not**. Shaping the
reward (score pellets, ignore death, then penalise it) would leave the contradictory rule in
place; staging removes it and puts it back once the agent can walk.

Staging changes the *rules*, and the rules are the shipped game (FR-112), so the switches are
**runtime** configuration and not a training-only build:

```c
game_config_t config;
game_get_default_config(&config);      /* everything on: the game FR-001..029 describe */
config.has_ghosts = false;             /* stage 1 */
config.has_power_pellets = false;      /* stages 1 and 2 */
game_start_on_map_configured(&game, &normal_maze, &config);
```

`game_start`, `game_start_on_map` and `game_start_on_normal_maze` pass the defaults, so the firmware
neither sets a config nor is affected by one existing. **Measured** after the change: flash **+288 bytes**, RAM **+0** (the
two flags fit in padding `game_t` already had), 381 host unit tests green, both builds
warning-free.

Two honest limitations of the seam as built:

- **The ghosts are inert, not absent.** They are still placed and still appear in a snapshot, so
  a stage-1 agent sees four stationary ghosts — three parked in the house it cannot enter anyway,
  and Blinky standing above the gate forever. It will learn to avoid that one cell for no reason.
  Making them genuinely absent would need an "absent" encoding in `msg_actor_t` that nothing else
  wants, and the cost of the wart is one cell.
- **Demotion is substitution, not removal.** A power pellet becomes an ordinary one, so the pellet
  counts are unchanged and "the level is cleared" keeps its meaning and its code path. The score
  is lower by what the four energizers and any eaten ghost would have paid — which matters when
  comparing a stage-2 fitness against a stage-3 one, and is why FR-037 is only ever measured at
  stage 3.

## 7 The training harness

Python drives C over `ctypes` against a shared library built from the same sources as the
firmware:

```
Firmware/Training/env_api.c        -> libpacman_env.so, the game as a shared library
Firmware/Training/pacman_env.py    -> the ctypes shim, and nothing else
Firmware/Training/net.py           -> a genome flattened into what neural_net reads
Firmware/Training/train.py         -> neat-python, the evolution loop, the curriculum
Firmware/Training/evaluate.py      -> VT-UNIT-010: FR-037 and its baseline, one run
Firmware/Training/export_c.py      -> the winner -> App/pacman_ai/ai_weights.[ch]
Firmware/Training/record_states.c  -> pacman_ai_record: the FR-039 state set as C
Firmware/Training/config-neat.txt  -> the evolution's settings
Firmware/Training/requirements.txt -> neat-python 2.0.0, and that is all
```

**The network is evaluated by the C side even during training**
([DEC-042](../PrePlanning/11-Decisions-and-As-Built.md)) — `net.py` flattens a genome into the
arrays `Services/neural_net` reads and the library plays the whole episode. neat-python's own
`FeedForwardNetwork` never plays. That is what turns FR-039 from something to verify into
something that cannot be violated: there is one implementation of inference in this project.

`Firmware/Training/` is a new top-level folder in the firmware tree and therefore an amendment to
[03 §3.9](../PrePlanning/03-Architecture.md#39-firmware-source-tree-layout) — recorded as
[DEC-040](../PrePlanning/11-Decisions-and-As-Built.md). It builds only in the host
configuration and nothing in the target build refers to it.

The API is **batched on purpose**: one call steps *every* environment, rather than one call per
environment. At 150 genomes and ~15 k steps/s each, per-call FFI overhead would otherwise become
the bottleneck rather than the simulation. `env_run` goes further and plays whole episodes without
returning, which is what training uses; `env_step` and `env_observe` remain for a caller that wants
to drive one decision at a time, which is what recording and debugging want.

```c
env_batch_t* env_create(uint32_t count);
void         env_reset(env_batch_t*, const uint32_t* seeds, uint8_t stage, uint8_t maze);
void         env_step(env_batch_t*, const uint8_t* relative_actions,
                      float* out_features, float* out_fitness_delta, uint8_t* out_done);
void         env_destroy(env_batch_t*);
```

`maze` is `ENV_MAZE_NORMAL` or `ENV_MAZE_GENERATED`, and the seeds are ignored for the former —
there is one normal maze and every level of a run plays it. Training and FR-037 use the normal maze
only; the generated path stays so that "how does this agent do on a maze nobody has ever played"
remains a question that can be *asked* (`evaluate.py --maze generated`), even though nothing gates
on the answer any more.

The feature extraction lives on the **C** side of that line, not in Python — because it is the
same code the firmware must run (FR-039). A Python re-implementation of the observation would be
a second thing to keep in step, and the first thing to silently diverge.

## 8 From weights to firmware

Two modules, split along the project's own convention that a generic primitive and its instance
are separate ([`switch`](../../Firmware/Bsp/switch) vs. `user_button`):

| Module | What |
|---|---|
| `Services/neural_net` | Generic feed-forward evaluator over a `const` weight blob: topologically sorted nodes, connection list, biases. Knows nothing about Pacman. |
| `App/pacman_ai` | The instance: extracts the 23 features from a game state and a maze, evaluates the network, turns the winning relative action into a `direction_e`. Owns the generated weights. |

Not `App/ai_agent`: `App/agent` is already the base class for everything that moves through the
maze, and two modules a letter apart is how a reader ends up in the wrong file.

`export_c.py` emits `App/pacman_ai/ai_weights.c` — `const` arrays of connections
(`from`, `to`, `weight`), node biases, evaluation order, plus the feature count and a hash of the
whole blob. NEAT's topology is arbitrary, so the evaluator walks an explicit node order rather
than multiplying fixed-size matrices; that is a hundred lines and it is deterministic, which is
what §9 needs.

## 9 Making host and target agree

FR-039 asks the target to choose the same direction as the host for the same state. This is the
same discipline that made the maze generator checkable — compare the port against its original
instead of believing it ([DEC-029](../PrePlanning/11-Decisions-and-As-Built.md)) — and it has two
traps that must be designed out rather than debugged later.

**Trap 1: `double` on the host, `float` on the target.** C promotes float arithmetic to `double`
by default on the host, while the target has a single-precision FPU
(`-mfpu=fpv5-sp-d16 -mfloat-abi=hard`). The same network would then evaluate differently on the
two sides. So the evaluator uses `float` throughout with explicit `float` literals, in a fixed
evaluation order, and `-ffast-math` is banned in both builds.

**Trap 2: transcendental activations are not portable.** `tanh` and `sigmoid` come from the host's
libm on one side and newlib on the other, and they are not required to agree to the last bit. The
NEAT configuration is therefore restricted to **`activation_options = relu`**, which is exact in
float32 and identical everywhere. This costs some expressiveness and buys an equivalence that can
actually be asserted.

Even so, the comparison is on the **chosen action**, not on bit-identical activations — with the
tie-break defined as the lowest action index, so a tie cannot decide differently on two machines.

The check itself: the host writes a set of recorded states and the actions it chose for them, and
`ott ai_equivalence` replays them on the target and compares (VT-INT-024). The states cover
ordinary play, frightened mode, a tunnel and a life just lost — the four places where the feature
extractor has something interesting to do.

**As built** ([DEC-043](../PrePlanning/11-Decisions-and-As-Built.md)): the recorder is
`Training/record_states.c`, built as `pacman_ai_record`, and it is C rather than Python so that it
calls the very `pacman_ai_decide` the target calls. It plays seeds in order until all four
situations have turned up — they came from seeds 1 and 2 — and prints them as the C the target
compiles in. A recorded case carries its **maze but not its pellets**: the state's own bitmaps
already say where those are, and the target eats them back off a freshly loaded maze, so there is no
second copy to disagree with the first. That step is not optional, because one of the 23 features is
how much of the level is left and it comes from the playfield rather than from the state. The
recording also carries the weight table's **digest**, and the test refuses to run when it does not
match the firmware's — so re-exporting weights without re-recording says so instead of looking like
a porting fault. **It passed on the board first time.**

## 10 Fitting the target

The room available, **measured** before any of this was built: **419,804 bytes of flash free**
(96,292 of 516,096 used) and **83,900 bytes of RAM free** (178,244 of 262,144), plus the 16 kB SRAM4
that nothing uses. Of the 16 ms frame, about 8 ms is unused.

**What it came to, measured after.** Flash **21.00 %** (108,380 of 516,096) and RAM **71.54 %**
(187,528 of 262,144), from 18.7 % and 68.0 %. Broken down: the trained network's tables are **298
bytes**, the feature extractor's search scratch is **4,340 bytes** of RAM, and the rest of the
growth — about 7 kB of each — belongs to `ott ai_equivalence`, which carries four recorded states in
flash and rebuilds a 7 kB playfield in RAM. So the *agent* costs well under a tenth of NFR-007's
300 kB flash and an eighth of its 40 kB RAM; the *test* costs more than the agent does, which is a
fair trade for an equivalence that is checked on silicon rather than assumed.

Against NFR-007's 300 kB flash / 40 kB RAM, an evolved network is not a close call:

| | weights | inference RAM |
|---|---|---|
| NEAT network as the linked configs start it (8→4, no hidden) | ~150 B | ~100 B |
| a grown network, say 23 → 40 hidden → 4, fully connected | ~4.5 kB | ~300 B |
| the feature extractor's BFS over 868 cells | — | ~1.7 kB |

The BFS dominates, and it is still 2 % of the RAM budget. Inference cost is dominated by the same
BFS — 868 cells of integer work, tens of microseconds against NFR-006's 2 ms.

This is the sizing that justified §2's choice of a small network over a grid-input convolutional
one. A conv net over the whole 28 × 31 grid with 32 channels needs **108 kB** of activations in
float32, against 82 kB free — it does not fit at all, and at 8 channels with two ping-pong buffers
it is ~54 kB, still over NFR-007. The escalation path, if a feature-based agent is not good
enough, is an egocentric window with int8 activations, not a whole-maze net.

## 11 In the game

The player-facing half is small and is fully specified by FR-030..034 and FR-040. Four notes on the
mechanics:

**The agent appears in two of the three games, and differently in each.** The menu asks which game to
play (FR-040): `NORMAL MAZE` lets the player hand Pac-Man over mid-run and take him back (FR-030),
`PAC-MAN AI` is the agent playing that same maze from the first frame with no way to take over
(FR-042), and `RANDOM MAZE` does not offer it at all. `shell_toggle_ai` enforces all three in the
*one place* the decision can be made once, for the same reason the joystick lock-out lives in
`game_session_set_direction`: one door, so it holds however many devices are wired to it. A menu that
says what the AI does and a button that does something else would be the menu lying.

**The agent's game keeps its own high scores, and FR-034 narrowed because of it.** The lockout exists
because an AI-assisted run would otherwise sit in a *person's* table; with a table per game
(FR-041) that reason stops applying to the agent's own, and refusing there would leave one of the
three tables permanently empty. So an AI-touched run of a person's game reaches no table at all —
checked on the board, including that it is not quietly filed under the agent's game — and the agent's
own game files into its own.

**And it can be left running.** In its own game the board button toggles an endless mode (FR-043): a
finished run starts the next one instead of returning to the menu, and the HUD says `LOOP`. It is a
way to watch what the agent actually does over many runs rather than one, which is the difference
between a score and a habit.

**The user button has to learn about screens.** `App/app_main.c:138` currently routes
`user_button_take_press()` straight into `shell_press_start()`, unconditionally. It has to become
a decision by screen: start on the menu and score screens (FR-003), toggle the AI while a game is
running (FR-030). The centre joystick key keeps meaning "start" only.

**The toggle belongs to the run, not to the frame.** FR-033 requires it to survive a level change
and a lost life, so the flag lives with the run — `shell` owns the run, so it owns the flag — and
is cleared when a new run begins.

**The lockout is a sticky bit.** FR-034 is not "the AI is on now" but "the AI was on at some
point", so a second flag latches on the first takeover and is only cleared by a new run. It is
read where the score is offered to `high_score`, and the HUD indication (FR-032) is driven from
the live flag rather than the sticky one.

## 12 Open points

1. ~~**`-O2` for the training environment.**~~ **Closed.** The library is built a second time from
   the same source list at `-O2` while `pacman_host` stays at `-g` for the debugger and the SDL
   window; sharing the list is what stops the two drifting apart.
2. **Whether 23 features are the right 23.** Still open, and now the *second* place to look rather
   than the third: §14 shows the limit was fitness noise, and the features have not yet been given a
   fair test against a policy that was selected on ability rather than on luck. The linked projects
   got by with 8.
3. ~~**Whether one policy can play *generated* mazes at all.**~~ **Closed, and by a change of
   product rather than by an answer.** It was measured first: a policy trained only on mazes it would
   never see again reached **2,270 points on twenty unseen mazes against 431 for a random policy**, so
   it did generalise and was simply not good enough. Then the owner asked for both mazes to be
   playable and for the AI to be offered in the arcade one only (FR-040,
   [DEC-045](../PrePlanning/11-Decisions-and-As-Built.md)) — which spends the single-maze fallback
   this point was keeping in reserve, deliberately and for a reason that is about the game rather
   than about the training. The question is no longer one the product asks.

## 13 The acceptance seed set — retired

**Superseded by [DEC-045](../PrePlanning/11-Decisions-and-As-Built.md).** FR-037 is now measured on
one full run of the **normal maze**, against twenty episodes of a uniform-random policy on the same
maze, because that is the only maze the game hands the AI control in (FR-040). A score on twenty
mazes the agent will never be handed control in answers nothing about the agent a player can switch
on. The rule that training may not draw seeds 1000..1019 goes with it: there is nothing left for it
to protect.

The reasoning below is kept because the *shape* of it still applies to any acceptance set this
project defines, and because `evaluate.py --maze generated` still uses this range when somebody asks
the generalisation question by hand.

FR-037 used to be measured over **20 full runs on the generated mazes of seeds 1000 to 1019**, the
figure being the mean of their final scores.

A contiguous range rather than a hand-picked list, deliberately. The maze generator decorrelates
neighbouring seeds — `test_maze_gen.c` asserts exactly that — so a range is as varied as a list,
and unlike a list it cannot be quietly tuned: changing the offset or the count is one visible line
in a diff, whereas swapping seed 7 for seed 8 because seed 7 is unkind looks like nothing at all.
The range starts at 1000 to stay clear of the low seeds the maze tests already use, so a
regression in the generator cannot be masked by a policy that has over-fitted to the same mazes.

Twenty runs is enough for the comparison FR-037 asks for, since the bar is a factor of ten over a
random policy rather than a few per cent. It is **not** enough to claim a small improvement over
another agent; a comparison that close needs a larger set and should say so at the time.

The random-policy baseline is re-measured by the same harness (VT-UNIT-010) rather than quoted from
this document — the 464.3-point figure here was taken over 329 episodes on generated mazes and
exists to justify the threshold, not to be compared against. On the normal maze the same harness
measures **433.5** over twenty episodes, which is close enough to leave FR-037's 4,600 where it is:
the bar was set at ten times random, and ten times random has not moved.

## 14 Play strength: what was measured, and what is being done

FR-037 is **not met yet**, and this section is the record of why rather than a promise about it.

The first full run of the curriculum, 150 genomes on four mazes per generation:

| | |
|---|---|
| stage 1 promoted after | **10** generations |
| stage 2 promoted after | **13** generations |
| stage 3, best fitness reached | ~3,500, at about generation 95 of 320, then flat |
| **on the acceptance seeds, stage 3** | **2,270 mean** — max 3,080, min 1,290, level 2 reached |
| the same harness's random baseline | **431 mean** |
| factor over random | **5.3**, where FR-037 asks for 10 |

The agent walks, eats and avoids ghosts. What it does not do is finish level 1 — it dies at
something like 85–95 % of it, which is exactly what a mean of 2,270 against a 2,440-point level
means.

**The limit is fitness noise, not compute, and that is visible in the log rather than inferred.**
Within one genome's four mazes the score ran from **1,180 to 5,570**. Fitness is the mean of four
such numbers, so it carried an uncertainty of several hundred points, and selection was therefore
deciding mostly on which mazes a genome happened to draw. Two things follow. Real ability is only
weakly rewarded — and a *deletion* that costs real ability is invisible, which is why networks fell
from their initial 92 connections to about a dozen while the score stopped moving. That is NEAT's
complexity pressure working exactly as designed against a fitness signal that cannot tell it apart
from luck.

So the change is aimed at the signal, not at the search
([DEC-044](../PrePlanning/11-Decisions-and-As-Built.md)):

| | was | is |
|---|---|---|
| mazes per genome | 4 | **12** — cuts the spread by about √3 |
| population | 150 | **250** |
| `conn_delete_prob` | 0.5 | **0.2** |
| `node_delete_prob` | 0.2 | **0.05** |
| stage-3 generations | 320 | **400** |

That is roughly seven times the work per generation, about ten hours on the two-core machine this
was developed on, and it runs overnight. The owner was offered a faster machine and declined it, and
was offered both cheaper alternatives — the single-maze fallback of §12 and lowering FR-037's
threshold, which the requirement itself marks tunable — and chose to keep the approach and the
requirement and pay for the run.

### 14.1 What happened instead: the noise was removed rather than blurred

That run was never made. The owner asked for something else first — both mazes playable, the AI
offered in the arcade one only ([DEC-045](../PrePlanning/11-Decisions-and-As-Built.md), FR-040) — and
it turns out to answer §14 as a side effect. Training now plays **one fixed maze** and the game has
nothing random in it, so a genome's score is a measurement:

| | 4 mazes | 12 mazes (planned) | the normal maze (now) |
|---|---|---|---|
| episodes per genome | 4 | 12 | **1** |
| spread within one genome's fitness | 1,180..5,570 | narrower by ~√3 | **none — there is one episode** |
| what selection is deciding on | mostly luck | mostly ability | **ability** |
| cost of a generation | 1 × | ~3 × | **~1/4 ×** (measured: 11 s → 2 s at stage 3) |

Twelve mazes would have *reduced* the noise at three times the cost. One fixed maze removes it at a
quarter of it. That is not a cleverer idea than DEC-044's — it is a different product, and the
cheaper measurement fell out of it.

The starting point is measured rather than assumed. The agent the firmware ships — trained on
generated mazes — plays the normal maze for **2,400 points** against **433.5** for a uniform-random
policy on the same maze: a factor of 5.5, where FR-037 asks for 10, and the same gap the generated
mazes showed. So the change of maze on its own buys nothing; what it buys is a fitness signal that
can tell ability from luck, and generations cheap enough to use a lot of.

### 14.2 What the retraining reached, and what it did not settle

Two time-budgeted runs, differing only in `--seed` — which is now the draw of the *search* rather
than of the mazes, since a run with the maze fixed has nothing else left to be lucky about:

| run | budget | best score | vs. random | FR-037 |
|---|---|---|---|---|
| the shipped agent, for reference | — | 2,400 | 5.5× | not met |
| `normal-seed1` | 90 min, 276 generations | **4,980** | **11.5×** | **met** |
| `normal-seed2` | 75 min, 320 generations | 4,260 | 9.8× | not met |

**FR-037 is met by the agent that ships** — seed 1's network, 35 nodes with 8 hidden and 19
connections, 334 bytes of tables, verified on the board by VT-INT-024 against the host. It clears
level 1 and dies in level 2.

**And the second run says the margin is luck as much as method.** 4,260 against 4,980 from the same
configuration and a different starting population is a spread of 15 %, straddling the threshold. So
the honest reading is: removing the fitness noise raised the *achievable* score from 2,400 to
somewhere around 4,500, and which side of 4,600 a given run lands on is not yet reliable. Two
samples cannot say more than that, and this document is not going to.

If a *reliable* margin is wanted rather than a passing one, the order to look in is unchanged: the
features (§12 point 2), then the expectimax reference agent §2 keeps in reserve as a teacher. Not the
threshold — that is the owner's to move, not this document's.

### 14.3 The jitter took the requirement back, and was right to

The owner then asked for the ghosts to be paced randomly (FR-044) — the house's dot counts, the
scatter/chase phases, the frightened window, the idle timer, each moved by up to two seconds or half
its nominal value. The same network, unchanged, was measured again:

| | one deterministic run | twenty jittered runs |
|---|---|---|
| `normal-seed1`, the shipped table | **4,980** | **2,197** mean (1,680..3,130) |
| uniform random, same game | 433.5 | 424.5 |
| FR-037 | met | **not met**, factor 5.2 |

**So the 4,980 was a trajectory, not a skill.** The agent had learned one sequence of turns that
worked against ghosts leaving the house on exactly known dots; move those dots by two and it scores
what it scored before any of this — about 2,200, which is where the generated-maze agent was as well.
That is a more useful thing to know than a passing number, and it is only knowable because the game
became stochastic. §14.2's worry about the margin was right and understated it.

Two consequences, both taken:

- **FR-037 is a mean over 20 runs again.** The single-episode form DEC-045 earned lasted an hour.
- **The objective is reshaped** (FR-036): +500 a ghost on top of the score, and an episode ends at
  the *first* life lost. The second is what makes dying cost anything at all — see
  [DEC-047](../PrePlanning/11-Decisions-and-As-Built.md) for why a flat penalty per life is a
  near-constant on a three-life run, and backwards on a run the idle rule ends.

**The retraining came out below what it replaced: 1,746.5 against 2,197.** And looking at *why* found
something the score alone does not say. The winning network:

```
nodes 29   hidden 2   connections 6
```

Six connections, against 23 inputs. **The agent is very nearly blind** and still scores 1,746, because
wandering a full maze pays. The log shows how it got there — 92 connections at the start, then 14, 18,
22, 19 — which is [DEC-044](../PrePlanning/11-Decisions-and-As-Built.md)'s diagnosis exactly: NEAT
deletes structure, and under a noisy fitness a deletion that costs real ability is invisible, so it
keeps deleting. FR-044's jitter put that noise back deliberately, and this is the bill.

That reorders everything §14.2 suggested. It is pointless to give the agent better eyes (§12 point 2)
while it uses six connections, and pointless to buy more compute for a search that spends it on
pruning.

### 14.4 A fixed topology, and 72 seconds that matched two hours

So the next thing tried is not a better observation and not more time: it is a search that **cannot**
prune itself blind. `Training/train_es.py` optimises a fixed 23-16-4 network — 452 numbers, every one
used on every decision — with a separable evolution strategy: a mean, one standard deviation per
dimension, ranked recombination with CMA-ES's log-weights. Deliberately *not* CMA-ES: the covariance
matrix is what that name is about, it wants numpy, numpy is not in the container, and a 452 x 452
eigendecomposition is not what this problem is short of.

Nothing in C changes. `Services/neural_net` already evaluates an arbitrary feed-forward graph, so a
dense network is a special case of what the firmware runs, and `export_c.py` writes it out unchanged —
measured at 2,860 bytes of tables against NEAT's 334, both far inside NFR-007. The episode, the
fitness, the ghost bonus and the curriculum are *imported* from `train.py` rather than restated, so
the two trainers remain comparable.

First measurement, and it is a strong hint rather than a result — **72 seconds of training per run**:

| | score | generations |
|---|---|---|
| the shipped NEAT table, hours of training | 2,197 | 276 |
| NEAT retrained for two hours | 1,746 | 317 |
| ~~`es-one-life`, 72 s~~ | ~~2,162~~ | 12 |
| ~~`es-whole-run`, 72 s~~ | ~~2,079~~ | 34 |
| ~~`es-whole-run-wide` (32 hidden), 72 s~~ | ~~2,133~~ | 34 |

**Those three rows are struck through because they do not measure what they say.** `train_es.py` kept
its best genome *across* the stages, and stage 1 has no ghosts and no power pellets: its networks
clear level after level and score tens of thousands, where a stage-3 network scores a couple of
thousand. Nothing after stage 1 could therefore ever beat the recorded best, and the winner file could
hold nothing but stage 1's — a **pellet-walker that has never seen a ghost**, measured against the
full game. The generation counts say so too: 12 and 34 are stage 1 promoting and stage 2 starting.
`train.py` resets per stage for exactly this reason, and the two had to agree.

Measured after the fix, on this four-core machine, both by `evaluate.py` on the acceptance seeds:

| | score |
|---|---|
| a stage-1 walker, 3 min, the old behaviour reproduced | 1,643 |
| a stage-3 network, 5 min, the same trainer fixed | **1,952** |
| `es-one-life`, **17 min**, through `campaign.py` | **2,396** |

So the honest reading of the struck-out rows is that **a network that only ever learned to eat scores
about 2,000 on this game** — which is worth knowing, and is the same reason §14.3's six-connection
network scored 1,746: wandering a full maze pays. It is not evidence about the evolution strategy,
and "a minute of the fixed topology reaches what NEAT reached in hours" was that artefact.

**The claim survives its evidence being wrong.** Seventeen minutes of the fixed topology on four
cores scores **2,396** — above the shipped table's 2,197 and above the three hours of NEAT that were
the whole of last night's 2,360, and it ate **40 ghosts across the twenty runs** where the shipped
table eats 6 and last night's 19. What is different is that the number now comes from a network
trained on the game it is measured on.

### 14.5 The night that ran for three hours out of twelve

The campaign of §14.4 was configured as four runs of three hours and started. One row came back.

**Three of the four never ran.** `train_es.py` shares `_write_winner` with `train.py`, and that helper
had just grown a line reading `arguments.no_deletion` — a flag only NEAT's parser has. Every ES run
died with an `AttributeError` on its first fitness improvement, seconds in. `campaign.py` checked
whether a winner file had appeared and not what the trainer's exit code was, so a crash and a run that
trained for three hours and found nothing produced the same line in the log. Nine hours of the machine
were idle, and the summary said only that three runs "produced no winner".

**The one run that did work is the best result so far, and its own experiment leaked.** With deletion
forbidden, NEAT kept 40 nodes and 22 connections and scored **2,360** against `normal-seed1`'s 1,746 —
the pruning diagnosis of §14.3 confirmed, with the spread cut from 555 to 313 as well. Against the
shipped table's 2,197 it is +163 over the same twenty seeds, which is 14 wins out of 20 and *not*
significant (paired t = 1.5). But `--no-deletion` had only closed one of two doors: `net.py` flattens
the **enabled** connections, and `enabled_mutate_rate = 0.01` switches about one connection a
generation off. Disabling is deletion under another name, which is why a run with deletion forbidden
still came out holding 22 of its initial 92. The flag now sets that rate to zero as well.

What the tooling learned from a wasted night, all of it aimed at making the *next* attempt cheap
enough to iterate on rather than at making it bigger:

- **A campaign's budget is an input and its runs share it.** `campaign.py --hours 1`, or
  `./dev.sh train --hours 1`, which runs it on this machine rather than in the container. The `hours`
  in each run's entry is a share, not an absolute.
- **A run the budget cannot pay for is dropped and says so**, rather than being shortened into a
  different experiment. Each run carries the least time it is worth starting with: about 14 s a
  generation for NEAT against 3 s for the ES on four cores, so an hour can pay for one ES run and
  cannot pay for NEAT at all.
- **The stages share a run's budget too** — 15 %, 30 %, and stage 3 gets the rest plus whatever the
  teaching stages promoted out of early. Without it stage 2 takes everything a short run has, since
  it does not reliably clear its promotion bar, and the winner comes from the wrong stage. That is
  survivable overnight and fatal in an hour.
- **A trainer that exits non-zero is reported as failed**, with the last lines of its log, rather
  than as a run that found nothing.

`./dev.sh train --hours 1` therefore plans one run — `es-one-life`, 0.95 h — and names the three it
cannot pay for along with the budget each would need (`--hours 1.6`, `2.6` and `6.9`). One clean
answer in an hour, rather than four runs of a quarter of one that all stop in stage 2.

**Two things to watch in the next run's log rather than assume.** The strategy's `sigma` halves about
every forty generations and is at its 0.01 floor well inside an hour, so a longer run may be buying
less than its length suggests. And with deletion *and* disabling forbidden, the best genome's
connection count still drifts down — 92 to 72 over fourteen generations where it used to be 92 to 26
over nineteen. Much slower, not stopped, and the remaining mechanism is not yet identified.

### 14.6 The night that found the wall: danger has to cost something before it kills you

Everything §14.5 left open was measured across five runs of four hours each, all on the 23 features,
all with restarts, all on the acceptance seeds at **100 episodes** rather than twenty. That change of
sample size is the first result: at n=20 the standard error is about 170 points against differences
of 150–350, and three conclusions drawn at that size had to be withdrawn. The figure FR-037 asks for
is a mean of twenty runs, which is fine as an acceptance gate and far too few to compare two agents
with.

| run | objective | score | max level | worst of 100 | over 2,600 |
|---|---|---|---|---|---|
| the shipped table, for reference | — | 2,254 | 1 | — | — |
| 23 features, 1 h, ghost bonus 500 | score + 500/ghost | 3,015 | 1 | 990 | 57 |
| 23 features, 1.5 h, ghost bonus 500 | the same | **2,171** | 1 | — | — |
| `arcade` | the plain arcade score | 2,753 | 1 | 670 | — |
| `arcade_lvl` | + 500 a finished level | **2,753** | 1 | 670 | — |
| **`arcade_danger`** | + 500 a level, **− 10 a decision spent in danger** | **3,035** | **2** | **1,940** | **77** |
| `danger_pop64` | the same, population 64 | 2,069 | 1 | 1,180 | — |

**The ghost bonus was costing score, and more training made it worse.** Two runs of the same
configuration, one twice as long as the other: 938 generations reached a *fitness* of 5,917 and a
*score* of 2,171, against 3,015 from a fifth of the search. The agent was doing exactly what it was
paid to do — 2.5 ghosts a run against 0.6 — and the thing being measured got worse for it. None of
the open-source Pac-Man agents this project was compared against pays anything for eating a ghost;
they penalise ghosts and never reward them. FR-036's bonus is gone.

**The level bonus was inert, and could not have been anything else.** `arcade_lvl` produced *the same
weights* as `arcade`, to the byte apart from the metadata that records what it was paid. The term is
`500 × (level − 1)` and no episode had ever finished a level, so it was identically zero and the
search followed an identical path. A reward for finishing a level cannot teach an agent to finish
one: there is no gradient until it has already happened by accident. It is kept because it is no
longer identically zero — level 2 is now reached — but it earned nothing here.

**The danger penalty is what moved the wall.** `env_api` counts the decisions taken with a
ghost that can kill within four cells, and fitness charges ten points for each. That is the one thing
every reference does and this project did not: the agent's whole lesson about danger used to be the
episode ending when it died — a single event, at the end, saying nothing about the twenty decisions
it spent walking beside a ghost beforehand.

What it bought is not in the mean. Against the best previous agent the paired difference is **+20
points, t = 0.12** — nothing. What changed is the shape of the distribution: the worst of a hundred
runs went from 990 to **1,940**, the runs clearing level 1's worth of points went from 57 to 77, and
for the first time in this project **an agent finished a level**. It stopped dying badly, which is a
different achievement from scoring well and the one that a run to level 21 is made of.

**Population 64 is worse than 32 at the same wall-clock**, by 966 points: twice the candidates is half
the generations, and with restarts already supplying the exploration, the generations are worth more.

Still true, and still the thing in the way: level 2 is not level 21, and the score is a long way from
FR-037's 4,600. What the night established is which levers move it — a dense, continuous cost for
being in danger — and which do not: a bigger population, a richer observation (§14.5), a sparse
terminal bonus, or paying the agent for the ghosts it eats.
