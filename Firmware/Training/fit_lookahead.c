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

    /* Diagnostics, printed as fields *after* the two the fitting script reads — which is safe by design,
     * because it takes the first two and no more. */
    unsigned long decisions = 0UL;
    unsigned long walk_backs = 0UL;
    unsigned long long spent_ticks = 0ULL;
    unsigned long long walked_cells = 0ULL;
    unsigned long long examined_legs = 0ULL;
    unsigned long long reached_depth = 0ULL;
    unsigned long truncations = 0UL;

    /* Where the points come from, classified by the size of the score jump. That is exact because nothing
     * else in this game scores: a pellet is 10, a power pellet 50, and a ghost 200, 400, 800 or 1600 by how
     * many were caught inside the same frightened window. */
    unsigned long eaten_pellets = 0UL;
    unsigned long eaten_power = 0UL;
    unsigned long eaten_ghosts = 0UL;
    unsigned long ghost_points = 0UL;

    /* Why a run stopped. If most of them stop because the tick ceiling was reached rather than because the
     * lives ran out, then "levels reached" is a property of this harness and not of the player. */
    unsigned long ended_by_cap = 0UL;
    unsigned long ended_by_death = 0UL;
    unsigned long ended_by_winning = 0UL;
    unsigned long long ticks_used = 0ULL;

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

        /* Where the last decisions landed him, newest first. A walk-back is arriving on the cell he arrived
         * at two cells ago — he went there, left, and came straight back. */
        cell_t history[2] = {last, last};
        unsigned history_count = 0U;

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
            const uint32_t before_score = game_get_score(&g);

            game_tick(&g, 16U);
            ++t;

            /* A tick can carry a pellet *and* a ghost, so the pellet part is peeled off before the
             * remainder is matched; otherwise the pair falls through and both are lost. */
            uint32_t gained = game_get_score(&g) - before_score;
            const uint32_t pellet_parts[3] = {0U, 10U, 50U};

            for (unsigned index = 0U; (index < 3U) && (gained > 0U); ++index)
            {
                const uint32_t rest = gained - pellet_parts[index];

                if ((gained >= pellet_parts[index])
                    && ((rest == 200U) || (rest == 400U) || (rest == 800U) || (rest == 1600U)))
                {
                    ++eaten_ghosts;
                    ghost_points += rest;
                    eaten_pellets += (pellet_parts[index] == 10U) ? 1UL : 0UL;
                    eaten_power += (pellet_parts[index] == 50U) ? 1UL : 0UL;
                    gained = 0U;
                }
            }

            if (gained == 10U)
            {
                ++eaten_pellets;
            }
            else if (gained == 50U)
            {
                ++eaten_power;
            }
            else
            {
                /* Nothing scored, or something this split does not name. */
            }

            const cell_t now = game_get_pacman_cell(&g);
            if (first_cell || (now.x != last.x) || (now.y != last.y))
            {
                if (!first_cell)
                {
                    /* The decision thought while he crossed the last cell is over: bank what it cost, and
                     * ask whether it brought him back where he had just been. */
                    pacman_lookahead_report_t report;

                    pacman_lookahead_get_report(&report);

                    ++decisions;
                    spent_ticks += report.simulated_ticks;
                    walked_cells += report.simulated_cells;
                    examined_legs += report.examined_legs;
                    reached_depth += report.reached_depth;
                    truncations += report.was_truncated ? 1UL : 0UL;

                    if ((history_count >= 2U) && (now.x == history[1].x) && (now.y == history[1].y))
                    {
                        ++walk_backs;
                    }

                    history[1] = history[0];
                    history[0] = now;
                    ++history_count;
                }

                first_cell = false;
                last = now;
                pacman_lookahead_restart(&g, PACMAN_LOOKAHEAD_MAX_DEPTH, PACMAN_LOOKAHEAD_ANYTIME_TICK_BUDGET);
            }

            (void)pacman_lookahead_think(PACMAN_LOOKAHEAD_FRAME_SLICE_TICKS);
            game_set_direction(&g, pacman_lookahead_get_direction());
        }

        ticks_used += t;

        if (game_get_state(&g) == GAME_STATE_RUNNING)
        {
            ++ended_by_cap;
        }
        else if (game_get_state(&g) == GAME_STATE_WON)
        {
            ++ended_by_winning;
        }
        else
        {
            ++ended_by_death;
        }

        total += (double)game_get_score(&g);
        levels += game_get_level(&g);
    }

    const double taken = (decisions > 0UL) ? (double)decisions : 1.0;

    printf("%.1f %.2f %s walkback=%.1f%% ticks/decision=%.0f cells/decision=%.1f ticks/cell=%.1f "
           "legs/decision=%.1f depth/decision=%.2f truncated=%.1f%% decisions=%lu power=%lu ghosts=%lu "
           "ghosts/power=%.2f ghost_points=%lu pellet_points=%lu "
           "ended_by_cap=%lu ended_by_death=%lu won=%lu ticks/run=%.0f ticks/level=%.0f\n",
           total / runs, (double)levels / runs, is_generated ? "generated" : "normal",
           (100.0 * (double)walk_backs) / taken, (double)spent_ticks / taken, (double)walked_cells / taken,
           (walked_cells > 0ULL) ? ((double)spent_ticks / (double)walked_cells) : 0.0, (double)examined_legs / taken,
           (double)reached_depth / taken, (100.0 * (double)truncations) / taken, decisions, eaten_power, eaten_ghosts,
           (eaten_power > 0UL) ? ((double)eaten_ghosts / (double)eaten_power) : 0.0, ghost_points, eaten_pellets * 10UL,
           ended_by_cap, ended_by_death, ended_by_winning, (double)ticks_used / runs,
           (levels > 0UL) ? ((double)ticks_used / (double)levels) : 0.0);
    return 0;
}
