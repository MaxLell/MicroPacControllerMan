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
#define OTT_SEARCH_BUDGET_SWEEPS   (8U)

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

    /* Two targets, because the two cases cost very different amounts and the game uses both.
     * Blinky aims at Pacman's own cell, which a ghost can stand on, so the ring expansion stops the
     * moment it arrives. Pinky aims four cells ahead of him and Inky at a reflection of Blinky —
     * both land in a wall routinely — and the scatter targets sit in unreachable dead space on
     * purpose (DEC-023). Those have to look at every reachable cell to find the nearest one.
     *
     * Blinky's *start* cell is not the unreachable one to use, and was here first: the arcade puts
     * him on the tile above the gate, which is outside the house and perfectly walkable, so both
     * passes measured the same case and reported the same number. */
    const cell_t reachable_target = playfield_get_pacman_start(&playfield);
    const cell_t unreachable_target = playfield_get_scatter_target(0U);
    cell_t target = unreachable_target;
    uint32_t pass;

    /* Every open cell of the maze, each entered from one of its own neighbours — not one junction
     * measured over and over. A ghost spends most of its life in corridors, where there is one way
     * out and no search to do, so a figure taken at a junction prices the worst case and says
     * nothing about what a step actually costs. */
    uint32_t calls = 0U;
    uint32_t wall_ms = 0U;
    uint32_t start_tick = systick_bsp_get_tick();

    for (pass = 0U; pass < 2U; ++pass)
    {
        target = (pass == 0U) ? unreachable_target : reachable_target;
        calls = 0U;
        start_tick = systick_bsp_get_tick();

        for (index = 0U; index < OTT_SEARCH_BUDGET_SWEEPS; ++index)
        {
            for (int16_t y = 0; y < PLAYFIELD_HEIGHT; ++y)
            {
                for (int16_t x = 0; x < PLAYFIELD_WIDTH; ++x)
                {
                    const cell_t from = {x, y};

                    if (!playfield_is_walkable(&playfield, from) || playfield_is_house(&playfield, from))
                    {
                        continue;
                    }

                    /* Where it came from is one of its own open neighbours, so the way back is barred
                 * the way it really is when a ghost walks. Forbidding a fixed compass direction
                 * instead prices cells the ghost could never have entered that way, and a corridor
                 * then looks like a choice when it is not. */
                    static const direction_e k_order[] = {DIRECTION_NORTH, DIRECTION_SOUTH, DIRECTION_EAST,
                                                          DIRECTION_WEST};
                    direction_e came_from = DIRECTION_NONE;

                    for (size_t which = 0U; which < (sizeof(k_order) / sizeof(k_order[0])); ++which)
                    {
                        const cell_t neighbour = playfield_step(from, k_order[which]);

                        if (playfield_is_walkable(&playfield, neighbour) && !playfield_is_house(&playfield, neighbour))
                        {
                            came_from = k_order[which];
                            break;
                        }
                    }

                    (void)ghost_path_find_step_towards(&playfield, from, target, came_from, false);
                    ++calls;
                }
            }
        }

        if (pass == 0U)
        {
            wall_ms = (systick_bsp_get_tick() - start_tick);
            cli_print("  target in a wall:  %lu steps in %lu ms -> %lu us each", (unsigned long)calls,
                      (unsigned long)wall_ms, (unsigned long)((wall_ms * 1000U) / calls));
            continue;
        }
    }

    cli_print("  target reachable:  %lu steps in %lu ms -> %lu us each", (unsigned long)calls,
              (unsigned long)(systick_bsp_get_tick() - start_tick),
              (unsigned long)(((systick_bsp_get_tick() - start_tick) * 1000U) / calls));

    /* The budget is figured on the *slower* of the two. Half of what the ghosts aim at is a wall or
     * off the board by design, and a look-ahead sized on the cheap case would come up short exactly
     * when the ghosts are scattering. */
    const uint32_t elapsed_ms = wall_ms;
    const uint32_t safe_ms = (elapsed_ms > 0U) ? elapsed_ms : 1U;

    /* Multiplied before divided, all the way through: at these rates a route search costs a
     * fraction of a millisecond and every intermediate division rounds the answer to zero. */
    const uint32_t microseconds_per_search = (safe_ms * 1000U) / calls;
    const uint32_t cells_per_frame = (calls * OTT_SEARCH_BUDGET_SPARE_MS) / (safe_ms * OTT_SEARCH_BUDGET_GHOSTS);
    const uint32_t branches = cells_per_frame / OTT_SEARCH_BUDGET_CORRIDOR;

    cli_print("  a look-ahead pays the slower of the two: %lu us a ghost step", (unsigned long)microseconds_per_search);
    cli_print("  a simulated cell costs %u of them; %u ms of spare frame buys %lu cells",
              (unsigned)OTT_SEARCH_BUDGET_GHOSTS, (unsigned)OTT_SEARCH_BUDGET_SPARE_MS, (unsigned long)cells_per_frame);
    cli_print("  that is about %lu junction branches of %u cells", (unsigned long)branches,
              (unsigned)OTT_SEARCH_BUDGET_CORRIDOR);

    (void)snprintf(out_reason, in_reason_size, "%lu us a search, %lu cells or %lu branches a frame",
                   (unsigned long)microseconds_per_search, (unsigned long)cells_per_frame, (unsigned long)branches);

    return cells_per_frame > 0U;
}
