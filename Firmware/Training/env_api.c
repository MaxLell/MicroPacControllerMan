/*
 * env_api.c — see env_api.h.
 */

#include "env_api.h"

#include <stdlib.h>
#include <string.h>

#include "game.h"
#include "game_session.h"
#include "neural_net.h"
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

/*! \brief The network #env_set_net installed, with the arrays it points at owned here.
 *
 * Copied rather than borrowed from the caller: Python is free to let its own buffers be collected
 * the moment the call returns, and a network read out of freed memory would evaluate to something
 * plausible rather than crashing.
 */
typedef struct
{
    neural_net_t net;

    uint16_t* output_nodes;
    float* biases;
    uint16_t* connection_offsets;
    uint16_t* connection_sources;
    float* connection_weights;
} env_policy_net_t;

struct env_batch_s
{
    uint32_t count;
    env_game_t* games;

    env_policy_net_t policy;

    /*! \brief `true` while #env_use_random_policy is in force — the VT-UNIT-010 baseline. */
    bool is_random_policy;

    /*! \brief State of the baseline's generator. One per batch, not one per game, so that the
     *         games do not all take the same random walk. */
    uint32_t rng_state;
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

/*! \brief One decision, carried out — the only place the episode rules live.
 *
 * Both the batched Python interface and the C-side runner go through here, which is what lets
 * FR-037 compare a trained agent against the random baseline: the two differ in how the action
 * was chosen and in nothing else.
 *
 * \return points gained during this decision
 */
static uint32_t prv_step_one(env_game_t* const inout_entry, pacman_ai_action_e in_action)
{
    if (inout_entry->is_done)
    {
        return 0U;
    }

    msg_game_state_t state;
    game_get_state_message(&inout_entry->game, &state);

    const uint32_t score_before = game_get_score(&inout_entry->game);
    const uint8_t cell_column_before = state.pacman.column;
    const uint8_t cell_row_before = state.pacman.row;

    /* The action is relative, so it only means anything alongside the way Pacman is facing.
     * Turning it into a compass direction here is what keeps `game` unaware that an AI exists. */
    const direction_e direction = pacman_ai_action_to_direction(in_action, (direction_e)state.pacman.direction);

    game_set_direction(&inout_entry->game, direction);

    uint32_t elapsed_ms = 0U;
    bool has_arrived = false;

    while ((elapsed_ms < DECISION_TIMEOUT_MS) && !has_arrived)
    {
        game_tick(&inout_entry->game, STEP_MS);
        elapsed_ms += STEP_MS;

        if (game_get_state(&inout_entry->game) != GAME_STATE_RUNNING)
        {
            break;
        }

        game_get_state_message(&inout_entry->game, &state);
        has_arrived = (state.pacman.column != cell_column_before) || (state.pacman.row != cell_row_before);
    }

    const uint32_t gained = game_get_score(&inout_entry->game) - score_before;

    /* Idling is measured in what the score did, not in what was walked past: an agent that runs
     * in circles through cleared corridors is idling however busy it looks. */
    inout_entry->idle_ms = (gained > 0U) ? 0U : (inout_entry->idle_ms + elapsed_ms);

    const bool is_over = (game_get_state(&inout_entry->game) != GAME_STATE_RUNNING);
    const bool is_idle = (inout_entry->idle_ms >= IDLE_LIMIT_MS);

    inout_entry->is_done = is_over || is_idle;

    return gained;
}

/*! \brief Let go of whatever #env_set_net copied. */
static void prv_release_policy(env_policy_net_t* const inout_policy)
{
    free(inout_policy->output_nodes);
    free(inout_policy->biases);
    free(inout_policy->connection_offsets);
    free(inout_policy->connection_sources);
    free(inout_policy->connection_weights);

    const env_policy_net_t empty = {0};
    *inout_policy = empty;
}

/*! \brief Copy `in_bytes` from `in_source`, or return `NULL` if there is nothing to copy. */
static void* prv_duplicate(const void* in_source, size_t in_bytes)
{
    if ((in_source == NULL) || (in_bytes == 0U))
    {
        return NULL;
    }

    void* const copy = malloc(in_bytes);

    if (copy != NULL)
    {
        memcpy(copy, in_source, in_bytes);
    }

    return copy;
}

/*! \brief The baseline's generator: xorshift32, written out here rather than taken from `rand()`.
 *
 * `rand()` differs between C libraries, and a baseline that cannot be reproduced on another
 * machine is not a baseline. This one is four lines and the same everywhere.
 */
static uint32_t prv_next_random(uint32_t* const inout_state)
{
    uint32_t value = *inout_state;

    value ^= value << 13U;
    value ^= value >> 17U;
    value ^= value << 5U;

    *inout_state = value;

    return value;
}

/*! \brief What the installed policy would do about one game's current state. */
static pacman_ai_action_e prv_choose_action(env_batch_t* const inout_batch, const env_game_t* const in_entry)
{
    if (inout_batch->is_random_policy)
    {
        return (pacman_ai_action_e)(prv_next_random(&inout_batch->rng_state) % (uint32_t)PACMAN_AI_ACTION_COUNT);
    }

    msg_game_state_t state;
    float features[PACMAN_AI_FEATURE_COUNT];
    float outputs[PACMAN_AI_ACTION_COUNT];

    game_get_state_message(&in_entry->game, &state);
    pacman_ai_get_features(&state, game_get_playfield(&in_entry->game), features);
    neural_net_evaluate(&inout_batch->policy.net, features, outputs);

    return pacman_ai_choose_action(outputs);
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

uint32_t env_max_decisions(void)
{
    return ENV_MAX_DECISIONS;
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

    prv_release_policy(&inout_batch->policy);
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
        pacman_ai_get_features(&state, game_get_playfield(&entry->game), &out_features[index * stride]);
    }
}

void env_step(env_batch_t* inout_batch, const uint8_t* in_actions, float* out_reward, uint8_t* out_done)
{
    for (uint32_t index = 0U; index < inout_batch->count; ++index)
    {
        env_game_t* const entry = &inout_batch->games[index];
        const uint32_t gained = prv_step_one(entry, (pacman_ai_action_e)in_actions[index]);

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

bool env_set_net(env_batch_t* inout_batch, uint16_t in_input_count, uint16_t in_node_count, uint16_t in_output_count,
                 const uint16_t* in_output_nodes, const float* in_biases, const uint16_t* in_connection_offsets,
                 const uint16_t* in_connection_sources, const float* in_connection_weights,
                 uint32_t in_connection_count)
{
    if (inout_batch == NULL)
    {
        return false;
    }

    prv_release_policy(&inout_batch->policy);
    inout_batch->is_random_policy = false;

    /* The observation and the action set are the firmware's, so a network shaped for anything else
     * is a mistake in the exporter and not something to discover halfway through a generation. */
    if ((in_input_count != (uint16_t)PACMAN_AI_FEATURE_COUNT) || (in_output_count != (uint16_t)PACMAN_AI_ACTION_COUNT))
    {
        return false;
    }

    if ((in_node_count == 0U) || (in_node_count > NEURAL_NET_MAX_NODES))
    {
        return false;
    }

    /* The offsets are `node_count + 1` long and the last one is the total, so a caller that
     * disagrees with itself about how many connections there are is caught here rather than by
     * reading past the end of an array. */
    if ((in_connection_offsets == NULL) || (in_connection_offsets[in_node_count] != in_connection_count))
    {
        return false;
    }

    env_policy_net_t* const policy = &inout_batch->policy;

    policy->output_nodes = prv_duplicate(in_output_nodes, (size_t)in_output_count * sizeof(uint16_t));
    policy->biases = prv_duplicate(in_biases, (size_t)in_node_count * sizeof(float));
    policy->connection_offsets = prv_duplicate(in_connection_offsets, ((size_t)in_node_count + 1U) * sizeof(uint16_t));
    policy->connection_sources = prv_duplicate(in_connection_sources, (size_t)in_connection_count * sizeof(uint16_t));
    policy->connection_weights = prv_duplicate(in_connection_weights, (size_t)in_connection_count * sizeof(float));

    const bool is_allocated = (policy->output_nodes != NULL) && (policy->biases != NULL)
                              && (policy->connection_offsets != NULL)
                              && ((in_connection_count == 0U)
                                  || ((policy->connection_sources != NULL) && (policy->connection_weights != NULL)));

    if (!is_allocated)
    {
        prv_release_policy(policy);

        return false;
    }

    policy->net.input_count = in_input_count;
    policy->net.node_count = in_node_count;
    policy->net.output_count = in_output_count;
    policy->net.output_nodes = policy->output_nodes;
    policy->net.biases = policy->biases;
    policy->net.connection_offsets = policy->connection_offsets;
    policy->net.connection_sources = policy->connection_sources;
    policy->net.connection_weights = policy->connection_weights;

    /* The firmware's own acceptance test, run against every genome of every generation. A topology
     * the port could not evaluate therefore never gets a fitness in the first place. */
    if (!neural_net_is_well_formed(&policy->net))
    {
        prv_release_policy(policy);

        return false;
    }

    return true;
}

void env_use_random_policy(env_batch_t* inout_batch, uint32_t in_rng_seed)
{
    prv_release_policy(&inout_batch->policy);

    inout_batch->is_random_policy = true;

    /* xorshift32 is stuck at zero, so a zero seed would give a policy that always chooses
     * `forward` and call itself random. */
    inout_batch->rng_state = (in_rng_seed == 0U) ? 0x9E3779B9U : in_rng_seed;
}

void env_run(env_batch_t* inout_batch, const uint32_t* in_seeds, uint8_t in_stage, uint32_t* out_scores,
             uint32_t* out_steps, uint8_t* out_levels)
{
    env_reset(inout_batch, in_seeds, in_stage);

    /* One game played out at a time rather than all of them in lockstep. Its `game_t` is 15 kB, so
     * finishing one before touching the next keeps the working set to that one game; and it makes
     * the random baseline reproducible, because the order in which the games draw from the one
     * generator is fixed. */
    for (uint32_t index = 0U; index < inout_batch->count; ++index)
    {
        env_game_t* const entry = &inout_batch->games[index];
        uint32_t decisions = 0U;

        while (!entry->is_done && (decisions < ENV_MAX_DECISIONS))
        {
            (void)prv_step_one(entry, prv_choose_action(inout_batch, entry));
            ++decisions;
        }

        if (out_scores != NULL)
        {
            out_scores[index] = game_get_score(&entry->game);
        }

        if (out_steps != NULL)
        {
            out_steps[index] = decisions;
        }

        if (out_levels != NULL)
        {
            out_levels[index] = game_get_level(&entry->game);
        }
    }
}
