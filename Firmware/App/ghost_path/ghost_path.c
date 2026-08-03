#include "ghost_path.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

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
typedef bool (*prv_is_better_fn)(uint32_t in_candidate, uint32_t in_incumbent);

static bool prv_is_further(uint32_t in_candidate, uint32_t in_incumbent)
{
    return in_candidate > in_incumbent;
}

/* The greedy one-cell choice, which is now the *fleeing* rule only.
 *
 * Seeking got a route search (below). Fleeing keeps this, and not for want of effort: there
 * is no destination to plan a route to — the rule is "get further from Pacman", and the
 * cell that does that best is by definition the one next door. §10.5 says nothing about
 * where a frightened ghost ends up, only which way it turns. */
static direction_e prv_find_step(const playfield_t* const in_playfield, cell_t in_from, cell_t in_reference,
                                 direction_e in_forbidden_direction, bool in_may_pass_gate,
                                 prv_is_better_fn in_is_better)
{
    direction_e best_direction = DIRECTION_NONE;
    uint32_t best_cost = 0U;
    direction_e dead_end_direction = DIRECTION_NONE;

    ASSERT(in_playfield != NULL);
    ASSERT(in_is_better != NULL);

    for (size_t index = 0U; index < CANDIDATE_COUNT; ++index)
    {
        const direction_e candidate = k_candidate_order[index];
        const cell_t neighbour = playfield_step(in_from, candidate);
        uint32_t cost;

        if (!playfield_is_walkable(in_playfield, neighbour))
        {
            continue;
        }

        /* The ghost house gate. A ghost on its way out may cross it and an eaten one may
         * cross it going home; one already loose in the maze may not, which is what stops
         * the four of them wandering back into their cage (§10.4). */
        if (!in_may_pass_gate && playfield_is_gate(in_playfield, neighbour))
        {
            continue;
        }

        if (candidate == in_forbidden_direction)
        {
            /* Remembered but not chosen: only used if nothing else is open. */
            dead_end_direction = candidate;
            continue;
        }

        cost = playfield_get_squared_distance(neighbour, in_reference);

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

/* ---- the route search --------------------------------------------------- */

#define CELL_COUNT  (PLAYFIELD_WIDTH * PLAYFIELD_HEIGHT)
#define NOT_VISITED (0U)

/* Scratch for one search. Static because it is 4 kB and belongs on neither a stack nor four
 * copies of a ghost; the search runs to completion inside one call, so one set does for all
 * of them. `first_direction` doubles as the visited flag, holding the direction plus one. */
static uint8_t g_first_direction[CELL_COUNT];
static uint16_t g_step_count[CELL_COUNT];
static uint16_t g_queue[CELL_COUNT];

static uint16_t prv_get_index(cell_t in_cell)
{
    const cell_t cell = playfield_wrap_cell(in_cell);

    return (uint16_t)((cell.y * PLAYFIELD_WIDTH) + cell.x);
}

static cell_t prv_get_cell(uint16_t in_index)
{
    const cell_t cell = {(int16_t)(in_index % PLAYFIELD_WIDTH), (int16_t)(in_index / PLAYFIELD_WIDTH)};

    return cell;
}

static bool prv_is_open(const playfield_t* const in_playfield, cell_t in_cell, bool in_may_pass_gate)
{
    if (!playfield_is_walkable(in_playfield, in_cell))
    {
        return false;
    }

    return in_may_pass_gate || !playfield_is_gate(in_playfield, in_cell);
}

/* The first step of the shortest route to the reachable cell nearest the target.
 *
 * **Why this and not a textbook A\*.** A\* prunes by guessing how much further there is to
 * go, and that only pays when the goal can actually be reached. Two of the four ghosts aim
 * at cells that usually cannot be: §10.4 has Pinky aim four cells past Pacman and Inky
 * double a vector from Blinky, so their targets sit inside walls or off the board entirely,
 * and no heuristic can prune a search for something that is not there — it has to expand
 * everything regardless. So the search is breadth-first over the whole reachable set, and
 * then the nearest reachable cell to the target is chosen. Where the target *is* reachable
 * this returns exactly the route A\* would; where it is not, it returns the sensible answer
 * A\* has none for.
 *
 * Ties are free: the queue holds the reachable cells in order of route length, so scanning
 * it and keeping a strict improvement prefers the shorter route, and among equal routes the
 * order in which the first steps were seeded — §10.4's up, left, down, right.
 */
static direction_e prv_find_route_step(const playfield_t* const in_playfield, cell_t in_from, cell_t in_target,
                                       direction_e in_forbidden_direction, bool in_may_pass_gate)
{
    direction_e dead_end_direction = DIRECTION_NONE;
    uint16_t head = 0U;
    uint16_t tail = 0U;
    uint16_t best_index = 0U;
    uint32_t best_distance = 0U;
    bool has_best = false;

    (void)memset(g_first_direction, 0, sizeof(g_first_direction));

    for (size_t index = 0U; index < CANDIDATE_COUNT; ++index)
    {
        const direction_e candidate = k_candidate_order[index];
        const cell_t neighbour = playfield_step(in_from, candidate);
        const uint16_t neighbour_index = prv_get_index(neighbour);

        if (!prv_is_open(in_playfield, neighbour, in_may_pass_gate))
        {
            continue;
        }

        if (candidate == in_forbidden_direction)
        {
            /* Remembered but not seeded: §10.1 forbids turning round as a *choice*, and only
             * a stub corridor makes it the last resort. */
            dead_end_direction = candidate;
            continue;
        }

        if (g_first_direction[neighbour_index] != NOT_VISITED)
        {
            continue;
        }

        g_first_direction[neighbour_index] = (uint8_t)(candidate + 1);
        g_step_count[neighbour_index] = 1U;
        g_queue[tail] = neighbour_index;
        ++tail;
    }

    while (head < tail)
    {
        const uint16_t index = g_queue[head];
        const cell_t cell = prv_get_cell(index);

        ++head;

        for (size_t candidate_index = 0U; candidate_index < CANDIDATE_COUNT; ++candidate_index)
        {
            const cell_t neighbour = playfield_step(cell, k_candidate_order[candidate_index]);
            const uint16_t neighbour_index = prv_get_index(neighbour);

            if (!prv_is_open(in_playfield, neighbour, in_may_pass_gate)
                || (g_first_direction[neighbour_index] != NOT_VISITED))
            {
                continue;
            }

            g_first_direction[neighbour_index] = g_first_direction[index];
            g_step_count[neighbour_index] = (uint16_t)(g_step_count[index] + 1U);
            g_queue[tail] = neighbour_index;
            ++tail;
        }
    }

    for (uint16_t position = 0U; position < tail; ++position)
    {
        const uint16_t index = g_queue[position];
        const uint32_t distance = playfield_get_squared_distance(prv_get_cell(index), in_target);

        if (!has_best || (distance < best_distance))
        {
            has_best = true;
            best_index = index;
            best_distance = distance;
        }
    }

    if (!has_best)
    {
        return dead_end_direction;
    }

    return (direction_e)(g_first_direction[best_index] - 1U);
}

direction_e ghost_path_find_step_towards(const playfield_t* in_playfield, cell_t in_from, cell_t in_target,
                                         direction_e in_forbidden_direction, bool in_may_pass_gate)
{
    ASSERT(in_playfield != NULL);

    return prv_find_route_step(in_playfield, in_from, in_target, in_forbidden_direction, in_may_pass_gate);
}

direction_e ghost_path_find_step_away_from(const playfield_t* in_playfield, cell_t in_from, cell_t in_avoid,
                                           direction_e in_forbidden_direction, bool in_may_pass_gate)
{
    return prv_find_step(in_playfield, in_from, in_avoid, in_forbidden_direction, in_may_pass_gate, prv_is_further);
}
