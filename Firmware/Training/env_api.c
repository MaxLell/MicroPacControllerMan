/*
 * env_api.c — see env_api.h.
 */

#include "env_api.h"

#include <stdlib.h>

#include "game.h"
#include "game_session.h"
#include "pacman_ai.h"

/* ==========================================================================
 * env_api - private types and data
 * ========================================================================= */

/*! \brief The simulation slice fed to `game_tick`.
 *
 * The frame the game is really played at, taken from the game rather than chosen here: an agent
 * trained against a different time step would be trained against a different game, because
 * Pacman's and the ghosts' speeds are expressed in milliseconds per cell.
 */
#define STEP_MS             (GAME_SESSION_FRAME_PERIOD_MS)

/*! \brief Longest a single decision may take before the agent is asked again.
 *
 * Reached only when Pacman is not moving — held against a wall by an intent he cannot follow. He
 * would then never arrive anywhere and the step would never return.
 */
#define DECISION_TIMEOUT_MS (500U)

/*! \brief Simulated milliseconds without eating that end an episode. See the header. */
#define IDLE_LIMIT_MS       (10000U)

typedef struct
{
    game_t game;

    /*! \brief Since the last time the score moved. Resetting it is what "something was eaten"
     *         means here — the score is the only signal the harness needs for it, which is why
     *         the game itself did not have to grow an idle rule. */
    uint32_t idle_ms;

    bool is_done;
} env_game_t;

struct env_batch_s
{
    uint32_t count;
    env_game_t* games;
};

/* ==========================================================================
 * env_api - private functions
 * ========================================================================= */

static void prv_config_for_stage(uint8_t in_stage, game_config_t* const out_config)
{
    game_get_default_config(out_config);

    switch ((env_stage_e)in_stage)
    {
        case ENV_STAGE_MAZE_ONLY:
            out_config->has_ghosts = false;
            out_config->has_power_pellets = false;
            break;

        case ENV_STAGE_GHOSTS: out_config->has_power_pellets = false; break;

        case ENV_STAGE_FULL:
        default: break;
    }
}

/* ==========================================================================
 * env_api - public functions
 * ========================================================================= */

uint32_t env_feature_count(void)
{
    return (uint32_t)PACMAN_AI_FEATURE_COUNT;
}

uint32_t env_action_count(void)
{
    return (uint32_t)PACMAN_AI_ACTION_COUNT;
}

uint32_t env_idle_limit_ms(void)
{
    return IDLE_LIMIT_MS;
}

env_batch_t* env_create(uint32_t in_count)
{
    if (in_count == 0U)
    {
        return NULL;
    }

    env_batch_t* const batch = calloc(1U, sizeof(env_batch_t));

    if (batch == NULL)
    {
        return NULL;
    }

    batch->games = calloc((size_t)in_count, sizeof(env_game_t));

    if (batch->games == NULL)
    {
        free(batch);

        return NULL;
    }

    batch->count = in_count;

    for (uint32_t index = 0U; index < in_count; ++index)
    {
        game_init(&batch->games[index].game);
    }

    return batch;
}

void env_destroy(env_batch_t* inout_batch)
{
    if (inout_batch == NULL)
    {
        return;
    }

    free(inout_batch->games);
    free(inout_batch);
}

void env_reset(env_batch_t* inout_batch, const uint32_t* in_seeds, uint8_t in_stage)
{
    game_config_t config;

    prv_config_for_stage(in_stage, &config);

    for (uint32_t index = 0U; index < inout_batch->count; ++index)
    {
        env_game_t* const entry = &inout_batch->games[index];

        game_init(&entry->game);
        game_start_configured(&entry->game, in_seeds[index], &config);

        entry->idle_ms = 0U;
        entry->is_done = false;
    }
}

void env_observe(const env_batch_t* in_batch, float* out_features)
{
    const uint32_t stride = env_feature_count();

    for (uint32_t index = 0U; index < in_batch->count; ++index)
    {
        const env_game_t* const entry = &in_batch->games[index];
        msg_game_state_t state;

        game_get_state_message(&entry->game, &state);
        pacman_ai_get_features(&state, &entry->game.playfield, &out_features[index * stride]);
    }
}

void env_step(env_batch_t* inout_batch, const uint8_t* in_actions, float* out_reward, uint8_t* out_done)
{
    for (uint32_t index = 0U; index < inout_batch->count; ++index)
    {
        env_game_t* const entry = &inout_batch->games[index];

        if (entry->is_done)
        {
            out_reward[index] = 0.0F;
            out_done[index] = 1U;
            continue;
        }

        msg_game_state_t state;
        game_get_state_message(&entry->game, &state);

        const uint32_t score_before = game_get_score(&entry->game);
        const uint8_t cell_column_before = state.pacman.column;
        const uint8_t cell_row_before = state.pacman.row;

        /* The action is relative, so it only means anything alongside the way Pacman is facing.
         * Turning it into a compass direction here is what keeps `game` unaware that an AI
         * exists. */
        const direction_e direction =
            pacman_ai_action_to_direction((pacman_ai_action_e)in_actions[index], (direction_e)state.pacman.direction);

        game_set_direction(&entry->game, direction);

        uint32_t elapsed_ms = 0U;
        bool has_arrived = false;

        while ((elapsed_ms < DECISION_TIMEOUT_MS) && !has_arrived)
        {
            game_tick(&entry->game, STEP_MS);
            elapsed_ms += STEP_MS;

            if (game_get_state(&entry->game) != GAME_STATE_RUNNING)
            {
                break;
            }

            game_get_state_message(&entry->game, &state);
            has_arrived = (state.pacman.column != cell_column_before) || (state.pacman.row != cell_row_before);
        }

        const uint32_t gained = game_get_score(&entry->game) - score_before;

        /* Idling is measured in what the score did, not in what was walked past: an agent that
         * runs in circles through cleared corridors is idling however busy it looks. */
        entry->idle_ms = (gained > 0U) ? 0U : (entry->idle_ms + elapsed_ms);

        const bool is_over = (game_get_state(&entry->game) != GAME_STATE_RUNNING);
        const bool is_idle = (entry->idle_ms >= IDLE_LIMIT_MS);

        entry->is_done = is_over || is_idle;

        out_reward[index] = (float)gained;
        out_done[index] = entry->is_done ? 1U : 0U;
    }
}

void env_scores(const env_batch_t* in_batch, uint32_t* out_scores)
{
    for (uint32_t index = 0U; index < in_batch->count; ++index)
    {
        out_scores[index] = game_get_score(&in_batch->games[index].game);
    }
}

void env_levels(const env_batch_t* in_batch, uint8_t* out_levels)
{
    for (uint32_t index = 0U; index < in_batch->count; ++index)
    {
        out_levels[index] = game_get_level(&in_batch->games[index].game);
    }
}
