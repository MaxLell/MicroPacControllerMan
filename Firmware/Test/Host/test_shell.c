/*
 * test_shell.c
 *
 * The order the screens come in, and the timing the requirements put on them.
 *
 * The display port and the flash are mocked and the tick is driven, so three seconds of
 * loading screen cost nothing and "the panel stayed dark" is a statement about transfers
 * rather than about a photograph. What is *drawn* is `test_render`'s and `test_sprite`'s
 * business; what is checked here is when, and in which order.
 */

#include "unity.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* Ceedling picks the sources to link from the includes it sees here, so the whole path
 * behind the shell has to be named even where this file calls none of it directly. */
#include "active_object.h"
#include "agent.h"
#include "ai_weights.h"
#include "assert_probe.h"
#include "circular_buffer.h"
#include "crc.h"
#include "custom_assert.h"
#include "difficulty.h"
#include "framebuffer.h"
#include "game.h"
#include "game_session.h"
#include "game_view.h"
#include "gfx.h"
#include "ghost.h"
#include "ghost_path.h"
#include "high_score.h"
#include "maze_gen.h"
#include "mock_display.h"
#include "mock_flash_bsp.h"
#include "mock_systick_bsp.h"
#include "msg.h"
#include "msg_broker.h"
#include "msg_queue.h"
#include "neural_net.h"
#include "pacman.h"
#include "pacman_ai.h"
#include "playfield.h"
#include "render.h"
#include "score.h"
#include "shell.h"
#include "sprite.h"
#include "sprite_set.h"
#include "sw_timer.h"

/* Compiled in only under TEST — see sw_timer.c and game_session.c. */
void sw_timer_test_reset(void);
void game_session_test_reset(void);

/* ==========================================================================
 * fixtures
 * ========================================================================= */

#define TEST_START_TICK  (1000U)
#define TEST_ERASED_BYTE (0xFFU)

static uint32_t g_now_ms;
static uint32_t g_region_count;
static uint8_t g_page[FLASH_BSP_BLOCK_SIZE];

static void prv_count_region(const framebuffer_t* in_framebuffer, int16_t in_x, int16_t in_y, int16_t in_width,
                             int16_t in_height, int in_call_count)
{
    (void)in_framebuffer;
    (void)in_x;
    (void)in_y;
    (void)in_width;
    (void)in_height;
    (void)in_call_count;

    ++g_region_count;
}

static void prv_on_flash_read(uint8_t* out_block, size_t in_block_size, int in_call_count)
{
    (void)in_call_count;

    (void)memcpy(out_block, g_page, in_block_size);
}

static bool prv_on_flash_replace(const uint8_t* in_block, size_t in_block_size, int in_call_count)
{
    (void)in_call_count;

    (void)memcpy(g_page, in_block, in_block_size);

    return true;
}

static bool prv_on_flash_erase(int in_call_count)
{
    (void)in_call_count;

    (void)memset(g_page, TEST_ERASED_BYTE, sizeof(g_page));

    return true;
}

/* A stub rather than IgnoreAndReturn: a run of several thousand frames asks the tick that
 * many times, and CMock allocates a record per expectation until it runs out. */
static uint32_t prv_get_tick(int in_call_count)
{
    (void)in_call_count;

    return g_now_ms;
}

/* Move the clock, let the timers see it, and give the shell its turn. */
static bool prv_advance(uint32_t in_elapsed_ms)
{
    g_now_ms += in_elapsed_ms;

    sw_timer_process();

    return shell_service();
}

/* Sit through the loading screen and arrive at the menu. */
/* Play a run out with the stick pushed, so that Pacman actually eats and the run has a score.
 *
 * A direction has to be asked for every frame: the stick is read as a level, not an edge (FR-004),
 * and the turn only happens at the first cell where it becomes possible. */
static void prv_play_until_the_run_ends(void)
{
    for (uint32_t frame = 0U; (frame < 20000U) && (shell_get_screen() == SHELL_SCREEN_GAME); ++frame)
    {
        shell_set_direction(DIRECTION_WEST);
        (void)prv_advance(GAME_SESSION_FRAME_PERIOD_MS);
    }
}

static void prv_reach_the_menu(void)
{
    (void)prv_advance(SHELL_LOGO_DELAY_MS);
    (void)prv_advance(SHELL_LOADING_MS);
    (void)prv_advance(0U);

    TEST_ASSERT_EQUAL_UINT(SHELL_SCREEN_MENU, shell_get_screen());
}

void setUp(void)
{
    g_now_ms = TEST_START_TICK;
    g_region_count = 0U;
    (void)memset(g_page, TEST_ERASED_BYTE, sizeof(g_page));

    display_init_Ignore();
    display_present_Ignore();
    display_service_Ignore();
    display_present_region_Stub(prv_count_region);

    flash_bsp_init_Ignore();
    flash_bsp_read_Stub(prv_on_flash_read);
    flash_bsp_replace_Stub(prv_on_flash_replace);
    flash_bsp_erase_Stub(prv_on_flash_erase);

    systick_bsp_get_tick_Stub(prv_get_tick);

    assert_probe_begin();

    sw_timer_test_reset();
    game_session_test_reset();
    sw_timer_init();

    high_score_init();
    shell_init();
}

void tearDown(void)
{
    /* No assertion may fire in a passing test. Without this the probe *masks* them: it
     * records and returns unless armed, so a screen drawn with a character the font does
     * not have would pass here and spin in `while (1)` on the board. It did, once — the
     * title was written "PAC-MAN" and the font has no hyphen. */
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0U, assert_probe_get_count(), "an assertion fired");

    assert_probe_end();
}

/* ==========================================================================
 * tests
 * ========================================================================= */

void test_the_board_comes_up_on_the_loading_screen(void)
{
    TEST_ASSERT_EQUAL_UINT(SHELL_SCREEN_LOADING, shell_get_screen());
}

/* NFR-005: 200 ms of darkness before the title. "Dark" is checked as *nothing sent to the
 * panel*, which is the only thing a unit test can honestly say about it. */
void test_the_panel_stays_dark_before_the_logo_appears(void)
{
    g_region_count = 0U;

    TEST_ASSERT_FALSE(prv_advance(SHELL_LOGO_DELAY_MS - 1U));
    TEST_ASSERT_EQUAL_UINT32(0U, g_region_count);

    TEST_ASSERT_TRUE(prv_advance(1U));
    TEST_ASSERT_GREATER_THAN_UINT32(0U, g_region_count);
}

/* NFR-001: the menu is up within three seconds of power-on, the dark part included. */
void test_the_menu_arrives_within_the_loading_budget(void)
{
    (void)prv_advance(SHELL_LOGO_DELAY_MS);

    TEST_ASSERT_EQUAL_UINT(SHELL_SCREEN_LOADING, shell_get_screen());

    (void)prv_advance(SHELL_LOADING_MS - SHELL_LOGO_DELAY_MS - 1U);

    TEST_ASSERT_EQUAL_UINT(SHELL_SCREEN_LOADING, shell_get_screen());

    (void)prv_advance(1U);

    TEST_ASSERT_EQUAL_UINT(SHELL_SCREEN_MENU, shell_get_screen());
}

/* Nothing may skip the loading screen: NFR-001 keeps it short enough that a player who
 * pressed early would only be confused by a title that vanished. */
void test_the_loading_screen_cannot_be_skipped(void)
{
    shell_press_start();

    TEST_ASSERT_EQUAL_UINT(SHELL_SCREEN_LOADING, shell_get_screen());
}

void test_the_start_key_starts_a_run_from_the_menu(void)
{
    prv_reach_the_menu();

    shell_press_start();

    TEST_ASSERT_EQUAL_UINT(SHELL_SCREEN_GAME, shell_get_screen());
    TEST_ASSERT_EQUAL_UINT(GAME_STATE_RUNNING, game_session_get_state());
}

/* A direction pressed on the menu must not be waiting for the player when the run starts. */
void test_a_direction_outside_a_run_goes_nowhere(void)
{
    prv_reach_the_menu();

    shell_set_direction(DIRECTION_WEST);

    TEST_ASSERT_EQUAL_UINT(SHELL_SCREEN_MENU, shell_get_screen());
}

void test_a_run_that_ends_shows_the_score_and_then_the_menu(void)
{
    prv_reach_the_menu();
    shell_press_start();

    /* Nobody is steering, so the ghosts take all three lives. Bounded well above what that
     * needs, and the assertion below is what says it happened rather than the loop ending. */
    for (uint32_t frame = 0U; (frame < 4000U) && (shell_get_screen() == SHELL_SCREEN_GAME); ++frame)
    {
        (void)prv_advance(GAME_SESSION_FRAME_PERIOD_MS);
    }

    TEST_ASSERT_EQUAL_UINT(SHELL_SCREEN_SCORE, shell_get_screen());

    /* FR-023: two seconds, then the menu comes back on its own. */
    (void)prv_advance(SHELL_SCORE_MS - 1U);

    TEST_ASSERT_EQUAL_UINT(SHELL_SCREEN_SCORE, shell_get_screen());

    (void)prv_advance(1U);
    (void)prv_advance(0U);

    TEST_ASSERT_EQUAL_UINT(SHELL_SCREEN_MENU, shell_get_screen());
}

/* A player who is ready should not have to wait out the score screen. */
void test_the_start_key_cuts_the_score_screen_short(void)
{
    prv_reach_the_menu();
    shell_press_start();

    for (uint32_t frame = 0U; (frame < 4000U) && (shell_get_screen() == SHELL_SCREEN_GAME); ++frame)
    {
        (void)prv_advance(GAME_SESSION_FRAME_PERIOD_MS);
    }

    TEST_ASSERT_EQUAL_UINT(SHELL_SCREEN_SCORE, shell_get_screen());

    shell_press_start();

    TEST_ASSERT_EQUAL_UINT(SHELL_SCREEN_MENU, shell_get_screen());
}

/* The run is offered to the table when it ends, and only then — the score screen has to be
 * able to say whether it got in, and storing it costs a flash erase (§10.11). */
void test_a_finished_run_reaches_the_high_score_table(void)
{
    prv_reach_the_menu();

    TEST_ASSERT_EQUAL_UINT32(0U, high_score_get_best());

    shell_press_start();
    prv_play_until_the_run_ends();

    /* Both halves asserted, because a table that refuses everything would pass the second one
     * alone. This test used to leave Pacman standing still and compare his score against the
     * table — and a standing Pacman scores nothing, so it was comparing zero with zero. Pacman
     * starts on an empty cell: he has to be pushed somewhere to eat at all. */
    TEST_ASSERT_NOT_EQUAL_UINT32(0U, game_session_get_score());
    TEST_ASSERT_EQUAL_UINT32(game_session_get_score(), high_score_get_best());
}

/* --- the AI takeover (FR-030/033/034) ------------------------------------- */

/* The board button means start on the menu, so a toggle there must refuse and say so — otherwise
 * `app_main` would swallow the press and the menu would stop responding to the button. */
void test_the_ai_cannot_be_toggled_outside_a_run(void)
{
    prv_reach_the_menu();

    TEST_ASSERT_FALSE(shell_toggle_ai());
    TEST_ASSERT_FALSE(shell_is_ai_playing());
    TEST_ASSERT_EQUAL_UINT(SHELL_SCREEN_MENU, shell_get_screen());
}

void test_the_ai_takes_over_and_hands_back_during_a_run(void)
{
    prv_reach_the_menu();
    shell_press_start();

    TEST_ASSERT_FALSE(shell_is_ai_playing());

    TEST_ASSERT_TRUE(shell_toggle_ai());
    TEST_ASSERT_TRUE(shell_is_ai_playing());

    TEST_ASSERT_TRUE(shell_toggle_ai());
    TEST_ASSERT_FALSE(shell_is_ai_playing());
}

/* FR-034 is "was on at some point", not "is on now". Handing control back before the last life must
 * not launder the score, which is exactly what a live flag would allow. */
void test_the_lockout_survives_handing_control_back(void)
{
    prv_reach_the_menu();
    shell_press_start();

    TEST_ASSERT_FALSE(shell_has_ai_played());

    (void)shell_toggle_ai();
    (void)shell_toggle_ai();

    TEST_ASSERT_FALSE(shell_is_ai_playing());
    TEST_ASSERT_TRUE(shell_has_ai_played());
}

/* The two halves in one test on purpose: a lockout that simply broke high scores altogether would
 * pass the first assertion and fail the second. */
void test_an_ai_run_is_kept_out_of_the_table_and_a_player_run_is_not(void)
{
    prv_reach_the_menu();
    shell_press_start();

    TEST_ASSERT_TRUE(shell_toggle_ai());

    prv_play_until_the_run_ends();

    TEST_ASSERT_EQUAL_UINT(SHELL_SCREEN_SCORE, shell_get_screen());

    /* The AI's run has to have *scored* for the lockout to mean anything: a refused zero would
     * look exactly like a working lockout. */
    TEST_ASSERT_NOT_EQUAL_UINT32(0U, game_session_get_score());
    TEST_ASSERT_EQUAL_UINT32(0U, high_score_get_best());

    /* And now the same flow without the AI, which must reach the table. */
    shell_press_start();
    shell_press_start();
    prv_play_until_the_run_ends();

    TEST_ASSERT_EQUAL_UINT(SHELL_SCREEN_SCORE, shell_get_screen());
    TEST_ASSERT_NOT_EQUAL_UINT32(0U, high_score_get_best());
}

/* FR-033's second half: every new run begins under player control, whatever the last one did. */
void test_a_new_run_starts_under_player_control(void)
{
    prv_reach_the_menu();
    shell_press_start();
    (void)shell_toggle_ai();

    TEST_ASSERT_TRUE(shell_is_ai_playing());

    prv_play_until_the_run_ends();

    shell_press_start();
    shell_press_start();

    TEST_ASSERT_EQUAL_UINT(SHELL_SCREEN_GAME, shell_get_screen());
    TEST_ASSERT_FALSE(shell_is_ai_playing());
    TEST_ASSERT_FALSE(shell_has_ai_played());
}
