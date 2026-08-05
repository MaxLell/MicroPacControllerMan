# M6 — Pacman AI

[← Back to Index](../PrePlanning/Index.md) · Requirements: [02 §2.1.11](../PrePlanning/02-Requirements.md) ·
Milestone: [04 §4.4](../PrePlanning/04-Implementation-Phases-and-Milestones.md#44-milestone-6--pacman-ai)

The *how* behind FR-030..039 and FR-112..114: an agent trained on the host that plays Pacman on
the board. The requirements say what it must do; this document says how it is built, and it is
the place for every number, tool choice and trap.

> **Design, not as-built.** Nothing here is implemented yet. Figures marked **measured** were
> taken on this machine against the shipped game; everything else is a budget or an estimate and
> says so.

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

Fitness is the run's score (FR-036), with one addition the transcript in the resources is emphatic
about: **an idle agent must die.** Code Bullet's first stage killed a Pacman that ate nothing
within a time limit, and without that an agent discovers that hiding in a corner beats playing.
Our game has `dots_idle_ms`, but it only releases ghosts from the house — it does not end a run.
The **training harness** therefore ends an episode when no pellet has been eaten for a set
interval. That is a harness rule, not a game rule: nothing about the shipped firmware changes.

The transcript's other lesson is a three-stage curriculum — first no ghosts and no power pellets,
then ghosts, then power pellets — the point being that "sometimes I may eat a ghost and sometimes
it kills me" is an ambiguity a fresh network cannot resolve while it is still learning to walk.

That staging changes the *rules*, and the rules are the shipped game (FR-112). So the plan is:

1. **First attempt: shape the fitness, not the rules.** Early generations score pellets and
   ignore death; later ones penalise it; finally fitness is the score. No game code changes.
2. **If that stalls: stage the rules.** Removing the power pellets needs no new code at all —
   `game_start_on_map` already takes a map, so the harness can hand one with the power pellets
   turned into ordinary pellets. Removing the *ghosts* has no such route and would need a small,
   explicitly training-only seam in `game`. That is deliberately deferred until it is proven
   necessary, because it is the one change here that touches shipped logic.

Honest about the difference: fitness shaping does not remove the rule ambiguity, only the
pressure to resolve it early. If step 1 plateaus well under FR-037, step 2 is the reason.

## 7 The training harness

Python drives C over `ctypes` against a shared library built from the same sources as the
firmware:

```
Firmware/Training/env_api.c      -> libpacman_env.so, links pacman_host
Firmware/Training/train.py       -> neat-python, the evolution loop
Firmware/Training/export_c.py    -> the evolved winner -> ai_weights.c
```

`Firmware/Training/` is a new top-level folder in the firmware tree and therefore an amendment to
[03 §3.9](../PrePlanning/03-Architecture.md#39-firmware-source-tree-layout) — recorded as
[DEC-040](../PrePlanning/11-Decisions-and-As-Built.md). It builds only in the host
configuration and nothing in the target build refers to it.

The API is **batched on purpose**: one call steps *every* environment, rather than one call per
environment. At 150 genomes and ~15 k steps/s each, per-call FFI overhead would otherwise become
the bottleneck rather than the simulation.

```c
env_batch_t* env_create(uint32_t count);
void         env_reset(env_batch_t*, const uint32_t* seeds);
void         env_step(env_batch_t*, const uint8_t* relative_actions,
                      float* out_features, float* out_fitness_delta, uint8_t* out_done);
void         env_destroy(env_batch_t*);
```

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
`ott ai_equivalence` replays them on the target and compares (VT-INT-024). The states must cover
ordinary play, frightened mode, a tunnel and a life just lost — the four places where the feature
extractor has something interesting to do.

## 10 Fitting the target

The room available, **measured** on the current firmware: **419,804 bytes of flash free** (96,292
of 516,096 used) and **83,900 bytes of RAM free** (178,244 of 262,144), plus the 16 kB SRAM4 that
nothing uses. Of the 16 ms frame, about 8 ms is unused.

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

The player-facing half is small and is fully specified by FR-030..034. Three notes on the
mechanics:

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

1. **FR-037's absolute threshold is set, its seed set is not.** The floor is 4,600 points — ten
   times the **measured** random-policy mean of 464.3 (median 440, best 1,440, over 329 episodes).
   For scale, clearing level 1 is about 2,600 points before any ghost is eaten. Which seeds the
   acceptance set uses, and how many, is still open; it must be fixed and written down before
   training starts, or the number can be gamed by choosing kind mazes.
2. **Whether fitness shaping is enough**, or the rule-staged curriculum of §6 is needed — and with
   it a training-only seam in `game`. Unknown until the first campaign runs.
3. **`-O2` for the training environment.** The 15,429 steps/s was measured against the `-g` host
   library. The training build should optimise; how much that buys is unmeasured.
4. **Whether 23 features are the right 23.** They are a considered starting point, not a result.
   The linked projects got by with 8.
