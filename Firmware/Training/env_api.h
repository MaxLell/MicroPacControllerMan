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

#include <stdint.h>

/* ==========================================================================
 * env_api - public types
 * ========================================================================= */

/*! \brief A set of independent games, stepped together. Opaque to the caller. */
typedef struct env_batch_s env_batch_t;

/*! \brief Which rules a run plays under — the curriculum of [M6 §6](../../Docu/Design/M6-Pacman-AI.md).
 *
 * Stage 3 is the real game, and it is the only stage FR-037 may be measured at: the earlier ones
 * score lower because they have fewer things worth points in them, so their fitness is not
 * comparable with it.
 */
typedef enum
{
    ENV_STAGE_MAZE_ONLY = 1, /*!< No ghosts, no power pellets: learn to walk and eat  */
    ENV_STAGE_GHOSTS = 2,    /*!< Ghosts hunt, no power pellets: a ghost is only death */
    ENV_STAGE_FULL = 3       /*!< The game as it ships                                */
} env_stage_e;

/* ==========================================================================
 * env_api - public API
 * ========================================================================= */

/*! \brief How many features one observation holds. Asked rather than assumed, so Python cannot
 *         disagree with the firmware about it. */
uint32_t env_feature_count(void);

/*! \brief How many actions there are, likewise. */
uint32_t env_action_count(void);

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
 * \param[in]       in_seeds: one maze seed per game, `count` long
 * \param[in]       in_stage: a \ref env_stage_e; anything else is treated as #ENV_STAGE_FULL
 */
void env_reset(env_batch_t* inout_batch, const uint32_t* in_seeds, uint8_t in_stage);

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

#endif /* ENV_API_H */
