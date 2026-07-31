#include "ghost_path.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "custom_assert.h"
#include "msg.h"
#include "playfield.h"

/* ==========================================================================
 * ghost_path - private
 * ========================================================================= */

/* Tie-break order of §10.4: Up, Left, Down, Right. Candidates are examined in this
 * sequence and only a strictly better one displaces the incumbent, which makes the order
 * itself the tie-break — no comparison of "equal" cases needed. */
static const direction_e k_candidate_order[] = {DIRECTION_NORTH, DIRECTION_WEST, DIRECTION_SOUTH, DIRECTION_EAST};

#define CANDIDATE_COUNT (sizeof(k_candidate_order) / sizeof(k_candidate_order[0]))

/*! \brief Whether a candidate beats the incumbent, for the two search senses. */
typedef bool (*prv_is_better_fn)(uint16_t in_candidate, uint16_t in_incumbent);

static bool prv_is_closer(uint16_t in_candidate, uint16_t in_incumbent)
{
    return in_candidate < in_incumbent;
}

static bool prv_is_further(uint16_t in_candidate, uint16_t in_incumbent)
{
    return in_candidate > in_incumbent;
}

/* The one search both public functions share: walk the candidates in tie-break order and
 * keep the best by whichever comparison was handed in.
 *
 * When A* replaces the greedy step, this is the function that changes — the cost of a
 * candidate stops being "distance from the neighbour to the target" and becomes "length
 * of the route through it". The signature and the tie-break stay as they are. */
static direction_e prv_find_step(const playfield_t* const in_playfield, cell_t in_from, cell_t in_reference,
                                 direction_e in_forbidden_direction, prv_is_better_fn in_is_better)
{
    direction_e best_direction = DIRECTION_NONE;
    uint16_t best_cost = 0U;
    direction_e dead_end_direction = DIRECTION_NONE;

    ASSERT(in_playfield != NULL);
    ASSERT(in_is_better != NULL);

    for (size_t index = 0U; index < CANDIDATE_COUNT; ++index)
    {
        const direction_e candidate = k_candidate_order[index];
        const cell_t neighbour = playfield_step(in_from, candidate);
        uint16_t cost;

        if (!playfield_is_walkable(in_playfield, neighbour))
        {
            continue;
        }

        if (candidate == in_forbidden_direction)
        {
            /* Remembered but not chosen: only used if nothing else is open. */
            dead_end_direction = candidate;
            continue;
        }

        cost = playfield_get_distance(neighbour, in_reference);

        if ((best_direction == DIRECTION_NONE) || in_is_better(cost, best_cost))
        {
            best_direction = candidate;
            best_cost = cost;
        }
    }

    if (best_direction != DIRECTION_NONE)
    {
        return best_direction;
    }

    /* A dead-end corridor: the way back is the only way out. §10.1 forbids reversing as a
     * *choice*, not as the last resort — the alternative is a ghost frozen in a stub for
     * the rest of the level. */
    return dead_end_direction;
}

/* ==========================================================================
 * ghost_path - public
 * ========================================================================= */

direction_e ghost_path_find_step_towards(const playfield_t* in_playfield, cell_t in_from, cell_t in_target,
                                         direction_e in_forbidden_direction)
{
    return prv_find_step(in_playfield, in_from, in_target, in_forbidden_direction, prv_is_closer);
}

direction_e ghost_path_find_step_away_from(const playfield_t* in_playfield, cell_t in_from, cell_t in_avoid,
                                           direction_e in_forbidden_direction)
{
    return prv_find_step(in_playfield, in_from, in_avoid, in_forbidden_direction, prv_is_further);
}
