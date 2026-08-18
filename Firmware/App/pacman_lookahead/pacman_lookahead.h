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
 * Three, and what stops it being four is RAM: a level of depth is a `game_t` that has to be kept
 * while its branches are tried, the target has about 74 kB spare and a `game_t` is 15 kB. A fourth
 * level would leave the firmware 14 kB, which is not a margin.
 *
 * **It is a ceiling that is not reached, and that is the more useful fact.** At
 * #PACMAN_LOOKAHEAD_DEFAULT_TICK_BUDGET the search finishes 1.63 deepenings on an average decision,
 * so a fourth clone would be memory bought for a level the budget never pays for. Whatever binds
 * this module, it is not this constant.
 */
#define PACMAN_LOOKAHEAD_MAX_DEPTH           (4U)

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
 *
 * **What this number costs, measured** ([M6 §15.5](../../../Docu/Design/M6-Pacman-AI.md)): it is
 * the difference between a player scoring **3,132** and the same code scoring **7,076**. It is also
 * why that version of the player visibly dithered —
 * past a 1.63-junction horizon every branch evaluates alike, so the tie-break decides and it flips
 * one cell later. **Raising it is the only thing measured to help**: a coarser simulation step and
 * a pruned reversal branch both buy the same depth for free and lose score doing it. What a tick
 * *costs* is the figure to redo if the game gets slower; the depth and the maze do not enter into
 * it.
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

/*! \brief How far a leaf looks around itself, in cells of maze distance.
 *
 * The evaluation's own horizon, and a different thing from the search's: the search sees three
 * junctions of *future*, this sees twenty cells of *surroundings* at the position it ends on. It is
 * what stops a leaf being blind to everything the branches did not walk into
 * ([M6 §15.5](../../../Docu/Design/M6-Pacman-AI.md)), and it bounds the scan that measures it.
 */
#define PACMAN_LOOKAHEAD_SCAN_RADIUS          (20U)

/*! \brief The most cells one scan may look at, whatever the radius allows.
 *
 * **The radius bounds the answer; this bounds the work, and only one of those is what a frame needs.**
 * Twenty cells of radius reaches a few hundred cells in an open part of the maze and a dozen in a
 * corridor, so a radius alone makes the worst frame a property of where Pacman happens to be
 * standing: measured on the board, a scan free to sweep its whole radius took the worst frame to
 * **23 ms of the 13 a frame has spare**, while the mean stayed comfortable.
 *
 * It is the same lesson [DEC-050](../../../Docu/PrePlanning/11-Decisions-and-As-Built.md) learned
 * about the search itself — a budget has to be denominated in what is actually paid for. Every cell
 * a scan visits costs the same, so cells are the unit, and forty-eight of them is about a fifth of
 * an unbounded sweep.
 *
 * What it costs in answers is small and one-sided: a distance further than this reads as
 * #PACMAN_LOOKAHEAD_SCAN_RADIUS, which is what "nothing near" already meant.
 */
#define PACMAN_LOOKAHEAD_SCAN_CELLS           (48U)

/*! \brief How far the food term looks, in cells of maze distance.
 *
 * Sixty-four, which is most of the maze — the point of this one is that it is **not** bounded the way
 * the leaf scan is. The last pellets of a level are the ones the search cannot see, and they are
 * exactly the ones that are far away: measured, the last ten pellets of a level are 4 % of them and
 * cost **33 % of the level**, at twenty-two times the ticks per pellet of the first hundred and
 * forty-four ([M6 §18](../../../Docu/Design/M6-Pacman-AI.md)).
 *
 * It is affordable because **pellets do not move**. One breadth-first walk from every remaining
 * pellet at once, taken when the search begins, gives every cell in the maze its distance to the
 * nearest one — and then a leaf is an array lookup. That is one walk a decision against forty-five,
 * which is the whole reason the leaf scan has to be capped and this does not.
 */
#define PACMAN_LOOKAHEAD_FOOD_HORIZON         (64U)

/*! \brief Cells of the food walk one frame may do.
 *
 * The walk crosses up to 868 cells in an almost-empty maze, and doing that in one frame took the worst
 * frame to **14 ms of the 13 a frame has** — twice, after two attempts to shave a constant until it
 * fitted. So the walk is cut up instead of the answer being: sixty-four cells a frame, into a second
 * field that nothing reads until it is finished, and the two swap when it is.
 *
 * A whole walk therefore takes at worst fourteen frames — **longer than the ten a cell lasts**, and
 * deliberately so. There is no reason for the field to be ready within one decision: pellets move
 * slowly, so an answer that arrives a cell or two later is the same answer. Every leaf in the meantime
 * reads the last *complete* field — stale by however many pellets were eaten since, which is
 * a handful, and the branch that ate them was already paid for them in score.
 */
#define PACMAN_LOOKAHEAD_FOOD_CELLS_PER_SLICE (64U)

/*! \brief What each thing a leaf can see is worth, relative to the others.
 *
 * **These are meant to be fitted, not argued.** The three the module started with were chosen by
 * reasoning — ten a point so that one pellet outranks the whole safety range — which works while
 * there are three of them and stops working at seven, because what decides the playing style is the
 * *ratios* and nobody can reason those out. They are a struct rather than `#define`s so a host
 * trainer can vary them; the target takes the defaults and never calls the setter.
 *
 * Every term is "more is better" and bounded, so the fit has no cliffs in it: distances are counted
 * as `PACMAN_LOOKAHEAD_SCAN_RADIUS - distance`, which is *nearness*.
 *
 * **There was an eighth**, the count of pellets within the radius, and it is gone: the fit gave it a
 * weight of 2 where it gave the frightened ghosts 53, and it was the only term that made the scan
 * sweep its whole radius rather than stopping at the nearest thing of each kind. The cheapest term
 * to compute was carrying the most and the dearest was carrying almost nothing — on the board the
 * difference was 23 ms of a frame that has 13.
 */
typedef struct
{
    int32_t point;  /*!< per point of score the branch gained                        */
    int32_t death;  /*!< subtracted per life it cost                                 */
    int32_t threat; /*!< per cell of maze distance to the nearest killing ghost      */
    int32_t prey;   /*!< per cell of nearness to the nearest frightened ghost        */
    int32_t food;   /*!< per cell of nearness to the nearest uneaten pellet          */
    int32_t escape; /*!< per direction out of the cell that is not a wall            */
} pacman_lookahead_weights_t;

/*! \brief The weights the firmware ships with.
 *
 * \param[out]      out_weights: filled in, must not be `NULL`
 */
void pacman_lookahead_get_default_weights(pacman_lookahead_weights_t* out_weights);

/*! \brief Use these weights until told otherwise. `NULL` restores the defaults.
 *
 * For the host trainer. Nothing on the target calls it, and the defaults are what a board plays.
 *
 * \param[in]       in_weights: weights to adopt, or `NULL` for #pacman_lookahead_get_default_weights
 */
void pacman_lookahead_set_weights(const pacman_lookahead_weights_t* in_weights);

/*! \brief What one frame may spend on thinking.
 *
 * A frame is 20 ms and drawing plus deciding measured 7, so 13 are spare and a tick costs 22 us on
 * this part.
 *
 * **It was 350 and had to come down to 250, because the frame is shared now.** A slice pays for its
 * ticks *and* for one leaf scan per leg it reaches (#PACMAN_LOOKAHEAD_SCAN_CELLS), and the two
 * together took the worst frame to 14 ms of 13. Two hundred and fifty is about 5.5 ms of ticks and
 * leaves the scans room. It costs almost nothing in thinking: a decision spends about 2,000 ticks
 * across the ten frames its cell lasts, so 200 a frame is the average and the slice only binds the
 * expensive frames — which are exactly the ones the ceiling is for.
 *
 * A slice is checked *between* legs and never inside one, so a frame can overrun it by up to one
 * leg — about thirty ticks, two thirds of a millisecond. That is the price of being able to pause
 * with no half-finished state, and it is paid out of the margin above.
 */
#define PACMAN_LOOKAHEAD_FRAME_SLICE_TICKS   (250U)

/*! \brief What a whole decision may spend when it is allowed to think across frames.
 *
 * Four thousand. The score curve saturates at about five thousand, where the depth ceiling is
 * reached on every decision ([M6 §15.5](../../../Docu/Design/M6-Pacman-AI.md)), and a cell lasts
 * 10.6 frames on average — 3,700 ticks at the slice above — so a larger number would describe a
 * cell that does not occur. It is a backstop against a search that never converges, not a target.
 */
#define PACMAN_LOOKAHEAD_ANYTIME_TICK_BUDGET (4000U)

/*! \brief Begin a search, rooted here, thrown away by the next call.
 *
 * The first of the three calls that let a decision be worked out **across frames instead of inside
 * one** ([M6 §15.6](../../../Docu/Design/M6-Pacman-AI.md)). The run is copied, so the caller may
 * tick the real game as much as it likes while the answer is being worked out; every branch,
 * slice and deepening is measured against the copy taken here.
 *
 * Call it when Pacman reaches a new cell, then #pacman_lookahead_think once a frame, and read
 * #pacman_lookahead_get_direction whenever an answer is wanted. There is an answer from the first
 * slice onwards, and it only gets deeper.
 *
 * \param[in]       in_game: the run to decide for, must not be `NULL`
 * \param[in]       in_depth: junctions to look ahead, 1..#PACMAN_LOOKAHEAD_MAX_DEPTH
 * \param[in]       in_tick_budget: simulated ticks the whole decision may spend, must not be `0`
 */
void pacman_lookahead_restart(const game_t* in_game, uint8_t in_depth, uint16_t in_tick_budget);

/*! \brief Spend a slice of thinking on the search begun by #pacman_lookahead_restart.
 *
 * \param[in]       in_slice_ticks: simulated ticks this call may spend, must not be `0`
 * \return          `true` while a further slice would still change the answer
 */
bool pacman_lookahead_think(uint16_t in_slice_ticks);

/*! \brief The best answer the search has reached so far.
 *
 * The deepest deepening that was allowed to *finish*, which is what makes a half-thought answer
 * safe to use: a truncated look compares one branch that was studied against three that were not,
 * and is never what comes back here while a complete one exists. Before any deepening finishes it
 * is the first way out of the cell, which is a legal move and honestly all the search yet knows.
 *
 * \return          The absolute direction to hand to `game_set_direction`
 */
direction_e pacman_lookahead_get_direction(void);

/*! \brief What the search in progress has done so far.
 *
 * \param[out]      out_report: filled in, must not be `NULL`
 */
void pacman_lookahead_get_report(pacman_lookahead_report_t* out_report);

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
