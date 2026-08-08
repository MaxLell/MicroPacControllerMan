#include "ott_search_budget.h"

#include <stdio.h>

#include "Cli.h"
#include "ghost_path.h"
#include "playfield.h"
#include "systick_bsp.h"

/* ==========================================================================
 * ott_search_budget - private
 * ========================================================================= */

/*! \brief Route searches timed. Enough that the 1 ms tick stops rounding the answer away. */
#define OTT_SEARCH_BUDGET_SEARCHES (2000U)

/*! \brief Ghosts whose next cell has to be worked out for every cell of simulated future. */
#define OTT_SEARCH_BUDGET_GHOSTS   (4U)

/*! \brief What a frame may spend on looking ahead.
 *
 * The owner allowed the frame rate to fall to 50 fps, so the period may be 20 ms, and a frame
 * measured 7 ms of drawing and deciding. Thirteen is what is left. */
#define OTT_SEARCH_BUDGET_SPARE_MS (13U)

/*! \brief Cells of simulated future between one junction and the next, near enough.
 *
 * The arcade maze's corridors run about this long, and a search over *junctions* has to simulate
 * every cell between them. */
#define OTT_SEARCH_BUDGET_CORRIDOR (8U)

/* ==========================================================================
 * ott_search_budget - public
 * ========================================================================= */

bool ott_search_budget_run(const uint8_t* in_parameter, char* out_reason, size_t in_reason_size)
{
    static playfield_t playfield;
    uint32_t index;

    (void)in_parameter;

    cli_print("Search budget: what a cell of look-ahead costs on this board.");

    playfield_load(&playfield);

    const cell_t from = playfield_get_pacman_start(&playfield);
    const cell_t target = playfield_get_ghost_start(&playfield, 0U);

    /* Timed as a block rather than one call at a time: a single route search is far below the 1 ms
     * tick, so the only honest way to price it is to do thousands and divide. */
    const uint32_t start_tick = systick_bsp_get_tick();

    for (index = 0U; index < OTT_SEARCH_BUDGET_SEARCHES; ++index)
    {
        /* The target moves a little so that nothing can be hoisted or cached by accident — a
         * measurement of a call the compiler folded away is worse than no measurement. */
        const cell_t moving = {(int16_t)(target.x + (int16_t)(index & 1U)), target.y};

        (void)ghost_path_find_step_towards(&playfield, from, moving, DIRECTION_NONE, false);
    }

    const uint32_t elapsed_ms = (systick_bsp_get_tick() - start_tick);
    const uint32_t safe_ms = (elapsed_ms > 0U) ? elapsed_ms : 1U;

    /* Multiplied before divided, all the way through: at these rates a route search costs a
     * fraction of a millisecond and every intermediate division rounds the answer to zero. */
    const uint32_t microseconds_per_search = (safe_ms * 1000U) / OTT_SEARCH_BUDGET_SEARCHES;
    const uint32_t cells_per_frame =
        (OTT_SEARCH_BUDGET_SEARCHES * OTT_SEARCH_BUDGET_SPARE_MS) / (safe_ms * OTT_SEARCH_BUDGET_GHOSTS);
    const uint32_t branches = cells_per_frame / OTT_SEARCH_BUDGET_CORRIDOR;

    cli_print("  %lu route searches in %lu ms -> %lu us each", (unsigned long)OTT_SEARCH_BUDGET_SEARCHES,
              (unsigned long)elapsed_ms, (unsigned long)microseconds_per_search);
    cli_print("  a simulated cell costs %u of them; %u ms of spare frame buys %lu cells",
              (unsigned)OTT_SEARCH_BUDGET_GHOSTS, (unsigned)OTT_SEARCH_BUDGET_SPARE_MS, (unsigned long)cells_per_frame);
    cli_print("  that is about %lu junction branches of %u cells", (unsigned long)branches,
              (unsigned)OTT_SEARCH_BUDGET_CORRIDOR);

    (void)snprintf(out_reason, in_reason_size, "%lu us a search, %lu cells or %lu branches a frame",
                   (unsigned long)microseconds_per_search, (unsigned long)cells_per_frame, (unsigned long)branches);

    return cells_per_frame > 0U;
}
