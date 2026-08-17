/*
 * env_api.h
 *
 * The training environment as Python sees it
 * ([M6 §7](../../Docu/Design/M6-Pacman-AI.md), FR-112/113/114,
 * [DEC-040](../../Docu/PrePlanning/11-Decisions-and-As-Built.md)).
 *
 * A thin shell around the firmware's own `game_t` and `pacman_ai`. It adds no rules of its own
 * except the two that belong to *training* rather than to the game: an episode ends if nothing
 * is eaten for a while, and a decision is asked for once per cell rather than once per frame.
 *
 * **Everything is batched.** One call steps every environment, because at a population of 150
 * and ~15,000 steps a second each, per-call overhead across the language boundary would cost
 * more than the simulation does. The arrays a caller passes are therefore `count`-long, and the
 * feature array is `count * env_feature_count()` long, environment-major.
 *
 * This is host-only code and it is not firmware: it may allocate, and nothing in the target
 * build refers to it. The dependency runs one way (CON-105) — the harness knows about the game,
 * the game knows nothing about the harness.
 */

#ifndef ENV_API_H
#define ENV_API_H

#include <stdbool.h>
#include <stdint.h>

/* ==========================================================================
 * env_api - public types
 * ========================================================================= */

/*! \brief A set of independent games, stepped together. Opaque to the caller. */
typedef struct env_batch_s env_batch_t;

/*! \brief Most decisions one episode may take before #env_run gives up on it.
 *
 * A backstop, not a budget: the idle rule ends a run that stops scoring, so only an agent that
 * keeps eating just often enough to stay alive reaches this. Twenty thousand decisions is several
 * cleared levels.
 */
#define ENV_MAX_DECISIONS (20000U)

/*! \brief Which rules a run plays under — the curriculum of [M6 §6](../../Docu/Design/M6-Pacman-AI.md).
 *
 * Stage 3 is the real game, and it is the only stage a whole-game score may be read at: the earlier ones
 * score lower because they have fewer things worth points in them, so their fitness is not
 * comparable with it.
 */
typedef enum
{
    ENV_STAGE_MAZE_ONLY = 1, /*!< No ghosts, no power pellets: learn to walk and eat  */
    ENV_STAGE_GHOSTS = 2,    /*!< Ghosts hunt, no power pellets: a ghost is only death */
    ENV_STAGE_FULL = 3       /*!< The game as it ships                                */
} env_stage_e;

/*! \brief Which of the game's two mazes a run is played on.
 *
 * The shipped game offers both (FR-040), but the AI may only be handed control in the normal
 * one — so #ENV_MAZE_NORMAL is where training and the measured score belong, and the seeds passed alongside
 * it are ignored: there is one normal maze and every level of a run plays it.
 *
 * #ENV_MAZE_GENERATED stays because the question "how does this agent do on a maze nobody has
 * ever played" is still worth being able to ask, even though nothing gates on the answer any more.
 */
typedef enum
{
    ENV_MAZE_NORMAL = 0,   /*!< The arcade's own layout — the maze the AI plays (FR-040) */
    ENV_MAZE_GENERATED = 1 /*!< A maze generated from the seed (FR-029)                  */
} env_maze_e;

/* ==========================================================================
 * env_api - public API
 * ========================================================================= */

/*! \brief How many features one observation holds. Asked rather than assumed, so Python cannot
 *         disagree with the firmware about it. */
uint32_t env_feature_count(void);

/*! \brief How many actions there are, likewise. */
uint32_t env_action_count(void);

/*! \brief #ENV_MAX_DECISIONS, asked for rather than repeated on the Python side. */
uint32_t env_max_decisions(void);

/*! \brief Simulated milliseconds an episode may go without anything being eaten.
 *
 * Training's own rule, not the game's: without it an agent discovers that hiding in a corner
 * outlives playing, which is the first thing Code Bullet's transcript warns about. It stays out
 * of `game` because the firmware has no use for it.
 */
uint32_t env_idle_limit_ms(void);

/*! \brief Create `in_count` independent games. Returns `NULL` if `in_count` is 0 or on failure. */
env_batch_t* env_create(uint32_t in_count);

/*! \brief Release a batch. Safe on `NULL`. */
void env_destroy(env_batch_t* inout_batch);

/*! \brief Start every game afresh.
 *
 * \param[in,out]   inout_batch: the batch
 * \param[in]       in_seeds: one seed per game, `count` long. It seeds the game's timing jitter
 *                      (FR-044) always, and the maze when `in_maze` is #ENV_MAZE_GENERATED
 * \param[in]       in_stage: a \ref env_stage_e; anything else is treated as #ENV_STAGE_FULL
 * \param[in]       in_maze: a \ref env_maze_e; anything else is treated as #ENV_MAZE_NORMAL
 */
void env_reset(env_batch_t* inout_batch, const uint32_t* in_seeds, uint8_t in_stage, uint8_t in_maze);

/*! \brief Describe every game.
 *
 * \param[in]       in_batch: the batch
 * \param[out]      out_features: `count * env_feature_count()` floats, environment-major
 */
void env_observe(const env_batch_t* in_batch, float* out_features);

/*! \brief Advance every running game to its next decision.
 *
 * A step is "until Pacman reaches another cell", not "one frame": he can only turn on a cell
 * boundary, so a decision inside one would be thrown away. A game that has already finished is
 * left alone and reports `0` reward and `done`.
 *
 * Pacman held against a wall never reaches another cell, so a step also gives up after a bounded
 * stretch of simulated time and lets the agent choose again. Without that the loop would hang on
 * the first agent that walks into a corner.
 *
 * \param[in,out]   inout_batch: the batch
 * \param[in]       in_actions: one \ref pacman_ai_action_e per game, `count` long
 * \param[out]      out_reward: points gained by each game during this step, `count` long
 * \param[out]      out_done: non-zero where the episode has ended, `count` long
 */
void env_step(env_batch_t* inout_batch, const uint8_t* in_actions, float* out_reward, uint8_t* out_done);

/*! \brief Each game's score so far — the fitness FR-036 asks for. `count` long. */
void env_scores(const env_batch_t* in_batch, uint32_t* out_scores);

/*! \brief Each game's level, for reporting how far a genome actually got. `count` long. */
void env_levels(const env_batch_t* in_batch, uint8_t* out_levels);

/*! \brief Ghosts each game has eaten. `count` long.
 *
 * Reported separately from the score although the score already pays the arcade's 200/400/800/1600
 * for them, because training pays **more** than that (FR-036): the owner wants eating a ghost to be
 * worth learning, and a fitness that only saw the score could not weight it differently from the
 * pellets that produced the same points.
 */
/*! \brief Decisions each game took while a ghost that could kill was within four cells.
 *
 * What a fitness charges for danger, rather than only for dying. See the field it reads.
 *
 * \param[in]       in_batch: the batch
 * \param[out]      out_danger_decisions: `count` long
 */
void env_danger_decisions(const env_batch_t* in_batch, uint32_t* out_danger_decisions);

void env_ghosts_eaten(const env_batch_t* in_batch, uint16_t* out_ghosts_eaten);

/*! \brief End every episode at the first life lost, rather than when the run is over (FR-036).
 *
 * Training's rule and not the game's: the shipped game always has its three lives. It is what makes
 * dying cost something — a flat penalty per life would be almost a constant, since a run ends
 * *because* the lives are gone, and where it is not constant it rewards the run that stopped early
 * by idling.
 *
 * Off by default, because a reported score is what a **run** scores and a run has three lives. Evaluation
 * therefore leaves it alone and training turns it on.
 *
 * \param[in,out]   inout_batch: the batch
 * \param[in]       in_ends_at_first_death: `true` to stop at the first death
 */
void env_set_episode_ends_at_first_death(env_batch_t* inout_batch, bool in_ends_at_first_death);

/* ==========================================================================
 * env_api - playing a whole episode without leaving C
 * ========================================================================= */

/*! \brief Install the network the batch plays with.
 *
 * The arrays are the ones `Services/neural_net` evaluates and the ones `export_c.py` will emit;
 * they are handed over flat rather than as a struct so that Python never has to mirror a C
 * layout — the thing that would silently disagree after an innocent-looking edit to the header.
 * They are **copied**, so the caller may free its own.
 *
 * The network is put through `neural_net_is_well_formed` and refused if it fails, which means
 * every genome of every generation is checked by the same function the firmware trusts at
 * start-up. A topology the port could not evaluate therefore cannot be trained in the first
 * place.
 *
 * \param[in,out]   inout_batch: the batch
 * \param[in]       in_connection_count: entries in the two connection arrays, which is also
 *                  what the last offset must equal
 * \return          `true` when the network was accepted and installed
 */
bool env_set_net(env_batch_t* inout_batch, uint16_t in_input_count, uint16_t in_node_count, uint16_t in_output_count,
                 const uint16_t* in_output_nodes, const float* in_biases, const uint16_t* in_connection_offsets,
                 const uint16_t* in_connection_sources, const float* in_connection_weights,
                 uint32_t in_connection_count);

/*! \brief Play uniformly at random instead of with a network — the baseline of VT-UNIT-010.
 *
 * Deliberately a *mode of the same runner* rather than a loop of its own in Python. The score is a
 * comparison against this baseline, so the two figures have to come out of one harness; if the
 * baseline had its own episode loop, a difference between the loops would look like a difference
 * between the policies.
 *
 * \param[in,out]   inout_batch: the batch
 * \param[in]       in_rng_seed: reproducible — same seed, same episode (FR-114)
 */
void env_use_random_policy(env_batch_t* inout_batch, uint32_t in_rng_seed);

/*! \brief Play every game to its end and report what happened.
 *
 * This is the call training spends its time in: one call per genome rather than one per decision,
 * because the observation, the network and the choice of action are all C — the same C the
 * firmware runs (FR-039). Nothing about the policy lives in Python, so there is no second
 * implementation to keep in step, and the network that was trained is the network that was
 * exported.
 *
 * A game runs until it is over, until the idle rule ends it, or until #ENV_MAX_DECISIONS have
 * been taken — the last of which is reported through `out_steps` so that a run cut short is
 * visible rather than looking like a policy that stopped scoring.
 *
 * \param[in,out]   inout_batch: the batch, with a policy already installed
 * \param[in]       in_seeds: one maze seed per game, `count` long; ignored for #ENV_MAZE_NORMAL
 * \param[in]       in_stage: a \ref env_stage_e
 * \param[in]       in_maze: a \ref env_maze_e
 * \param[out]      out_scores: final score of each game, `count` long, may be `NULL`
 * \param[out]      out_steps: decisions each game took, `count` long, may be `NULL`
 * \param[out]      out_levels: level each game reached, `count` long, may be `NULL`
 * \param[out]      out_ghosts_eaten: ghosts each game ate, `count` long, may be `NULL`
 */
void env_run(env_batch_t* inout_batch, const uint32_t* in_seeds, uint8_t in_stage, uint8_t in_maze,
             uint32_t* out_scores, uint32_t* out_steps, uint8_t* out_levels, uint16_t* out_ghosts_eaten);

#endif /* ENV_API_H */
