#include "ott_pacman.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "Cli.h"
#include "difficulty.h"
#include "game.h"
#include "game_session.h"
#include "joystick.h"
#include "msg.h"
#include "ott.h"
#include "st7789.h"
#include "sw_timer.h"
#include "systick_bsp.h"
#include "user_button.h"

/* ==========================================================================
 * ott_pacman - private
 * ========================================================================= */

/* Long, because this one is played rather than checked: it is the only chance to see a
 * ghost leave the house, a power pellet take effect and a level turn over, and all three
 * take minutes at the arcade's own pace. */
#define OTT_PACMAN_TIMEOUT_MS    (600000U)

#define OTT_PACMAN_MS_PER_SECOND (1000U)

/* Enough frames that the 1 ms tick's granularity stops showing in the average. */
#define OTT_PACMAN_TIMED_FRAMES  (300U)

static sw_timer_t g_timeout_timer;

static void prv_on_timeout(void)
{
    /* Nothing to do: the run loop watches sw_timer_is_active(). */
}

/* The stick, read as a level rather than as an edge — the same reading `app_main` does,
 * and for the same reason: an arcade stick is held, and holding a direction has to keep
 * asking for it because the turn only happens at the first cell where it becomes
 * possible (§10.1). */
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

/* Say what the run is doing when it changes, so it can be followed on the console by
 * someone who cannot see the panel. Only on a change: a line per pellet would flood the
 * console and upset the silence `run_ott.py` waits for. */
static void prv_report_progress(void)
{
    static game_state_e g_reported_state = GAME_STATE_IDLE;
    static uint8_t g_reported_level;
    static uint8_t g_reported_lives;

    const game_state_e state = game_session_get_state();
    const uint8_t level = game_session_get_level();
    const uint8_t lives = game_session_get_lives();

    if ((state == g_reported_state) && (level == g_reported_level) && (lives == g_reported_lives))
    {
        return;
    }

    g_reported_state = state;
    g_reported_level = level;
    g_reported_lives = lives;

    cli_print("  level %u - %u lives, %lu points", (unsigned)level, (unsigned)lives,
              (unsigned long)game_session_get_score());
}

/* What a frame of the real game costs on this board — the on-target number behind NFR-002.
 *
 * Measured over the first frames of an untouched run, which is the honest case: every actor
 * is moving and the view is redrawing all five of them plus the pellets they eat. The
 * session paces itself, so the figure to watch is not the period — that is the timer's — but
 * how much of the period is left over.
 */
static void prv_measure_frame_cost(void)
{
    uint32_t busy_ms = 0U;
    const uint32_t start_tick = systick_bsp_get_tick();
    uint32_t frames = 0U;
    uint32_t elapsed_ms;

    while (frames < OTT_PACMAN_TIMED_FRAMES)
    {
        const uint32_t before = systick_bsp_get_tick();

        sw_timer_process();

        if (game_session_service())
        {
            busy_ms += systick_bsp_get_tick() - before;
            ++frames;
        }
    }

    elapsed_ms = systick_bsp_get_tick() - start_tick;

    cli_print("  %lu frames in %lu ms -> %lu fps, %lu ms of the %u ms budget spent drawing", (unsigned long)frames,
              (unsigned long)elapsed_ms,
              (unsigned long)((frames * OTT_PACMAN_MS_PER_SECOND) / ((elapsed_ms > 0U) ? elapsed_ms : 1U)),
              (unsigned long)(busy_ms / frames), (unsigned)GAME_SESSION_FRAME_PERIOD_MS);
}

/* ==========================================================================
 * ott_pacman - public
 * ========================================================================= */

bool ott_pacman_run(const uint8_t* in_parameter, char* out_reason, size_t in_reason_size)
{
    bool has_confirmed = false;

    (void)in_parameter;

    cli_print("Pacman: the game itself, on the panel — the end-to-end test for M3.");

    /* `render` brings the display up, but it cannot tell whether anything answered, and a
     * silent SPI bus would otherwise look like a game that renders nothing. */
    game_session_init();

    if (!st7789_is_present())
    {
        (void)snprintf(out_reason, in_reason_size, "the display controller does not answer");

        return false;
    }

    game_session_start();

    prv_measure_frame_cost();

    cli_print("Play it. The stick steers Pacman, and CENTER starts the next run once this");
    cli_print("one is over. Watch for: the maze drawn whole, Pacman turning where he can");
    cli_print("and only there, the ghosts leaving the house one at a time, a power pellet");
    cli_print("turning them blue, and the level number going up when the maze is empty.");
    cli_print("Press B1 to pass, or let it time out after %u s to fail.",
              OTT_PACMAN_TIMEOUT_MS / OTT_PACMAN_MS_PER_SECOND);
    cli_print("There are %u levels to win (FR-027) — nobody is expected to sit through them.",
              (unsigned)DIFFICULTY_FINAL_LEVEL);

    sw_timer_create(&g_timeout_timer);
    sw_timer_start(&g_timeout_timer, OTT_PACMAN_TIMEOUT_MS, prv_on_timeout);

    while (sw_timer_is_active(&g_timeout_timer) && !has_confirmed)
    {
        /* The console stays alive through this one, which no other scenario bothers with:
         * ten minutes is long enough that being unable to type `reset` is a nuisance, and
         * a game with a console alongside it is what normal operation looks like anyway
         * (DEC-022). A command that schedules another test resets the board, which ends
         * this one the same way the timeout would. */
        ott_poll();
        sw_timer_process();
        prv_poll_stick();

        if (joystick_take_press(JOYSTICK_KEY_CENTER) && (game_session_get_state() != GAME_STATE_RUNNING))
        {
            game_session_start();
        }

        if (game_session_service())
        {
            prv_report_progress();
        }

        has_confirmed = user_button_take_press();
    }

    sw_timer_stop(&g_timeout_timer);

    if (!has_confirmed)
    {
        (void)snprintf(out_reason, in_reason_size, "not confirmed at the board within %u s",
                       OTT_PACMAN_TIMEOUT_MS / OTT_PACMAN_MS_PER_SECOND);
    }

    return has_confirmed;
}
