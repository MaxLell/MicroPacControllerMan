#include "ott_ai_high_score.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "Cli.h"
#include "game_session.h"
#include "high_score.h"
#include "ott.h"
#include "pacman_lookahead.h"
#include "shell.h"
#include "st7789.h"
#include "sw_timer.h"
#include "systick_bsp.h"

/* ==========================================================================
 * ott_ai_high_score - private
 * ========================================================================= */

/* Long enough for two runs to game over. Measured on the host: an episode the trained agent plays
 * ends after 192 to 460 decisions, and a decision is about 167 ms at level-1 speed — so a run is
 * one to two minutes and two of them are four at the outside. */
#define RUN_TIMEOUT_MS           (240000U)

#define MS_PER_SECOND            (1000U)

/* Which way the player half pushes. One fixed direction rather than something clever: the point of
 * that run is to *score* and then die, not to play well, and a held direction does both — Pacman
 * eats his way into a corner and a ghost finishes it. */
#define PLAYER_DIRECTION         (DIRECTION_WEST)

/* How much the agent scores before control is handed back. Enough that the run has a score worth
 * refusing, small enough that it happens in a few seconds — about twenty pellets. */
#define AI_SCORE_BEFORE_HANDBACK (200U)

static sw_timer_t g_timeout_timer;

/* The three scores as they were when the test started, so the board is left as it was found. */
/* Every table, because the test clears every table: the run it plays is a normal-maze run, and part
 * of what it checks is that the *other* two are left alone. */
static uint32_t g_saved_scores[HIGH_SCORE_TABLE_COUNT][HIGH_SCORE_COUNT];

static void prv_on_timeout(void)
{
    /* Nothing to do: the loops watch sw_timer_is_active(). */
}

/* One turn of the loop `app_main` runs, minus the input: the console stays alive so `reset` still
 * works through a four-minute test. */
static void prv_service(void)
{
    ott_poll();
    sw_timer_process();
    (void)shell_service();
}

/* Wait for the shell to reach a screen, or give up when the timer runs out. */
static bool prv_wait_for_screen(shell_screen_e in_screen, bool in_is_player_steering)
{
    while (sw_timer_is_active(&g_timeout_timer) && (shell_get_screen() != in_screen))
    {
        if (in_is_player_steering && (shell_get_screen() == SHELL_SCREEN_GAME))
        {
            /* Asked every turn, because the stick is read as a level and the turn only happens at
             * the first cell where it becomes possible. */
            shell_set_direction(PLAYER_DIRECTION);
        }

        prv_service();
    }

    return shell_get_screen() == in_screen;
}

/* Play one run from the menu to the score screen, and report what it scored.
 *
 * `in_is_ai` decides who plays, and since DEC-054 it decides it **on the menu**: handing Pac-Man
 * over mid-run went with the trained network, so the AI's run is the AI's *game* from the first
 * frame and a person's run is a person's game throughout.
 */
static bool prv_play_one_run(bool in_is_ai, uint32_t* const out_score, char* out_reason, size_t in_reason_size)
{
    if (!prv_wait_for_screen(SHELL_SCREEN_MENU, false))
    {
        (void)snprintf(out_reason, in_reason_size, "the menu never came up");

        return false;
    }

    /* **The search is made hopeless for this test, on purpose.** What is being checked is FR-034's
     * lockout — which table a run reaches — and not how well the search plays. With the weights the
     * firmware ships it reaches level six, and a whole run of that is many minutes of wall clock:
     * this test timed out at 240 s before the weights were touched. Zero weights leave every
     * position worth the same, so it falls back to the first way out of each cell and dies in the
     * time a suite can spend. It is the shipped code either way, driven through its own public
     * setter, and the defaults are put back before the test returns. */
    if (in_is_ai)
    {
        pacman_lookahead_weights_t hopeless = {0};

        hopeless.death = 1;

        pacman_lookahead_set_weights(&hopeless);
    }

    /* Who plays, on the menu's top row, which since DEC-055 is the only place it is decided. The
     * maze row is left where it is: both halves of this test play the arcade's layout. */
    while (shell_get_selected_row() != SHELL_ROW_PLAYER)
    {
        shell_move_selection(DIRECTION_NORTH);
    }

    if ((shell_get_selected_player() == SHELL_PLAYER_MACHINE) != in_is_ai)
    {
        shell_move_selection(DIRECTION_EAST);
    }

    shell_press_start();

    if (shell_get_screen() != SHELL_SCREEN_GAME)
    {
        (void)snprintf(out_reason, in_reason_size, "start did not begin a run");

        return false;
    }

    if (in_is_ai)
    {
        /* Let it actually play for a while: a run that scores nothing cannot show a lockout, because
         * a refused zero looks exactly like a working one. */
        while (sw_timer_is_active(&g_timeout_timer) && (shell_get_screen() == SHELL_SCREEN_GAME)
               && (game_session_get_score() < AI_SCORE_BEFORE_HANDBACK))
        {
            prv_service();
        }

        if (!shell_has_ai_played())
        {
            (void)snprintf(out_reason, in_reason_size, "the run does not remember that the AI played");

            return false;
        }
    }

    /* The player's half is steered by hand from here; the AI's steers itself and must not be
     * pushed, because FR-031 makes the stick dead while it plays and pushing would only be noise. */
    if (!prv_wait_for_screen(SHELL_SCREEN_SCORE, !in_is_ai))
    {
        (void)snprintf(out_reason, in_reason_size, "the %s run did not finish within %u s", in_is_ai ? "AI" : "player",
                       RUN_TIMEOUT_MS / MS_PER_SECOND);

        return false;
    }

    *out_score = game_session_get_score();

    pacman_lookahead_set_weights(NULL);

    /* Off the score screen, so the next run can start from the menu. */
    shell_press_start();

    return true;
}

/* ==========================================================================
 * ott_ai_high_score - public
 * ========================================================================= */

bool ott_ai_high_score_run(const uint8_t* in_parameter, char* out_reason, size_t in_reason_size)
{
    uint32_t ai_score = 0U;
    uint32_t player_score = 0U;

    (void)in_parameter;

    cli_print("AI high-score lockout: a run the AI touched must not reach NVM (FR-034).");
    cli_print("Two runs played to game over, so this one takes a few minutes.");

    /* The shell first, because it is what brings `render` — and therefore the panel — up. Asking
     * the controller before that is asking a bus nobody has configured. */
    shell_init();
    (void)shell_service();

    if (!st7789_is_present())
    {
        (void)snprintf(out_reason, in_reason_size, "the display controller does not answer");

        return false;
    }

    for (uint8_t table = 0U; table < HIGH_SCORE_TABLE_COUNT; ++table)
    {
        for (uint8_t place = 0U; place < HIGH_SCORE_COUNT; ++place)
        {
            g_saved_scores[table][place] = high_score_get(table, place);
        }
    }

    if (!high_score_reset())
    {
        (void)snprintf(out_reason, in_reason_size, "the table could not be cleared");

        return false;
    }

    sw_timer_create(&g_timeout_timer);
    sw_timer_start(&g_timeout_timer, RUN_TIMEOUT_MS, prv_on_timeout);

    bool has_passed = false;

    if (prv_play_one_run(true, &ai_score, out_reason, in_reason_size))
    {
        /* Two tables, because since DEC-054 the AI's run *is* the agent's own game: it belongs in
         * the agent's table (FR-041) and must stay out of the person's (FR-034). Before DEC-054 the
         * AI played a normal-maze run the player had handed over, which belonged in no table at all
         * — the same requirement, a different run. */
        const uint32_t persons_table_after_ai = high_score_get_best((uint8_t)SHELL_MODE_PLAY_CLASSIC);
        const uint32_t agents_table_after_ai = high_score_get_best((uint8_t)SHELL_MODE_AI_CLASSIC);

        cli_print("  the AI's run scored %lu; its own table holds %lu, the person's %lu", (unsigned long)ai_score,
                  (unsigned long)agents_table_after_ai, (unsigned long)persons_table_after_ai);

        if (ai_score == 0U)
        {
            /* A refused nothing looks exactly like a working lockout, so it is not allowed to
             * count as one. */
            (void)snprintf(out_reason, in_reason_size, "the AI's run scored nothing, so the lockout proves nothing");
        }
        else if (persons_table_after_ai != 0U)
        {
            (void)snprintf(out_reason, in_reason_size, "the AI's run of %lu reached the person's table",
                           (unsigned long)ai_score);
        }
        else if (agents_table_after_ai != ai_score)
        {
            /* And the other side of FR-041: a lockout that refused *every* table would pass the
             * assertion above and be wrong. The agent keeps its own scoreboard. */
            (void)snprintf(out_reason, in_reason_size,
                           "the AI's run of %lu did not reach the agent's own table, which holds %lu",
                           (unsigned long)ai_score, (unsigned long)agents_table_after_ai);
        }
        else
        {
            /* The other half: the table still works. A lower score than the AI's is not required
             * for the requirement, but it is what makes "however high the score" visible. */
            sw_timer_start(&g_timeout_timer, RUN_TIMEOUT_MS, prv_on_timeout);

            if (prv_play_one_run(false, &player_score, out_reason, in_reason_size))
            {
                cli_print("  the player's run scored %lu, the table holds %lu", (unsigned long)player_score,
                          (unsigned long)high_score_get_best((uint8_t)SHELL_MODE_PLAY_CLASSIC));

                if (player_score == 0U)
                {
                    (void)snprintf(out_reason, in_reason_size, "the player's run scored nothing");
                }
                else if (high_score_get_best((uint8_t)SHELL_MODE_PLAY_CLASSIC) != player_score)
                {
                    (void)snprintf(out_reason, in_reason_size, "the player's run of %lu did not reach the table",
                                   (unsigned long)player_score);
                }
                else
                {
                    has_passed = true;
                }
            }
        }
    }

    sw_timer_stop(&g_timeout_timer);

    /* Put the table back whatever happened, so a failing test does not also cost somebody their
     * high scores. */
    (void)high_score_reset();

    for (uint8_t table = 0U; table < HIGH_SCORE_TABLE_COUNT; ++table)
    {
        for (uint8_t place = HIGH_SCORE_COUNT; place > 0U; --place)
        {
            if (g_saved_scores[table][place - 1U] != 0U)
            {
                (void)high_score_offer(table, g_saved_scores[table][place - 1U]);
            }
        }
    }

    return has_passed;
}
