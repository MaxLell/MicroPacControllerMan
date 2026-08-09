/*
 * pacman_lookahead.h
 *
 * A Pacman that decides by playing the game forward
 * ([M6 §2](../../../Docu/Design/M6-Pacman-AI.md), [DEC-050](../../../Docu/PrePlanning/11-Decisions-and-As-Built.md)).
 *
 * The trained network of `pacman_ai` answers from what it can see *now*: distances down four
 * corridors and three facts about the run. This module answers from what actually happens — it
 * clones the run, drives the clone down each way out, and reads the score off the end of it.
 * There is nothing to train and nothing to port, because the model of the game is the game
 * ([`game_clone`](../game/game.h)): every rule the search assumes is a rule the player is playing
 * under, and the two cannot drift apart the way a hand-written simulator would.
 *
 * **It exists because a route search was deleted.** The arcade's greedy ghost rule costs about
 * 10 us a step where the breadth-first one cost 300
 * ([DEC-049](../../../Docu/PrePlanning/11-Decisions-and-As-Built.md)), and simulating a future is
 * mostly simulating ghosts. That change is what made this affordable at all — though by rather
 * less than the decision claimed, see #PACMAN_LOOKAHEAD_DEFAULT_TICK_BUDGET.
 *
 * **What it is not:** it is not the agent FR-038 asks for, and it does not replace it. FR-038
 * wants trained weights evaluated on the target, and a search is not weights. What this is good
 * for is being the *reference* M6 §2 kept it in reserve as — an upper mark to measure a trained
 * agent against, and a teacher if one is ever cloned from it.
 *
 * **Not reentrant, and it owns real memory:** the clones are file-scope, one per level of depth,
 * about 15 kB each. That is deliberate (NFR-008) and it is why the depth is a compile-time
 * ceiling rather than an argument.
 */

#ifndef PACMAN_LOOKAHEAD_H
#define PACMAN_LOOKAHEAD_H

#include <stdbool.h>
#include <stdint.h>

#include "game.h"
#include "playfield.h"

/* ==========================================================================
 * pacman_lookahead - public types
 * ========================================================================= */

/*! \brief How many junctions ahead the search may look.
 *
 * Three, and the limit is RAM rather than time: a level of depth is a `game_t` that has to be
 * kept while its branches are tried, the target has about 74 kB spare and a `game_t` is 15 kB.
 * A fourth level would leave the firmware 14 kB, which is not a margin.
 */
#define PACMAN_LOOKAHEAD_MAX_DEPTH           (3U)

/*! \brief What a search may simulate, in `game_tick` calls, unless the caller says otherwise.
 *
 * **The unit is the interesting part.** It was cells of simulated future, the vocabulary
 * [DEC-049](../../../Docu/PrePlanning/11-Decisions-and-As-Built.md) argued the greedy ghost rule
 * back in, and a cell is the wrong thing to charge for: the search pays per **tick**, and a branch
 * that walks Pacman into a wall burns its whole tick allowance without ever reaching a cell. So a
 * cell budget bounded the useful work and left the wasted work unbounded — measured on the board,
 * a search that kept exactly to 48 cells still took 19 ms of a frame that had 13.
 *
 * Five hundred, from the board rather than from arithmetic. `ott lookahead_cost` prices a tick at
 * **19 us** on this part — the first estimate of 36 was taken from a search that was also paying
 * for the stalled branches this budget now charges for — and the worst decision runs about a fifth
 * over the mean. So 500 ticks is 9.5 ms of mean and about 12 of worst, inside the 13 a frame has
 * spare with the margin on the side that matters. What a tick *costs* is the figure to redo if the
 * game gets slower; the depth and the maze do not enter into it.
 *
 * A search that reaches the budget stops where it is and throws away the deepening it was in the
 * middle of, keeping the last one that finished — see \ref pacman_lookahead_decide_within — which
 * is why the ceiling can be this tight without the answer becoming lopsided.
 */
#define PACMAN_LOOKAHEAD_DEFAULT_TICK_BUDGET (500U)

/*! \brief What a search did, for a caller that has to know whether it was allowed to finish.
 *
 * The budget is the interesting field. A search that spent it is a search that returned a
 * *partial* answer — the branches it did explore, in the order it explores them — and a frame
 * that consistently truncates is a frame that wants a smaller depth, not a slower game.
 */
typedef struct
{
    uint16_t simulated_ticks; /*!< What was spent, in the budget's own unit  */
    uint16_t simulated_cells; /*!< Cells of future actually walked           */
    uint16_t examined_legs;   /*!< Junction-to-junction runs explored        */
    uint8_t reached_depth;    /*!< Deepest level any branch got to           */
    bool was_truncated;       /*!< The cell budget ran out before the search did */
} pacman_lookahead_report_t;

/* ==========================================================================
 * pacman_lookahead - public API
 * ========================================================================= */

/*! \brief Which way to go, worked out by playing this run forward.
 *
 * The direction is absolute and meant for `game_set_direction`, so this drops into the same slot
 * `pacman_ai_decide` occupies. Ask it when Pacman reaches a new cell; asking more often costs a
 * search and answers the same.
 *
 * \ref DIRECTION_NONE comes back when the run is not running, which is the honest answer to
 * "which way" for a game that has ended.
 *
 * \param[in]       in_game: the run to decide for, must not be `NULL`
 * \return          The absolute direction to hand to `game_set_direction`
 */
direction_e pacman_lookahead_decide(const game_t* in_game);

/*! \brief The same decision, with the budget named and what it cost reported back.
 *
 * What the on-target test and the host measurements use. \ref pacman_lookahead_decide is this
 * with #PACMAN_LOOKAHEAD_DEFAULT_TICK_BUDGET and no report.
 *
 * \param[in]       in_game: the run to decide for, must not be `NULL`
 * \param[in]       in_depth: junctions to look ahead, 1..#PACMAN_LOOKAHEAD_MAX_DEPTH
 * \param[in]       in_tick_budget: simulated ticks it may spend, must not be `0`
 * \param[out]      out_report: what the search did, may be `NULL`
 * \return          The absolute direction to hand to `game_set_direction`
 */
direction_e pacman_lookahead_decide_within(const game_t* in_game, uint8_t in_depth, uint16_t in_tick_budget,
                                           pacman_lookahead_report_t* out_report);

#endif /* PACMAN_LOOKAHEAD_H */
