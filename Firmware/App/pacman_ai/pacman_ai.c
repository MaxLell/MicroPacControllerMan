/*
 * pacman_ai.c — see pacman_ai.h.
 */

#include "pacman_ai.h"

#include <stddef.h>

#include "ai_weights.h"
#include "custom_assert.h"
#include "neural_net.h"

/* ==========================================================================
 * pacman_ai - private types and data
 * ========================================================================= */

#define CELL_COUNT     (PLAYFIELD_WIDTH * PLAYFIELD_HEIGHT)

/*! \brief What a cell's `first_step` holds before the search reaches it. */
#define STEP_UNREACHED ((uint8_t)PACMAN_AI_ACTION_COUNT)

/*! \brief The compass directions, in the order the search fans out.
 *
 * Fixed on purpose. Two cells can be the same distance away, and which of them the search
 * settles on decides a feature — so the order has to be part of the algorithm rather than of
 * the enum's spelling, or the host and the target could disagree (FR-039).
 */
static const direction_e g_search_order[PACMAN_AI_ACTION_COUNT] = {DIRECTION_NORTH, DIRECTION_SOUTH, DIRECTION_EAST,
                                                                   DIRECTION_WEST};

/* Breadth-first scratch space, file-scope rather than on the stack: NFR-008 wants the target's
 * buffers reserved statically, and this is 4 kB of them. See the reentrancy note in the header. */
static uint16_t g_distance[CELL_COUNT];
static uint8_t g_first_step[CELL_COUNT];
static uint16_t g_queue[CELL_COUNT];

/* ==========================================================================
 * pacman_ai - private functions
 * ========================================================================= */

static uint16_t prv_cell_index(cell_t in_cell)
{
    return (uint16_t)(((uint16_t)in_cell.y * PLAYFIELD_WIDTH) + (uint16_t)in_cell.x);
}

static cell_t prv_make_cell(int16_t in_x, int16_t in_y)
{
    const cell_t cell = {in_x, in_y};

    return cell;
}

/* Turn a cell count into the feature's `[0, 1]`, saturating at #PACMAN_AI_DISTANCE_NONE. */
static float prv_scale_distance(uint16_t in_distance)
{
    if (in_distance >= (uint16_t)PACMAN_AI_DISTANCE_SCALE)
    {
        return PACMAN_AI_DISTANCE_NONE;
    }

    return (float)in_distance / (float)PACMAN_AI_DISTANCE_SCALE;
}

/* Walk the open cells outwards from Pacman, recording for each how far it is and which of his
 * four neighbours the shortest way to it begins with.
 *
 * One search answers every distance question in the observation: the caller then only has to
 * look up the nearest cell of each kind. Doing it per question would be five searches for the
 * same information.
 *
 * The gate is closed to Pacman (§10.4), so the search must not walk through the ghost house —
 * otherwise a ghost sitting in there would look reachable and the agent would be told to chase
 * a target it can never get to.
 *
 * A consequence worth knowing: a ghost still inside the house has no route to Pacman at all, so
 * it reports as absent and appears the instant it steps off the gate. The player sees it waiting
 * and the agent does not. That is a real difference and it is accepted — a *distance* through a
 * wall Pacman may not cross would be a fiction, and the alternative encodings all amount to
 * telling the agent something it cannot act on.
 */
static void prv_search_from(const playfield_t* const in_playfield, cell_t in_origin)
{
    for (uint16_t index = 0U; index < CELL_COUNT; ++index)
    {
        g_distance[index] = UINT16_MAX;
        g_first_step[index] = STEP_UNREACHED;
    }

    uint16_t head = 0U;
    uint16_t tail = 0U;

    const uint16_t origin_index = prv_cell_index(in_origin);

    g_distance[origin_index] = 0U;
    g_queue[tail++] = origin_index;

    while (head < tail)
    {
        const uint16_t current_index = g_queue[head++];
        const cell_t current =
            prv_make_cell((int16_t)(current_index % PLAYFIELD_WIDTH), (int16_t)(current_index / PLAYFIELD_WIDTH));

        for (uint8_t step = 0U; step < (uint8_t)PACMAN_AI_ACTION_COUNT; ++step)
        {
            const cell_t next = playfield_step(current, g_search_order[step]);

            if (!playfield_is_walkable(in_playfield, next) || playfield_is_house(in_playfield, next)
                || playfield_is_gate(in_playfield, next))
            {
                continue;
            }

            const uint16_t next_index = prv_cell_index(next);

            if (g_distance[next_index] != UINT16_MAX)
            {
                continue;
            }

            g_distance[next_index] = (uint16_t)(g_distance[current_index] + 1U);

            /* The first step of the route: inherited from whoever we came through, except one
             * step out from Pacman, where this *is* the first step. */
            g_first_step[next_index] = (current_index == origin_index) ? step : g_first_step[current_index];

            g_queue[tail++] = next_index;
        }
    }
}

/* The nearest reached cell for which `in_matches` is true, as a distance and a first step. */
typedef bool (*prv_cell_predicate_t)(const msg_game_state_t* in_state, const playfield_t* in_playfield, cell_t in_cell,
                                     uint8_t in_context);

static void prv_find_nearest(const msg_game_state_t* const in_state, const playfield_t* const in_playfield,
                             prv_cell_predicate_t in_matches, uint8_t in_context, uint16_t* const out_distance,
                             uint8_t* const out_first_step)
{
    *out_distance = UINT16_MAX;
    *out_first_step = STEP_UNREACHED;

    for (uint16_t index = 0U; index < CELL_COUNT; ++index)
    {
        if ((g_distance[index] == UINT16_MAX) || (g_distance[index] >= *out_distance))
        {
            continue;
        }

        /* Skips Pacman's own cell, the one reached cell with no first step. Something he is
         * already standing on needs no direction, and reporting it would blank all four slots
         * instead — which is what happened before this guard existed. */
        if (g_first_step[index] == STEP_UNREACHED)
        {
            continue;
        }

        const cell_t cell = prv_make_cell((int16_t)(index % PLAYFIELD_WIDTH), (int16_t)(index / PLAYFIELD_WIDTH));

        if (in_matches(in_state, in_playfield, cell, in_context))
        {
            *out_distance = g_distance[index];
            *out_first_step = g_first_step[index];
        }
    }
}

static bool prv_is_pellet(const msg_game_state_t* in_state, const playfield_t* in_playfield, cell_t in_cell,
                          uint8_t in_context)
{
    (void)in_playfield;
    (void)in_context;

    return msg_cell_bitmap_get(in_state->has_pellet, (uint8_t)in_cell.x, (uint8_t)in_cell.y);
}

static bool prv_is_power_pellet(const msg_game_state_t* in_state, const playfield_t* in_playfield, cell_t in_cell,
                                uint8_t in_context)
{
    (void)in_playfield;
    (void)in_context;

    return msg_cell_bitmap_get(in_state->has_pellet, (uint8_t)in_cell.x, (uint8_t)in_cell.y)
           && msg_cell_bitmap_get(in_state->is_power, (uint8_t)in_cell.x, (uint8_t)in_cell.y);
}

/* A ghost standing on this cell, wanted either frightened (`in_context` non-zero) or not.
 *
 * A frightened ghost that has already been eaten is sitting in the pen un-frightened while the
 * others are still blue, which is why the state carries a bit per ghost rather than one flag —
 * and why this asks per ghost rather than about the game. */
static bool prv_is_ghost(const msg_game_state_t* in_state, const playfield_t* in_playfield, cell_t in_cell,
                         uint8_t in_context)
{
    (void)in_playfield;

    const bool want_frightened = (in_context != 0U);

    for (uint8_t index = 0U; index < MSG_GHOST_COUNT; ++index)
    {
        const bool is_frightened = ((in_state->frightened_ghosts & (uint8_t)(1U << index)) != 0U);

        if (is_frightened != want_frightened)
        {
            continue;
        }

        if ((in_state->ghosts[index].column == (uint8_t)in_cell.x)
            && (in_state->ghosts[index].row == (uint8_t)in_cell.y))
        {
            return true;
        }
    }

    return false;
}

/* Which relative action a compass step is, for an agent facing `in_facing`. */
static pacman_ai_action_e prv_step_to_action(uint8_t in_step, direction_e in_facing)
{
    const direction_e step_direction = g_search_order[in_step];

    for (uint8_t action = 0U; action < (uint8_t)PACMAN_AI_ACTION_COUNT; ++action)
    {
        if (pacman_ai_action_to_direction((pacman_ai_action_e)action, in_facing) == step_direction)
        {
            return (pacman_ai_action_e)action;
        }
    }

    return PACMAN_AI_ACTION_FORWARD;
}

/* Write one distance into the feature slot of whichever action leads that way. */
static void prv_place_distance(float* const out_features, uint8_t in_slot, uint16_t in_distance, uint8_t in_first_step,
                               direction_e in_facing)
{
    for (uint8_t action = 0U; action < (uint8_t)PACMAN_AI_ACTION_COUNT; ++action)
    {
        out_features[(action * PACMAN_AI_FEATURES_PER_ACTION) + in_slot] = PACMAN_AI_DISTANCE_NONE;
    }

    if (in_first_step == STEP_UNREACHED)
    {
        return;
    }

    const pacman_ai_action_e action = prv_step_to_action(in_first_step, in_facing);

    out_features[((uint8_t)action * PACMAN_AI_FEATURES_PER_ACTION) + in_slot] = prv_scale_distance(in_distance);
}

/* ==========================================================================
 * pacman_ai - public functions
 * ========================================================================= */

direction_e pacman_ai_action_to_direction(pacman_ai_action_e in_action, direction_e in_facing)
{
    /* Facing north, "left" is west. The rest is this table turned. */
    static const direction_e g_turns[4][PACMAN_AI_ACTION_COUNT] = {
        /* facing NORTH */ {DIRECTION_NORTH, DIRECTION_WEST, DIRECTION_EAST, DIRECTION_SOUTH},
        /* facing SOUTH */ {DIRECTION_SOUTH, DIRECTION_EAST, DIRECTION_WEST, DIRECTION_NORTH},
        /* facing EAST  */ {DIRECTION_EAST, DIRECTION_NORTH, DIRECTION_SOUTH, DIRECTION_WEST},
        /* facing WEST  */ {DIRECTION_WEST, DIRECTION_SOUTH, DIRECTION_NORTH, DIRECTION_EAST},
    };

    ASSERT(in_action < PACMAN_AI_ACTION_COUNT);

    uint8_t row = 0U;

    switch (in_facing)
    {
        case DIRECTION_SOUTH: row = 1U; break;
        case DIRECTION_EAST: row = 2U; break;
        case DIRECTION_WEST: row = 3U; break;

        /* A level begins with Pacman stationary, so this is a real state and not a mistake. */
        case DIRECTION_NORTH:
        case DIRECTION_NONE:
        default: row = 0U; break;
    }

    return g_turns[row][(uint8_t)in_action];
}

pacman_ai_action_e pacman_ai_choose_action(const float* in_scores)
{
    ASSERT(in_scores != NULL);

    pacman_ai_action_e best = PACMAN_AI_ACTION_FORWARD;

    /* Strictly greater, walking upwards from the first action: that is the documented
     * tie-break, and it is what makes the host and the target agree. */
    for (uint8_t action = 1U; action < (uint8_t)PACMAN_AI_ACTION_COUNT; ++action)
    {
        if (in_scores[action] > in_scores[(uint8_t)best])
        {
            best = (pacman_ai_action_e)action;
        }
    }

    return best;
}

void pacman_ai_get_features(const msg_game_state_t* in_state, const playfield_t* in_playfield, float* out_features)
{
    ASSERT(in_state != NULL);
    ASSERT(in_playfield != NULL);
    ASSERT(out_features != NULL);

    const cell_t pacman = prv_make_cell((int16_t)in_state->pacman.column, (int16_t)in_state->pacman.row);
    const direction_e facing = (direction_e)in_state->pacman.direction;

    /* Is each way out open? Asked of the maze directly rather than of the search, because a
     * corridor can be open and lead nowhere the search cares about. */
    for (uint8_t action = 0U; action < (uint8_t)PACMAN_AI_ACTION_COUNT; ++action)
    {
        const direction_e direction = pacman_ai_action_to_direction((pacman_ai_action_e)action, facing);
        const cell_t next = playfield_step(pacman, direction);
        const bool is_open = playfield_is_walkable(in_playfield, next) && !playfield_is_house(in_playfield, next)
                             && !playfield_is_gate(in_playfield, next);

        out_features[(action * PACMAN_AI_FEATURES_PER_ACTION)] = is_open ? 1.0F : 0.0F;
    }

    prv_search_from(in_playfield, pacman);

    uint16_t distance = 0U;
    uint8_t first_step = 0U;

    prv_find_nearest(in_state, in_playfield, prv_is_pellet, 0U, &distance, &first_step);
    prv_place_distance(out_features, 1U, distance, first_step, facing);

    prv_find_nearest(in_state, in_playfield, prv_is_power_pellet, 0U, &distance, &first_step);
    prv_place_distance(out_features, 2U, distance, first_step, facing);

    prv_find_nearest(in_state, in_playfield, prv_is_ghost, 0U, &distance, &first_step);
    prv_place_distance(out_features, 3U, distance, first_step, facing);

    prv_find_nearest(in_state, in_playfield, prv_is_ghost, 1U, &distance, &first_step);
    prv_place_distance(out_features, 4U, distance, first_step, facing);

    const uint8_t global = (uint8_t)PACMAN_AI_ACTION_COUNT * PACMAN_AI_FEATURES_PER_ACTION;
    const uint16_t remaining = playfield_get_remaining_pellet_count(in_playfield);
    const uint16_t total = playfield_get_total_pellet_count(in_playfield);

    out_features[global] = (in_state->frightened_ghosts != 0U) ? 1.0F : 0.0F;
    out_features[global + 1U] = in_state->are_frightened_ghosts_flashing ? 1.0F : 0.0F;
    out_features[global + 2U] = (total > 0U) ? ((float)remaining / (float)total) : 0.0F;
}

bool pacman_ai_is_available(void)
{
    /* Both halves matter. The table could be malformed, which `neural_net` can tell; or it could
     * be well formed but shaped for a different observation — a table exported before a feature
     * was added would evaluate happily and read garbage past the end of the 23 it was given. */
    return neural_net_is_well_formed(&g_ai_weights_network)
           && (g_ai_weights_network.input_count == (uint16_t)PACMAN_AI_FEATURE_COUNT)
           && (g_ai_weights_network.output_count == (uint16_t)PACMAN_AI_ACTION_COUNT);
}

direction_e pacman_ai_decide(const msg_game_state_t* in_state, const playfield_t* in_playfield)
{
    ASSERT(in_state != NULL);
    ASSERT(in_playfield != NULL);

    float features[PACMAN_AI_FEATURE_COUNT];
    float scores[PACMAN_AI_ACTION_COUNT];

    pacman_ai_get_features(in_state, in_playfield, features);
    neural_net_evaluate(&g_ai_weights_network, features, scores);

    return pacman_ai_action_to_direction(pacman_ai_choose_action(scores), (direction_e)in_state->pacman.direction);
}
