#include "app_main.h"

#include <stdbool.h>
#include <stdint.h>

#include "Cli.h"
#include "console.h"
#include "difficulty.h"
#include "dio_bsp.h"
#include "game.h"
#include "game_session.h"
#include "joystick.h"
#include "msg.h"
#include "ott.h"
#include "spi_bsp.h"
#include "sw_timer.h"
#include "systick_bsp.h"
#include "user_button.h"

/* ==========================================================================
 * app_main - private
 * ========================================================================= */

#define APP_MAIN_BOOT_BANNER "MicroPacControllerMan booted. Type 'ott' for tests, 'reset' to restart the game."

static void prv_on_systick(void)
{
    (void)user_button_poll();
    joystick_poll();

    /* The console rides the tick for the same reason the buttons do: the receive register
     * holds one character and has no FIFO, and once a frame is being drawn the main loop
     * is away for milliseconds at a time. */
    console_poll_receive();
}

static void prv_init_platform(void)
{
    systick_bsp_init();
    dio_bsp_init();
    console_init();
    spi_bsp_init();
    sw_timer_init();
    user_button_init();
    joystick_init();

    /* Debouncing needs a steady 1 ms sample rate, so it rides the tick interrupt
     * rather than the main loop. */
    systick_bsp_register_tick_callback(prv_on_systick);
}

/* The stick, once per frame.
 *
 * Read as a *level* rather than as an edge: an arcade stick is held, and holding a
 * direction has to keep asking for it, because the turn only happens at the first cell
 * where it becomes possible (§10.1). Letting go asks for nothing, which leaves the last
 * request standing — which is what a player expects mid-corridor.
 */
static void prv_poll_input(void)
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

    for (uint8_t index = 0U; index < (sizeof(k_stick) / sizeof(k_stick[0])); ++index)
    {
        if (joystick_is_pressed(k_stick[index].key))
        {
            game_session_set_direction(k_stick[index].direction);
        }
    }

    /* The centre press starts the next run once this one is over (FR-003). Taken as an
     * edge, so a thumb resting on it does not restart the game every frame. */
    if (joystick_take_press(JOYSTICK_KEY_CENTER) && (game_session_get_state() != GAME_STATE_RUNNING))
    {
        game_session_start();
    }
}

/* Say what the run is doing, on the console, when it changes.
 *
 * FR-107's principle applied to the game itself: the board reports over the serial line so
 * nothing needs a debugger to be understood. It also makes the game observable to someone
 * who cannot see the panel — which is how the missing timer re-arm above was found, because
 * the loop was silent where it should have been reporting.
 *
 * Only on a change, and the things that change are rare: a level, a life, the end of a run.
 * A line per pellet would flood the console and upset the silence `run_ott.py` waits for. */
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

    switch (state)
    {
        case GAME_STATE_RUNNING:
            cli_print("level %u - %u lives, %lu points", (unsigned)level, (unsigned)lives,
                      (unsigned long)game_session_get_score());
            break;

        case GAME_STATE_OVER:
            cli_print("game over on level %u with %lu points - centre key to play again", (unsigned)level,
                      (unsigned long)game_session_get_score());
            break;

        case GAME_STATE_WON:
            cli_print("all %u levels cleared with %lu points - centre key to play again",
                      (unsigned)DIFFICULTY_FINAL_LEVEL, (unsigned long)game_session_get_score());
            break;

        default: break;
    }
}

/* Normal operation: the game, with the console alongside it. Never returns — the way out
 * is `ott <name>` or `reset`, and both reboot the board. */
static void prv_run_game(void)
{
    game_session_init();
    game_session_start();

    for (;;)
    {
        /* The console shares the loop rather than the frame: a command arriving mid-frame
         * is answered at the end of it, which is well inside a keystroke. */
        ott_poll();
        sw_timer_process();
        prv_poll_input();

        if (game_session_service())
        {
            prv_report_progress();
        }
    }
}

/* ==========================================================================
 * app_main - public
 * ========================================================================= */

void app_main(void)
{
    prv_init_platform();

    /* The command line comes up first: it clears the screen, and everything below
     * reports through it — including a pending test's verdict. */
    ott_init();

    cli_print(APP_MAIN_BOOT_BANNER);

    if (ott_execute_pending())
    {
        /* This boot belongs to the test that asked for it. Scheduling one resets the
         * board, so a test and the game are alternatives rather than a sequence: starting
         * the game now would paint over whatever the test left on the panel, and the
         * harness is still reading the verdict off the same serial line. */
        cli_print("test complete — the board is idle. Type 'reset' to start the game.");

        for (;;)
        {
            ott_poll();
        }
    }

    prv_run_game();
}
