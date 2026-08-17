#include "ott_pacman_ai.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "Cli.h"
#include "ai_weights.h"
#include "game.h"
#include "game_session.h"
#include "joystick.h"
#include "msg.h"
#include "ott.h"
#include "pacman_ai.h"
#include "st7789.h"
#include "sw_timer.h"
#include "systick_bsp.h"
#include "user_button.h"

/* ==========================================================================
 * ott_pacman_ai - private
 * ========================================================================= */

/* Long, as `pacman`'s is: a level turning over and a life being lost are two of the four things the
 * operator has to see, and both take minutes at the arcade's own pace. */
#define OTT_PACMAN_AI_TIMEOUT_MS    (600000U)

#define OTT_PACMAN_AI_MS_PER_SECOND (1000U)

/* Enough frames that the 1 ms tick's granularity stops showing in the average. */
#define OTT_PACMAN_AI_TIMED_FRAMES  (300U)

/* Two presses of the same button mean different things here, so the confirm is the *stick's* centre
 * and the board button is the toggle — the other way round from every other manual test. Said out
 * loud on the console, because it is the one surprise in this scenario. */
static sw_timer_t g_timeout_timer;

static void prv_on_timeout(void)
{
    /* Nothing to do: the run loop watches sw_timer_is_active(). */
}

static void prv_poll_stick(void)
{
    static const struct
    {
        joystick_key_e key;
        direction_e direction;
    } k_stick[] = {
        {JOYSTICK_KEY_NORTH, DIRECTION_NORTH},
        {JOYSTICK_KEY_SOUTH, DIRECTION_SOUTH},
        {JOYSTICK_KEY_WEST, DIRECTION_WEST},
        {JOYSTICK_KEY_EAST, DIRECTION_EAST},
    };

    for (size_t index = 0U; index < (sizeof(k_stick) / sizeof(k_stick[0])); ++index)
    {
        if (joystick_is_pressed(k_stick[index].key))
        {
            game_session_set_direction(k_stick[index].direction);
        }
    }
}

/* Say what changed, so the run can be followed by someone who cannot see the panel — and so that
 * FR-033 leaves a trace on the console: a level or a life changing while the AI is still marked as
 * playing is the evidence. */
static void prv_report_progress(void)
{
    static game_state_e g_reported_state = GAME_STATE_IDLE;
    static uint8_t g_reported_level;
    static uint8_t g_reported_lives;
    static bool g_reported_ai = false;
    static bool g_has_reported = false;

    const game_state_e state = game_session_get_state();
    const uint8_t level = game_session_get_level();
    const uint8_t lives = game_session_get_lives();
    const bool is_ai = game_session_is_ai_enabled();

    if (g_has_reported && (state == g_reported_state) && (level == g_reported_level) && (lives == g_reported_lives)
        && (is_ai == g_reported_ai))
    {
        return;
    }

    g_has_reported = true;
    g_reported_state = state;
    g_reported_level = level;
    g_reported_lives = lives;
    g_reported_ai = is_ai;

    cli_print("  level %u - %u lives, %lu points, %s at the stick", (unsigned)level, (unsigned)lives,
              (unsigned long)game_session_get_score(), is_ai ? "AI" : "player");
}

/* What a frame costs with inference in it — the figure NFR-006 is about.
 *
 * Measured with the AI *on*, which is the point: the observation runs a breadth-first search over
 * 868 cells once per cell entered, and that is the only new work in the frame. Compared against
 * `pacman`'s own figure for the same board, the difference is what the agent costs.
 */
static void prv_measure_frame_cost(void)
{
    uint32_t busy_ms = 0U;
    uint32_t frames = 0U;
    const uint32_t start_tick = systick_bsp_get_tick();

    while (frames < OTT_PACMAN_AI_TIMED_FRAMES)
    {
        const uint32_t before = systick_bsp_get_tick();

        sw_timer_process();

        if (game_session_service())
        {
            busy_ms += systick_bsp_get_tick() - before;
            ++frames;
        }
    }

    const uint32_t elapsed_ms = systick_bsp_get_tick() - start_tick;

    cli_print("  %lu frames in %lu ms -> %lu fps, %lu ms of the %u ms budget spent", (unsigned long)frames,
              (unsigned long)elapsed_ms,
              (unsigned long)((frames * OTT_PACMAN_AI_MS_PER_SECOND) / ((elapsed_ms > 0U) ? elapsed_ms : 1U)),
              (unsigned long)(busy_ms / frames), (unsigned)GAME_SESSION_FRAME_PERIOD_MS);
}

/* ==========================================================================
 * ott_pacman_ai - public
 * ========================================================================= */

bool ott_pacman_ai_run(const uint8_t* in_parameter, char* out_reason, size_t in_reason_size)
{
    bool has_confirmed = false;

    (void)in_parameter;

    cli_print("Pac-Man AI: the trained agent playing on the board (FR-030..034).");
    cli_print("weights %s", AI_WEIGHTS_DIGEST);

    game_session_init();

    if (!st7789_is_present())
    {
        (void)snprintf(out_reason, in_reason_size, "the display controller does not answer");

        return false;
    }

    if (!pacman_ai_is_available())
    {
        (void)snprintf(out_reason, in_reason_size, "the weight table cannot be evaluated on this build");

        return false;
    }

    /* The normal maze, because that is the only maze the game offers the AI in (FR-040) and the one
     * it was evolved against. No seed is printed: there is nothing to reproduce, since every run of
     * this test plays the same maze — which is also what makes "it is still playing after a level
     * turns over" a comparison an operator can make between two runs. */
    game_session_start_on_normal_maze();

    if (!game_session_set_ai_enabled(true))
    {
        (void)snprintf(out_reason, in_reason_size, "the session refused to hand Pac-Man to the AI");

        return false;
    }

    prv_measure_frame_cost();

    cli_print("The AI has Pac-Man. Watch for, and confirm:");
    cli_print("  - the HUD shows AI between the score and the level (FR-032)");
    cli_print("  - the stick does nothing at all while it plays (FR-031)");
    cli_print("  - it is still playing after a level turns over and after a life is lost (FR-033)");
    cli_print("  - B1 hands Pac-Man back, and the AI in the HUD goes away; B1 again takes over");
    cli_print("B1 = hand over / take back. CENTER on the stick = pass this test.");
    cli_print("Note the button roles are swapped here: every other manual test confirms with B1.");
    cli_print("Times out after %u s to fail.", OTT_PACMAN_AI_TIMEOUT_MS / OTT_PACMAN_AI_MS_PER_SECOND);

    sw_timer_create(&g_timeout_timer);
    sw_timer_start(&g_timeout_timer, OTT_PACMAN_AI_TIMEOUT_MS, prv_on_timeout);

    while (sw_timer_is_active(&g_timeout_timer) && !has_confirmed)
    {
        /* The console stays alive through this one, as `pacman`'s does: ten minutes is long enough
         * that being unable to type `reset` is a nuisance (DEC-022). */
        ott_poll();
        sw_timer_process();
        prv_poll_stick();

        if (user_button_take_press())
        {
            (void)game_session_set_ai_enabled(!game_session_is_ai_enabled());
        }

        if (game_session_service())
        {
            prv_report_progress();
        }

        has_confirmed = joystick_take_press(JOYSTICK_KEY_CENTER);
    }

    sw_timer_stop(&g_timeout_timer);

    if (!has_confirmed)
    {
        (void)snprintf(out_reason, in_reason_size, "not confirmed at the board within %u s",
                       OTT_PACMAN_AI_TIMEOUT_MS / OTT_PACMAN_AI_MS_PER_SECOND);
    }

    return has_confirmed;
}
