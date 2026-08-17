#include "ott_lookahead_cost.h"

#include <stdio.h>

#include "Cli.h"
#include "game.h"
#include "pacman_lookahead.h"
#include "systick_bsp.h"

/* ==========================================================================
 * ott_lookahead_cost - private
 * ========================================================================= */

/*! \brief Frames measured.
 *
 * **Frames, not decisions, because a frame is what has to fit.** Since the search thinks across
 * frames ([M6 §15.6](../../../../Docu/Design/M6-Pacman-AI.md)) the question "does one decision fit
 * in a frame" no longer has an answer — a decision is deliberately larger than a frame and is paid
 * for in slices. What must hold is that no single *slice* overruns.
 *
 * Two thousand is about two hundred cells of a real run at ten frames a cell — long enough to
 * contain the expensive positions. They are the crowded ones: a junction with three ways on, ghosts
 * out of the house, nothing eaten yet. The opening cell is the cheapest position in the game and
 * measuring it alone is how a search gets called affordable.
 */
#define OTT_LOOKAHEAD_COST_FRAMES    (2000U)

/*! \brief The simulated tick, matching what `game_session` runs the game at. */
#define OTT_LOOKAHEAD_COST_STEP_MS   (16U)

/*! \brief What a frame has left over for thinking, in milliseconds.
 *
 * The same thirteen `search_budget` is argued in: the owner allowed the rate to fall to 50 fps, so
 * a frame is 20 ms, and drawing and deciding measured 7.
 */
#define OTT_LOOKAHEAD_COST_SPARE_MS  (13U)

#define OTT_LOOKAHEAD_COST_US_PER_MS (1000U)

/* ==========================================================================
 * ott_lookahead_cost - public
 * ========================================================================= */

bool ott_lookahead_cost_run(const uint8_t* in_parameter, char* out_reason, size_t in_reason_size)
{
    /* Not on the stack: a `game_t` is about 12 kB on this part and the target reserves a kilobyte
     * of stack. */
    static game_t game;

    uint32_t frames = 0U;
    uint32_t decisions = 0U;
    uint32_t busy_ms = 0U;
    uint32_t worst_ms = 0U;
    uint32_t deepest = 0U;
    uint32_t depth_sum = 0U;
    uint32_t ticks_spent = 0U;

    (void)in_parameter;

    cli_print("Look-ahead cost: what one frame's slice costs on this board.");
    cli_print("depth %u, slice %u ticks, decision %u ticks, frame spare %u ms", (unsigned)PACMAN_LOOKAHEAD_MAX_DEPTH,
              (unsigned)PACMAN_LOOKAHEAD_FRAME_SLICE_TICKS, (unsigned)PACMAN_LOOKAHEAD_ANYTIME_TICK_BUDGET,
              (unsigned)OTT_LOOKAHEAD_COST_SPARE_MS);

    game_init(&game);
    game_start_on_normal_maze(&game);

    cell_t decided_at = game_get_pacman_cell(&game);
    bool is_first = true;

    while ((frames < OTT_LOOKAHEAD_COST_FRAMES) && (game_get_state(&game) == GAME_STATE_RUNNING))
    {
        game_tick(&game, OTT_LOOKAHEAD_COST_STEP_MS);
        ++frames;

        const cell_t now = game_get_pacman_cell(&game);

        /* The same three calls `game_session` makes, in the same order — this test is only worth
         * its result if it is the shipped loop being timed and not a re-statement of it. */
        const uint32_t before = systick_bsp_get_tick();

        if (is_first || !playfield_are_cells_equal(now, decided_at))
        {
            if (!is_first)
            {
                /* The decision that is *about* to be thrown away is the only one whose depth is
                 * final, so it is read here rather than at the end of a frame — where it would be
                 * the depth of a decision still being thought about. */
                pacman_lookahead_report_t finished;

                pacman_lookahead_get_report(&finished);

                depth_sum += finished.reached_depth;
                ticks_spent += finished.simulated_ticks;
            }

            is_first = false;
            decided_at = now;

            pacman_lookahead_restart(&game, PACMAN_LOOKAHEAD_MAX_DEPTH, PACMAN_LOOKAHEAD_ANYTIME_TICK_BUDGET);

            ++decisions;
        }

        (void)pacman_lookahead_think(PACMAN_LOOKAHEAD_FRAME_SLICE_TICKS);

        game_set_direction(&game, pacman_lookahead_get_direction());

        const uint32_t spent = systick_bsp_get_tick() - before;

        busy_ms += spent;

        if (spent > worst_ms)
        {
            /* The worst frame, not only the mean: a mean inside the budget with one frame far
             * outside it is a frame somebody sees drop. */
            worst_ms = spent;
        }

        pacman_lookahead_report_t report;

        pacman_lookahead_get_report(&report);

        if (report.reached_depth > deepest)
        {
            deepest = report.reached_depth;
        }
    }

    if (decisions == 0U)
    {
        (void)snprintf(out_reason, in_reason_size, "the run ended before a single decision was taken");

        return false;
    }

    /* Microseconds, because at 1 ms of tick resolution a per-frame figure in milliseconds is mostly
     * rounding. The total is what was measured; the division is this test's arithmetic. */
    const uint32_t mean_us = (busy_ms * OTT_LOOKAHEAD_COST_US_PER_MS) / frames;

    cli_print("frames %lu in %lu ms, over %lu decisions", (unsigned long)frames, (unsigned long)busy_ms,
              (unsigned long)decisions);
    cli_print("  a frame's slice: mean %lu us   worst %lu ms", (unsigned long)mean_us, (unsigned long)worst_ms);
    cli_print("  frames per decision %lu", (unsigned long)(frames / decisions));
    cli_print("  junctions reached: %lu.%lu mean, %lu deepest", (unsigned long)(depth_sum / decisions),
              (unsigned long)(((depth_sum * 100U) / decisions) % 100U), (unsigned long)deepest);
    cli_print("  a decision spends %lu ticks", (unsigned long)(ticks_spent / decisions));
    cli_print("score %lu, lives %u", (unsigned long)game_get_score(&game), (unsigned)game_get_lives(&game));

    if (worst_ms > OTT_LOOKAHEAD_COST_SPARE_MS)
    {
        (void)snprintf(out_reason, in_reason_size, "the worst frame took %lu ms of the %u a frame has spare",
                       (unsigned long)worst_ms, (unsigned)OTT_LOOKAHEAD_COST_SPARE_MS);

        return false;
    }

    return true;
}
