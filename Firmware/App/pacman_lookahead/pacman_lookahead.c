#include "pacman_lookahead.h"

#include <string.h>

#include "custom_assert.h"
#include "pacman.h"

/* ==========================================================================
 * pacman_lookahead - private
 * ========================================================================= */

/*! \brief How long a simulated tick is.
 *
 * The frame period the game is really ticked at, and it is that on purpose: a coarser step would
 * simulate a game nobody plays, and the actor speeds are per-millisecond figures that a step of
 * the wrong size rounds differently. `game_session` ticks at this, so the search does.
 */
#define PACMAN_LOOKAHEAD_STEP_MS        (16U)

/*! \brief Ticks a single simulated cell may take before the walk gives up on it.
 *
 * A Pacman stopped against a wall never reaches another cell, and without a ceiling the loop
 * looking for the next one would run for ever. Seven ticks is a cell at level 1, so this is four
 * times what the slowest crawl needs and is a backstop rather than a working limit.
 */
#define PACMAN_LOOKAHEAD_MAX_CELL_TICKS (32U)

/*! \brief Cells a single junction-to-junction leg may run for.
 *
 * The arcade's longest corridor is shorter than this. It exists for the case a leg has no
 * junction to end at — a ring of corridor with no branch — where the walk would otherwise spend
 * the whole budget on one branch.
 */
#define PACMAN_LOOKAHEAD_MAX_LEG_CELLS  (24U)

/*! \brief What a point of score is worth against the other terms.
 *
 * Ten, so that the smallest thing worth having — one pellet, ten points — outranks the whole
 * range of the safety term below. Score is what the game is scored on; safety only sorts branches
 * that are worth the same.
 */
#define PACMAN_LOOKAHEAD_POINT_WEIGHT   (10)

/*! \brief What losing a life costs the branch that leads to it.
 *
 * Large enough that no reachable amount of score buys one. A search whose branches all die still
 * picks the one that dies latest, because the score gathered on the way is what separates them.
 */
#define PACMAN_LOOKAHEAD_DEATH_PENALTY  (100000)

/*! \brief Where the safety term stops caring, as a squared distance.
 *
 * 64, which is eight cells — the arcade's own figure for "far enough away", the radius Clyde
 * turns shy at (§10.4). Beyond it one ghost is as harmless as another and the branch should be
 * decided by what there is to eat.
 */
#define PACMAN_LOOKAHEAD_SAFE_DISTANCE  (64)

/*! \brief The order branches are tried in.
 *
 * Fixed, and part of the behaviour rather than an implementation detail, for two reasons. A
 * search that spends its cell budget stops where it is, so the order decides what a truncated
 * search got to look at. And two branches can be worth exactly the same, in which case the first
 * one tried wins — the same reason `pacman_ai` writes its search order down.
 */
static const direction_e g_branch_order[] = {DIRECTION_NORTH, DIRECTION_SOUTH, DIRECTION_EAST, DIRECTION_WEST};

/*! \brief One game per level of depth, because a branch has to be tried without losing the
 *         position it branched from.
 *
 * File-scope rather than on the stack: a `game_t` is about 15 kB and the target reserves a
 * kilobyte of stack (NFR-008). This is the memory that makes #PACMAN_LOOKAHEAD_MAX_DEPTH a
 * ceiling. */
static game_t g_clone[PACMAN_LOOKAHEAD_MAX_DEPTH];

/*! \brief What is left of the search's budget, and what it has spent. Reset by every search. */
static uint16_t g_ticks_left;
static pacman_lookahead_report_t g_report;

/*! \brief Whether *this* deepening ran out of budget, as opposed to the search as a whole.
 *
 * Separate from the report's flag because the two answer different questions: the report says the
 * caller did not get everything it asked for, this says the iteration in progress is not worth
 * believing and its answer has to be thrown away. */
static bool g_is_iteration_truncated;

/* The straight-line distance to the nearest ghost that could kill him, squared and capped.
 *
 * Straight-line and not a maze distance on purpose: a maze distance is a breadth-first walk over
 * 868 cells, which is more than the whole leg that produced this position cost to simulate. This
 * term only sorts branches that scored the same, and for that a rough answer is the right price.
 * A frightened ghost is not counted — it is food, and `game` says which is which. */
static int32_t prv_safety_of(const game_t* const in_game)
{
    const cell_t pacman = game_get_pacman_cell(in_game);
    int32_t nearest = PACMAN_LOOKAHEAD_SAFE_DISTANCE;
    uint8_t index;

    for (index = 0U; index < GHOST_COUNT; ++index)
    {
        if (game_is_ghost_frightened(in_game, index))
        {
            continue;
        }

        const uint32_t squared = playfield_get_squared_distance(pacman, game_get_ghost_cell(in_game, index));

        if ((int32_t)squared < nearest)
        {
            nearest = (int32_t)squared;
        }
    }

    return nearest;
}

/* What a simulated position is worth, measured against the position the search started from.
 *
 * Everything here is a difference rather than an absolute, which is what makes the numbers mean
 * "what this branch got me" instead of "how the run is going". */
static int32_t prv_evaluate(const game_t* const in_leaf, const game_t* const in_root)
{
    const int32_t points = (int32_t)game_get_score(in_leaf) - (int32_t)game_get_score(in_root);
    const int32_t lives_lost = (int32_t)game_get_lives(in_root) - (int32_t)game_get_lives(in_leaf);

    return (points * PACMAN_LOOKAHEAD_POINT_WEIGHT) - (lives_lost * PACMAN_LOOKAHEAD_DEATH_PENALTY)
           + prv_safety_of(in_leaf);
}

/* Whether a cell is somewhere a decision gets made: more than the way in and one way on.
 *
 * Counted over all four neighbours including the one behind, so a dead end counts as a junction
 * too — turning round there is a choice, and the only one. */
static bool prv_is_junction(const game_t* const in_game)
{
    const playfield_t* const playfield = game_get_playfield(in_game);
    uint8_t open_count = 0U;
    uint8_t index;

    for (index = 0U; index < (uint8_t)(sizeof(g_branch_order) / sizeof(g_branch_order[0])); ++index)
    {
        if (pacman_may_step(&in_game->pacman, playfield, g_branch_order[index]))
        {
            ++open_count;
        }
    }

    return open_count > 2U;
}

/* Walk a clone from where it stands to the next place a decision is due, and say how far that
 * was in cells.
 *
 * The direction is set once and then the game steers: down a corridor there is nothing to decide,
 * so the walk ends at the next junction, at the end of the run, at the moment a life is lost, or
 * at a cap — whichever comes first. Cells rather than ticks are counted because a cell is what
 * the budget is denominated in and what the board was measured in. */
static uint16_t prv_walk_to_next_decision(game_t* const inout_game, direction_e in_direction)
{
    const uint8_t lives_at_start = game_get_lives(inout_game);
    cell_t previous = game_get_pacman_cell(inout_game);
    uint16_t cells = 0U;
    uint32_t ticks_on_this_cell = 0U;

    game_set_direction(inout_game, in_direction);

    while ((cells < PACMAN_LOOKAHEAD_MAX_LEG_CELLS) && (g_ticks_left > 0U)
           && (game_get_state(inout_game) == GAME_STATE_RUNNING))
    {
        game_tick(inout_game, PACMAN_LOOKAHEAD_STEP_MS);
        --g_ticks_left;
        ++g_report.simulated_ticks;
        ++ticks_on_this_cell;

        const cell_t now = game_get_pacman_cell(inout_game);

        if (playfield_are_cells_equal(now, previous))
        {
            /* Against a wall, or simply mid-step. Either way this cell is not going to end the
             * leg, and something has to stop the loop when it is the former. */
            if (ticks_on_this_cell >= PACMAN_LOOKAHEAD_MAX_CELL_TICKS)
            {
                break;
            }

            continue;
        }

        previous = now;
        ticks_on_this_cell = 0U;
        ++cells;
        ++g_report.simulated_cells;

        /* A death resets everyone to their starting cells, so carrying on here would be walking
         * a position that has nothing to do with the branch being priced. */
        if (game_get_lives(inout_game) != lives_at_start)
        {
            break;
        }

        if (prv_is_junction(inout_game))
        {
            break;
        }
    }

    return cells;
}

/* The value of the best branch out of this position, and which branch it was.
 *
 * `in_level` is both the recursion depth and the clone this level owns — the two are the same
 * number because a level tries its branches one at a time, so one game each is enough. */
static int32_t prv_value_of(const game_t* const in_game, const game_t* const in_root, uint8_t in_remaining_depth,
                            uint8_t in_level, direction_e* const out_best_direction)
{
    int32_t best_value = 0;
    bool has_branch = false;
    uint8_t index;

    if (out_best_direction != NULL)
    {
        *out_best_direction = DIRECTION_NONE;
    }

    if ((in_remaining_depth == 0U) || (g_ticks_left == 0U) || (game_get_state(in_game) != GAME_STATE_RUNNING))
    {
        if (g_ticks_left == 0U)
        {
            g_is_iteration_truncated = true;
        }

        return prv_evaluate(in_game, in_root);
    }

    for (index = 0U; index < (uint8_t)(sizeof(g_branch_order) / sizeof(g_branch_order[0])); ++index)
    {
        const direction_e direction = g_branch_order[index];

        if (!pacman_may_step(&in_game->pacman, game_get_playfield(in_game), direction))
        {
            continue;
        }

        if (g_ticks_left == 0U)
        {
            /* Out of budget with branches left unlooked-at, which makes this iteration's answer a
             * comparison between one branch that was studied and three that were not. The caller
             * throws it away; see #pacman_lookahead_decide_within. */
            g_is_iteration_truncated = true;
            break;
        }

        game_clone(&g_clone[in_level], in_game);

        /* Before a single tick of it: the jitter is drawn from one generator shared with the game
         * being played, so a simulation that let it draw would move the real run's future. See
         * #game_freeze_timings. */
        game_freeze_timings(&g_clone[in_level]);

        const uint16_t walked = prv_walk_to_next_decision(&g_clone[in_level], direction);

        if (walked == 0U)
        {
            /* He could not get out of the cell that way after all — a turn the rules allow but
             * the step timing did not deliver. Nothing was simulated, so there is nothing to
             * price. */
            continue;
        }

        ++g_report.examined_legs;

        const int32_t value = prv_value_of(&g_clone[in_level], in_root, (uint8_t)(in_remaining_depth - 1U),
                                           (uint8_t)(in_level + 1U), NULL);

        if (!has_branch || (value > best_value))
        {
            has_branch = true;
            best_value = value;

            if (out_best_direction != NULL)
            {
                *out_best_direction = direction;
            }
        }
    }

    if (!has_branch)
    {
        return prv_evaluate(in_game, in_root);
    }

    return best_value;
}

/* ==========================================================================
 * pacman_lookahead - public
 * ========================================================================= */

direction_e pacman_lookahead_decide(const game_t* in_game)
{
    return pacman_lookahead_decide_within(in_game, PACMAN_LOOKAHEAD_MAX_DEPTH, PACMAN_LOOKAHEAD_DEFAULT_TICK_BUDGET,
                                          NULL);
}

direction_e pacman_lookahead_decide_within(const game_t* in_game, uint8_t in_depth, uint16_t in_tick_budget,
                                           pacman_lookahead_report_t* out_report)
{
    direction_e best = DIRECTION_NONE;

    ASSERT(in_game != NULL);
    ASSERT(in_depth > 0U);
    ASSERT(in_depth <= PACMAN_LOOKAHEAD_MAX_DEPTH);
    ASSERT(in_tick_budget > 0U);

    memset(&g_report, 0, sizeof(g_report));
    g_ticks_left = in_tick_budget;

    if (game_get_state(in_game) != GAME_STATE_RUNNING)
    {
        if (out_report != NULL)
        {
            *out_report = g_report;
        }

        return DIRECTION_NONE;
    }

    /* **Shallow first, then deeper, keeping the last answer that was allowed to finish.** Straight
     * to the full depth is the obvious thing and it is wrong here, because the budget is smaller
     * than a full search: depth-first spends the whole allowance on the first branch, and the
     * answer is then a comparison between one way out that was studied three junctions deep and
     * three that were never looked at. That is not a theoretical worry — measured on the board's
     * own budget, the player it produced scored *worse* than one walking in a straight line.
     *
     * Iterating costs re-walking the shallow levels, which is real and is the price of an answer
     * that compares like with like. What the caller gets is the deepest complete look the budget
     * paid for, and #pacman_lookahead_report_t::reached_depth says which that was. */
    for (uint8_t depth = 1U; depth <= in_depth; ++depth)
    {
        direction_e candidate = DIRECTION_NONE;

        g_is_iteration_truncated = false;

        (void)prv_value_of(in_game, in_game, depth, 0U, &candidate);

        if (g_is_iteration_truncated)
        {
            g_report.was_truncated = true;

            /* A partial look is worth more than no answer, but only when there is no complete one
             * to prefer. A caller asks which way to go and has to be given a legal move — a budget
             * too small to finish even the first deepening used to hand back
             * \ref DIRECTION_NONE, which reads as "the run has ended" and is not what happened. */
            if ((best == DIRECTION_NONE) && (candidate != DIRECTION_NONE))
            {
                best = candidate;
            }

            break;
        }

        if (candidate != DIRECTION_NONE)
        {
            best = candidate;
            g_report.reached_depth = depth;
        }
    }

    if (best == DIRECTION_NONE)
    {
        /* A budget too small to walk even one cell — every branch was cut off before Pacman
         * reached the next square, so there is nothing to compare and nothing to prefer. The
         * contract still holds: this function answers \ref DIRECTION_NONE when the run is not
         * running, and this run is. So it hands back the first way out, which is a legal move and
         * is honestly all a search this size knows. */
        for (uint8_t index = 0U; index < (uint8_t)(sizeof(g_branch_order) / sizeof(g_branch_order[0])); ++index)
        {
            if (pacman_may_step(&in_game->pacman, game_get_playfield(in_game), g_branch_order[index]))
            {
                best = g_branch_order[index];
                break;
            }
        }
    }

    if (out_report != NULL)
    {
        *out_report = g_report;
    }

    return best;
}
