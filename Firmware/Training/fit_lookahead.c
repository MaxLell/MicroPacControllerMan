/*
 * fit_lookahead.c
 *
 * One candidate of `fit_lookahead.py`: plays whole runs of the look-ahead player with a given set of
 * evaluation weights and prints the mean score and the mean level reached.
 *
 * **It drives the shipped loop**, in the shipped order — restart on a new cell,
 * `pacman_lookahead_think` a slice a frame, read the direction every frame — so what is being fitted
 * is the player that ships and not a lookalike. That is the same argument DEC-042 makes about
 * training the network against the C evaluator: one implementation, so the fit cannot be measuring
 * something the board does not do.
 *
 * A separate process per candidate rather than threads, because `pacman_lookahead` is not reentrant
 * — its clones and its search stack are file-scope — and because it lets the driver use every core
 * without any of that mattering.
 *
 * usage: fit_lookahead <first_seed> <runs> <point> <death> <threat> <prey> <food> <escape>
 *
 * `FIT_MAZE=generated` plays a maze generated per level (FR-029) instead of the arcade's own layout.
 * The weights are fitted on the arcade's, because that is the maze the AI has always been offered in;
 * the switch exists to answer the other question — whether a search whose evaluation is fitted on one
 * layout still plays a layout it has never seen. It should, because nothing in the search or the scan
 * knows a particular maze, but "should" is not "does".
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game.h"
#include "pacman_lookahead.h"
#include "rng_bsp.h"

static game_t g;

int main(int argc, char** argv)
{
    if (argc < 9)
    {
        fprintf(stderr, "usage: fitness first_seed runs point death threat prey food escape\n");
        return 2;
    }

    const uint32_t first = (uint32_t)strtoul(argv[1], NULL, 10);
    const uint32_t runs = (uint32_t)strtoul(argv[2], NULL, 10);

    pacman_lookahead_weights_t w;
    w.point = (int32_t)strtol(argv[3], NULL, 10);
    w.death = (int32_t)strtol(argv[4], NULL, 10);
    w.threat = (int32_t)strtol(argv[5], NULL, 10);
    w.prey = (int32_t)strtol(argv[6], NULL, 10);
    w.food = (int32_t)strtol(argv[7], NULL, 10);
    w.escape = (int32_t)strtol(argv[8], NULL, 10);

    pacman_lookahead_set_weights(&w);

    const char* const maze = getenv("FIT_MAZE");
    const bool is_generated = (maze != NULL) && (strcmp(maze, "generated") == 0);

    double total = 0.0;
    unsigned long levels = 0;

    for (uint32_t run = 0U; run < runs; ++run)
    {
        rng_bsp_seed(first + run);
        game_init(&g);

        if (is_generated)
        {
            /* The seed doubles as the maze's: one number, so a run is reproducible whole. */
            game_start(&g, first + run);
        }
        else
        {
            game_start_on_normal_maze(&g);
        }

        cell_t last = game_get_pacman_cell(&g);
        bool first_cell = true;
        unsigned t = 0U;

        /* **Points in a fixed time, not points however long it takes.**
         *
         * This was a ceiling of 120,000 ticks — 32 minutes of game time — chosen when a run ended long
         * before it. It does not any more: a candidate that weights survival heavily simply *hides*,
         * reaches the ceiling on all sixteen runs, and holds a core for hours. Measured, one
         * generation of nine candidates took **fourteen hours** where the incumbent's sixteen runs
         * took two and a half minutes.
         *
         * Thirty thousand ticks is eight minutes of game time, and it changes what is being fitted
         * rather than only what it costs — which is the point. A fitness with no time limit rewards
         * a policy that survives without scoring; this one rewards getting on with it, which is
         * exactly the complaint that started this branch: a third of every level was being spent
         * wandering after the last few pellets. */
        while ((game_get_state(&g) == GAME_STATE_RUNNING) && (t < 30000U))
        {
            game_tick(&g, 16U);
            ++t;

            const cell_t now = game_get_pacman_cell(&g);
            if (first_cell || (now.x != last.x) || (now.y != last.y))
            {
                first_cell = false;
                last = now;
                pacman_lookahead_restart(&g, PACMAN_LOOKAHEAD_MAX_DEPTH, PACMAN_LOOKAHEAD_ANYTIME_TICK_BUDGET);
            }

            (void)pacman_lookahead_think(PACMAN_LOOKAHEAD_FRAME_SLICE_TICKS);
            game_set_direction(&g, pacman_lookahead_get_direction());
        }

        total += (double)game_get_score(&g);
        levels += game_get_level(&g);
    }

    printf("%.1f %.2f %s\n", total / runs, (double)levels / runs, is_generated ? "generated" : "normal");
    return 0;
}
