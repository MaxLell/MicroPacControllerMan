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
 */
#include <stdio.h>
#include <stdlib.h>

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

    double total = 0.0;
    unsigned long levels = 0;

    for (uint32_t run = 0U; run < runs; ++run)
    {
        rng_bsp_seed(first + run);
        game_init(&g);
        game_start_on_normal_maze(&g);

        cell_t last = game_get_pacman_cell(&g);
        bool first_cell = true;
        unsigned t = 0U;

        /* A ceiling on simulated time, so one lucky policy cannot hold a whole generation up. */
        while ((game_get_state(&g) == GAME_STATE_RUNNING) && (t < 120000U))
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

    printf("%.1f %.2f\n", total / runs, (double)levels / runs);
    return 0;
}
