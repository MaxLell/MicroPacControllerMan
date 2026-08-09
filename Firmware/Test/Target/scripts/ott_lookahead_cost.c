#include "ott_lookahead_cost.h"

#include <stdio.h>

#include "Cli.h"
#include "game.h"
#include "pacman_lookahead.h"
#include "systick_bsp.h"

/* ==========================================================================
 * ott_lookahead_cost - private
 * ========================================================================= */

/*! \brief Decisions measured.
 *
 * A decision is asked for once per cell Pacman reaches, so this is a couple of hundred cells of a
 * real run — long enough to contain the expensive positions. They are the crowded ones: a junction
 * with three ways on, ghosts out of the house, nothing eaten yet. The opening cell is the cheapest
 * position in the game and measuring it alone is how a search gets called affordable.
 */
#define OTT_LOOKAHEAD_COST_DECISIONS (200U)

/*! \brief Ticks between decisions before the run is given up on.
 *
 * Pacman reaches a new cell every seven ticks or so. This is the backstop for a run that has ended
 * without the loop noticing, not a working limit.
 */
#define OTT_LOOKAHEAD_COST_MAX_TICKS (64U)

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
    /* Not on the stack: a `game_t` is about 15 kB and the target reserves a kilobyte of it. */
    static game_t game;

    uint32_t decisions = 0U;
    uint32_t busy_ms = 0U;
    uint32_t worst_ms = 0U;
    uint32_t cells = 0U;
    uint32_t ticks_spent = 0U;
    uint32_t legs = 0U;
    uint32_t truncated = 0U;

    (void)in_parameter;

    cli_print("Look-ahead cost: what one decision costs on this board.");
    cli_print("depth %u, budget %u ticks, frame spare %u ms", (unsigned)PACMAN_LOOKAHEAD_MAX_DEPTH,
              (unsigned)PACMAN_LOOKAHEAD_DEFAULT_TICK_BUDGET, (unsigned)OTT_LOOKAHEAD_COST_SPARE_MS);

    game_init(&game);
    game_start_on_normal_maze(&game);

    cell_t decided_at = game_get_pacman_cell(&game);

    while ((decisions < OTT_LOOKAHEAD_COST_DECISIONS) && (game_get_state(&game) == GAME_STATE_RUNNING))
    {
        pacman_lookahead_report_t report;

        const uint32_t before = systick_bsp_get_tick();
        const direction_e chosen = pacman_lookahead_decide_within(&game, PACMAN_LOOKAHEAD_MAX_DEPTH,
                                                                  PACMAN_LOOKAHEAD_DEFAULT_TICK_BUDGET, &report);
        const uint32_t spent = systick_bsp_get_tick() - before;

        busy_ms += spent;

        if (spent > worst_ms)
        {
            /* The worst decision, not only the mean: a mean inside the budget with one decision far
             * outside it is a frame somebody sees drop. */
            worst_ms = spent;
        }

        cells += report.simulated_cells;
        ticks_spent += report.simulated_ticks;
        legs += report.examined_legs;
        truncated += report.was_truncated ? 1U : 0U;
        ++decisions;

        /* Play its answer, so the next decision is taken somewhere the search actually leads
         * rather than at a position picked by this test. A cost measured along a route nobody
         * would walk is a cost of the wrong thing. */
        game_set_direction(&game, chosen);

        uint32_t ticks = 0U;

        while ((ticks < OTT_LOOKAHEAD_COST_MAX_TICKS) && (game_get_state(&game) == GAME_STATE_RUNNING))
        {
            game_tick(&game, OTT_LOOKAHEAD_COST_STEP_MS);
            ++ticks;

            const cell_t now = game_get_pacman_cell(&game);

            if (!playfield_are_cells_equal(now, decided_at))
            {
                decided_at = now;
                break;
            }
        }
    }

    if (decisions == 0U)
    {
        (void)snprintf(out_reason, in_reason_size, "the run ended before a single decision was taken");

        return false;
    }

    /* Microseconds, because at 1 ms of tick resolution a per-decision figure in milliseconds is
     * mostly rounding. The total is what was measured; the division is this test's arithmetic. */
    const uint32_t mean_us = (busy_ms * OTT_LOOKAHEAD_COST_US_PER_MS) / decisions;
    const uint32_t cell_us = (cells > 0U) ? ((busy_ms * OTT_LOOKAHEAD_COST_US_PER_MS) / cells) : 0U;
    const uint32_t tick_us = (ticks_spent > 0U) ? ((busy_ms * OTT_LOOKAHEAD_COST_US_PER_MS) / ticks_spent) : 0U;

    cli_print("decisions %lu in %lu ms", (unsigned long)decisions, (unsigned long)busy_ms);
    cli_print("  mean  %lu us   worst %lu ms", (unsigned long)mean_us, (unsigned long)worst_ms);
    cli_print("  cells %lu (%lu per decision)   legs %lu", (unsigned long)cells, (unsigned long)(cells / decisions),
              (unsigned long)legs);
    cli_print("  a simulated tick costs %lu us, a cell %lu us", (unsigned long)tick_us, (unsigned long)cell_us);
    cli_print("  ticks %lu (%lu per decision)", (unsigned long)ticks_spent, (unsigned long)(ticks_spent / decisions));
    cli_print("  budget spent on %lu of %lu decisions", (unsigned long)truncated, (unsigned long)decisions);
    cli_print("score %lu, lives %u", (unsigned long)game_get_score(&game), (unsigned)game_get_lives(&game));

    if (worst_ms > OTT_LOOKAHEAD_COST_SPARE_MS)
    {
        (void)snprintf(out_reason, in_reason_size, "the worst decision took %lu ms of the %u a frame has spare",
                       (unsigned long)worst_ms, (unsigned)OTT_LOOKAHEAD_COST_SPARE_MS);

        return false;
    }

    return true;
}
