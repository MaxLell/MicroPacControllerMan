#include "app_main.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "Cli.h"
#include "console.h"
#include "difficulty.h"
#include "dio_bsp.h"
#include "flash_bsp.h"
#include "game.h"
#include "game_session.h"
#include "high_score.h"
#include "joystick.h"
#include "msg.h"
#include "ott.h"
#include "rng_bsp.h"
#include "shell.h"
#include "spi_bsp.h"
#include "sw_timer.h"
#include "systick_bsp.h"
#include "user_button.h"

/* ==========================================================================
 * app_main - private
 * ========================================================================= */

#define APP_MAIN_BOOT_BANNER     "MicroPacControllerMan booted. Type 'ott' for tests, 'reset' to restart the game."

/*! \brief The help texts the console's own commands are registered with.
 *
 * Named rather than written inline **so that they can be measured**. `cli_binding_t::help` is a
 * `char[CLI_MAX_HELPER_STRING_LENGTH]` and the initialiser copies into it, so a text that does not
 * fit is truncated to an array with **no terminator left in it** — after which `help` prints this
 * command's text and then keeps reading into the next binding's name. The compiler says so and
 * nothing else does, which is how `select` shipped five characters over.
 *
 * The assertions below are the fix rather than the shortening: a build has to fail on the next one.
 */
#define APP_MAIN_HELP_HIGH_SCORE "Show the three best scores; 'highscore reset' clears them"
#define APP_MAIN_HELP_START      "Press start: begins a run from the menu"
#define APP_MAIN_HELP_SELECT     "The menu; 'select up' and 'select down' move the cursor"
#define APP_MAIN_HELP_BUTTON     "Press the board button: start, hand over to the AI, or loop"

_Static_assert(sizeof(APP_MAIN_HELP_HIGH_SCORE) <= CLI_MAX_HELPER_STRING_LENGTH, "'highscore' help is truncated");
_Static_assert(sizeof(APP_MAIN_HELP_START) <= CLI_MAX_HELPER_STRING_LENGTH, "'start' help is truncated");
_Static_assert(sizeof(APP_MAIN_HELP_SELECT) <= CLI_MAX_HELPER_STRING_LENGTH, "'select' help is truncated");
_Static_assert(sizeof(APP_MAIN_HELP_BUTTON) <= CLI_MAX_HELPER_STRING_LENGTH, "'button' help is truncated");

static void prv_on_systick(void)
{
    (void)user_button_poll();
    joystick_poll();

    /* The console rides the tick for the same reason the buttons do: the receive register
     * holds one character and has no FIFO, and once a frame is being drawn the main loop
     * is away for milliseconds at a time. */
    console_poll_receive();
}

/* `highscore` with no argument shows the table; `highscore reset` empties it (FR-009).
 *
 * A console command rather than a menu entry, because the reason to clear the table is
 * almost always a developer's: a score set while testing sits in the top three for good,
 * and there is no way to play a worse game than one that has already happened. */
static int prv_high_score_command(int in_argument_count, char* in_arguments[], void* in_context)
{
    (void)in_context;

    if ((in_argument_count > 1) && (strcmp(in_arguments[1], "reset") == 0))
    {
        if (!high_score_reset())
        {
            cli_print("could not erase the stored scores");

            return CLI_FAIL_STATUS;
        }

        cli_print("high scores cleared");

        return CLI_OK_STATUS;
    }

    /* Both tables, each under the name of the maze it belongs to (FR-041). Labelled rather than
     * numbered, because "table 1" is a number only this file knows the meaning of. The machine has no
     * table since DEC-056, so there are two and they are the two mazes. */
    static const char* const k_table_names[] = {"classic", "random"};

    for (uint8_t table = 0U; table < HIGH_SCORE_TABLE_COUNT; ++table)
    {
        cli_print("%s:", k_table_names[table]);

        for (uint8_t place = 0U; place < HIGH_SCORE_COUNT; ++place)
        {
            cli_print("  %u. %lu", (unsigned)(place + 1U), (unsigned long)high_score_get(table, place));
        }
    }

    cli_print("'highscore reset' clears both.");

    return CLI_OK_STATUS;
}

/* `start` presses the start key from the console.
 *
 * FR-003's key is at the board, and that makes the one path a player actually walks —
 * menu, run, score, menu again — the one path no harness can drive. This is the seam:
 * `run_ott.py` can now walk it, and so can anyone reading the serial line who has not got
 * a finger free. It presses; it does not decide, so the flow rules stay in one place. */
static int prv_start_command(int in_argument_count, char* in_arguments[], void* in_context)
{
    (void)in_argument_count;
    (void)in_arguments;
    (void)in_context;

    shell_press_start();

    /* **What it did has to be said, because it no longer always starts a run.** Since DEC-056 the
     * centre key takes the highlighted option, and on every page but the last of a path that means
     * moving on. A harness driving the menu needs to know which — and when a run does begin, the
     * periodic report names it, so there is nothing to add here. */
    if (shell_get_screen() == SHELL_SCREEN_MENU)
    {
        static const char* const k_page_names[] = {"player", "maze", "endless"};

        cli_print("page: %s", k_page_names[shell_get_menu_page()]);
    }

    return CLI_OK_STATUS;
}

/* `select` shows what the menu is on and `select up`/`down` move the cursor within the page that is up
 * (DEC-056). Sideways is accepted and does nothing: a page that is a list has nothing sideways to be.
 *
 * `start` takes the highlighted option — which advances a page or begins the run — and `button` steps
 * back, so the whole three-page walk is drivable from the console.
 *
 * A *device* on the console rather than a decision, exactly like `start`: it pushes, and the shell
 * decides what pushing means. That is what lets the whole flow — pick a game, play it, see the
 * score — be walked from `run_ott.py` without anybody at the board (FR-040, VT-INT-011). */
static int prv_select_command(int in_argument_count, char* in_arguments[], void* in_context)
{
    (void)in_context;

    if (in_argument_count > 1)
    {
        if (strcmp(in_arguments[1], "up") == 0)
        {
            shell_move_selection(DIRECTION_NORTH);
        }
        else if (strcmp(in_arguments[1], "down") == 0)
        {
            shell_move_selection(DIRECTION_SOUTH);
        }
        else if (strcmp(in_arguments[1], "left") == 0)
        {
            shell_move_selection(DIRECTION_WEST);
        }
        else if (strcmp(in_arguments[1], "right") == 0)
        {
            shell_move_selection(DIRECTION_EAST);
        }
        else
        {
            cli_print("'select up', 'down', 'left' or 'right'");

            return CLI_FAIL_STATUS;
        }
    }

    /* The page, what is highlighted on it, and the choices made so far — so a harness can read back
     * exactly what the pushes did rather than trusting that they did anything (VT-INT-026/027). */
    static const char* const k_page_names[] = {"player", "maze", "endless"};

    cli_print("page: %s", k_page_names[shell_get_menu_page()]);
    cli_print("selected: %s", shell_get_mode_name());
    cli_print("endless: %s", shell_is_infinite() ? "on" : "off");

    return CLI_OK_STATUS;
}

/* `button` presses the board button from the console.
 *
 * The counterpart to `select`, and a device rather than a decision for the same reason: it presses,
 * and the shell decides what pressing means on the screen that is up (FR-003/043). It is what lets
 * the endless mode be switched — and therefore checked — without a finger on B1. */
static int prv_button_command(int in_argument_count, char* in_arguments[], void* in_context)
{
    (void)in_argument_count;
    (void)in_arguments;
    (void)in_context;

    shell_press_user_button();

    /* Same reason as `start`: on the menu the button is a step back, and which page it landed on is
     * the whole of what a harness can check. */
    if (shell_get_screen() == SHELL_SCREEN_MENU)
    {
        static const char* const k_page_names[] = {"player", "maze", "endless"};

        cli_print("page: %s", k_page_names[shell_get_menu_page()]);
    }

    return CLI_OK_STATUS;
}

static void prv_init_platform(void)
{
    systick_bsp_init();
    dio_bsp_init();

    /* Before anything that plays: the game draws its timing jitter when a level loads (FR-044), and
     * the maze seed of a run comes from here too. A generator that failed to come up hands out zero,
     * which the game reads as "no jitter" — a playable game rather than a refusal. Whether it came
     * up is reported after the console exists, which is not yet. */
    (void)rng_bsp_init();

    console_init();
    flash_bsp_init();
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
            shell_set_direction(k_stick[index].direction);
        }
    }

    /* The same two keys again, and deliberately read the other way: the menu's selection moves once
     * per *press* (FR-040), where the game wants the level of a held stick. Taking the latch costs
     * the game nothing, because the game never looks at it. */
    if (joystick_take_press(JOYSTICK_KEY_NORTH))
    {
        shell_move_selection(DIRECTION_NORTH);
    }

    if (joystick_take_press(JOYSTICK_KEY_SOUTH))
    {
        shell_move_selection(DIRECTION_SOUTH);
    }

    /* Sideways picks which agent the AI game uses. Taken as an edge like the other two, so holding
     * the stick over does not flick between the two agents once a frame. */
    if (joystick_take_press(JOYSTICK_KEY_WEST))
    {
        shell_move_selection(DIRECTION_WEST);
    }

    if (joystick_take_press(JOYSTICK_KEY_EAST))
    {
        shell_move_selection(DIRECTION_EAST);
    }

    /* Start comes from either key, and both are taken as an *edge* so a thumb resting on
     * one does not keep pressing it. FR-003 names the Nucleo's own button; the centre of
     * the stick is where a player's hand already is, and having both costs one line. */
    if (joystick_take_press(JOYSTICK_KEY_CENTER))
    {
        shell_press_start();
    }

    /* The board button means one of four things and the shell works out which (FR-003/030/043): it
     * knows the screen and the game, and this file knows neither. It used to be a fall-through here,
     * which was one condition too many the moment a third meaning arrived.
     *
     * The stick's centre keeps meaning start only — a player reaching for it mid-run is asking to
     * play, not to stop playing. */
    if (user_button_take_press())
    {
        shell_press_user_button();
    }
}

/* Say what the board is doing, on the console, when it changes.
 *
 * FR-107's principle applied to the game itself: the board reports over the serial line so
 * nothing needs a debugger to be understood. It also makes the game observable to someone
 * who cannot see the panel — which is how the missing timer re-arm was found, because the
 * loop was silent where it should have been reporting.
 *
 * Only on a change, and the things that change are rare: a screen, a level, a life. A line
 * per pellet would flood the console and upset the silence `run_ott.py` waits for. */
static void prv_report_progress(void)
{
    static const char* const k_screen_names[] = {"loading", "menu", "game", "score"};
    static shell_screen_e g_reported_screen = SHELL_SCREEN_GAME;
    static uint8_t g_reported_level;
    static uint8_t g_reported_lives;
    static bool g_reported_infinite;

    const shell_screen_e screen = shell_get_screen();
    const uint8_t level = game_session_get_level();
    const uint8_t lives = game_session_get_lives();
    const bool is_infinite = shell_is_infinite();

    if (is_infinite != g_reported_infinite)
    {
        g_reported_infinite = is_infinite;

        cli_print("endless mode %s", is_infinite ? "on - a finished run starts the next one" : "off");
    }

    if ((screen == g_reported_screen) && (level == g_reported_level) && (lives == g_reported_lives))
    {
        return;
    }

    if (screen != g_reported_screen)
    {
        cli_print("%s screen", k_screen_names[screen]);
    }

    if (screen == SHELL_SCREEN_GAME)
    {
        cli_print("  %s run %lu: level %u - %u lives, %lu points", shell_get_mode_name(),
                  (unsigned long)shell_get_run_count(), (unsigned)level, (unsigned)lives,
                  (unsigned long)game_session_get_score());
    }
    else if (screen == SHELL_SCREEN_SCORE)
    {
        cli_print("  %lu points on level %u%s", (unsigned long)game_session_get_score(), (unsigned)level,
                  (game_session_get_state() == GAME_STATE_WON) ? " - all levels cleared" : "");
    }
    else
    {
        /* Loading and the menu have nothing to add. */
    }

    g_reported_screen = screen;
    g_reported_level = level;
    g_reported_lives = lives;
}

/* Normal operation: the game, with the console alongside it. Never returns — the way out
 * is `ott <name>` or `reset`, and both reboot the board. */
static void prv_run_game(void)
{
    shell_init();

    for (;;)
    {
        /* The console shares the loop rather than the frame: a command arriving mid-frame
         * is answered at the end of it, which is well inside a keystroke. */
        ott_poll();
        sw_timer_process();
        prv_poll_input();

        (void)shell_service();

        prv_report_progress();
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

    {
        cli_binding_t high_score_binding = {"highscore", prv_high_score_command, NULL, APP_MAIN_HELP_HIGH_SCORE};

        cli_binding_t start_binding = {"start", prv_start_command, NULL, APP_MAIN_HELP_START};

        cli_binding_t select_binding = {"select", prv_select_command, NULL, APP_MAIN_HELP_SELECT};

        cli_binding_t button_binding = {"button", prv_button_command, NULL, APP_MAIN_HELP_BUTTON};

        cli_register(&high_score_binding);
        cli_register(&start_binding);
        cli_register(&select_binding);
        cli_register(&button_binding);
    }

    high_score_init();

    cli_print(APP_MAIN_BOOT_BANNER);

    if (!rng_bsp_is_available())
    {
        /* Said rather than swallowed: the game is still playable, but every run of a level would be
         * paced identically and the ghosts would leave the house on the same dot every time — which
         * looks like a design and is a fault. */
        cli_print("the hardware RNG did not come up: timings will not vary (FR-044/FR-045)");
    }

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
