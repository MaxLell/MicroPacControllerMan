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

/* Whether the run in progress is being played on the arcade's own maze.
 *
 * Asked of the **pellets**, because that is what the shell's own surface carries: a level that has
 * just begun has a pellet on exactly the cells its map marks with one, so the two bitmaps agreeing
 * for all 868 cells is the maze being the same maze. Comparing the walls would need a getter that
 * exists for no other reason. */
static bool prv_is_playing_the_normal_maze(void)
{
    playfield_map_t arcade;
    msg_game_state_t state;

    playfield_get_arcade_map(&arcade);
    game_session_get_state_message(&state);

    for (uint8_t row = 0U; row < PLAYFIELD_HEIGHT; ++row)
    {
        for (uint8_t column = 0U; column < PLAYFIELD_WIDTH; ++column)
        {
            const char tile = arcade.rows[row][column];
            const bool is_pellet_on_the_map = (tile == PLAYFIELD_MAP_PELLET) || (tile == PLAYFIELD_MAP_POWER_PELLET);

            if (msg_cell_bitmap_get(state.has_pellet, column, row) != is_pellet_on_the_map)
            {
                return false;
            }
        }
    }

    return true;
}

/* Push the stick until the menu is on a given game.
 *
 * By *pushing* rather than by setting: there is no way to set it, and there should not be — a test
 * that reached past the input would stop covering the path a player takes. */
static void prv_select(shell_mode_e in_mode)
{
    while (shell_get_selected_mode() < in_mode)
    {
        shell_move_selection(DIRECTION_SOUTH);
    }

    while (shell_get_selected_mode() > in_mode)
    {
        shell_move_selection(DIRECTION_NORTH);
    }

    TEST_ASSERT_EQUAL_UINT(in_mode, shell_get_selected_mode());
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
     * title was written "PAC-MAN" before the font had a hyphen. */
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

    TEST_ASSERT_EQUAL_UINT32(0U, high_score_get_best((uint8_t)SHELL_MODE_NORMAL_MAZE));

    shell_press_start();
    prv_play_until_the_run_ends();

    /* Both halves asserted, because a table that refuses everything would pass the second one
     * alone. This test used to leave Pacman standing still and compare his score against the
     * table — and a standing Pacman scores nothing, so it was comparing zero with zero. Pacman
     * starts on an empty cell: he has to be pushed somewhere to eat at all. */
    TEST_ASSERT_NOT_EQUAL_UINT32(0U, game_session_get_score());
    TEST_ASSERT_EQUAL_UINT32(game_session_get_score(), high_score_get_best((uint8_t)SHELL_MODE_NORMAL_MAZE));
}

/* --- the choice of maze (FR-040) ------------------------------------------ */

/* The normal maze first: it is the arcade's layout and the only one that offers the AI, so it is
 * what a player who presses start without reading anything should get. */
void test_the_menu_starts_on_the_normal_maze(void)
{
    prv_reach_the_menu();

    TEST_ASSERT_EQUAL_UINT(SHELL_MODE_NORMAL_MAZE, shell_get_selected_mode());
}

void test_the_stick_moves_the_selection_and_stops_at_the_ends(void)
{
    prv_reach_the_menu();

    shell_move_selection(DIRECTION_NORTH);

    TEST_ASSERT_EQUAL_UINT(SHELL_MODE_NORMAL_MAZE, shell_get_selected_mode());

    shell_move_selection(DIRECTION_SOUTH);

    TEST_ASSERT_EQUAL_UINT(SHELL_MODE_AI, shell_get_selected_mode());

    shell_move_selection(DIRECTION_SOUTH);

    TEST_ASSERT_EQUAL_UINT(SHELL_MODE_RANDOM_MAZE, shell_get_selected_mode());

    /* The end of the list, and it stays there rather than wrapping round to the top. */
    shell_move_selection(DIRECTION_SOUTH);

    TEST_ASSERT_EQUAL_UINT(SHELL_MODE_RANDOM_MAZE, shell_get_selected_mode());

    shell_move_selection(DIRECTION_NORTH);
    shell_move_selection(DIRECTION_NORTH);

    TEST_ASSERT_EQUAL_UINT(SHELL_MODE_NORMAL_MAZE, shell_get_selected_mode());
}

/* Sideways is the one direction the menu has no meaning for, and a menu that jumped on it would
 * make a player who nudged the stick mid-reach play the wrong game. */
void test_a_sideways_push_leaves_the_selection_alone(void)
{
    prv_reach_the_menu();

    shell_move_selection(DIRECTION_WEST);
    shell_move_selection(DIRECTION_EAST);

    TEST_ASSERT_EQUAL_UINT(SHELL_MODE_NORMAL_MAZE, shell_get_selected_mode());
}

/* Steering during a run must not change what the run is. The selection is also what
 * #shell_toggle_ai consults, so a stick pushed north mid-game moving it would take the AI away
 * from a normal-maze run halfway through. */
void test_the_selection_cannot_be_moved_during_a_run(void)
{
    prv_reach_the_menu();
    shell_press_start();

    shell_move_selection(DIRECTION_SOUTH);

    TEST_ASSERT_EQUAL_UINT(SHELL_MODE_NORMAL_MAZE, shell_get_selected_mode());
}

/* Moving the selection redraws the cursor and the three scores of the game now selected — not the
 * screen. Asserted as a *fraction* of what the whole menu cost rather than an exact count, because
 * the exact count is arithmetic about the score field's width and would have to be edited every
 * time the layout moved; what the test is defending is the order of magnitude. A full redraw would
 * be correct and would also blank and rebuild 240 x 320 pixels, which at whole-frame rates is a
 * third of a second to answer one push of the stick. */
void test_moving_the_selection_redraws_the_scores_and_the_cursor_and_not_the_screen(void)
{
    prv_reach_the_menu();

    const uint32_t menu_regions = g_region_count;
    const uint32_t regions_before = g_region_count;

    shell_move_selection(DIRECTION_SOUTH);

    TEST_ASSERT_TRUE(prv_advance(0U));

    const uint32_t move_regions = g_region_count - regions_before;

    TEST_ASSERT_GREATER_THAN_UINT32(0U, move_regions);
    TEST_ASSERT_LESS_THAN_UINT32(menu_regions / 2U, move_regions);
}

void test_the_normal_maze_option_plays_the_arcade_maze(void)
{
    prv_reach_the_menu();
    shell_press_start();

    TEST_ASSERT_EQUAL_UINT(GAME_STATE_RUNNING, game_session_get_state());
    TEST_ASSERT_TRUE(prv_is_playing_the_normal_maze());
}

void test_the_random_maze_option_plays_a_generated_maze(void)
{
    prv_reach_the_menu();
    prv_select(SHELL_MODE_RANDOM_MAZE);
    shell_press_start();

    TEST_ASSERT_EQUAL_UINT(GAME_STATE_RUNNING, game_session_get_state());

    /* Not "some maze" but "not that one": the generator cannot produce the arcade's layout — its
     * tunnels are one or two cells where the arcade's are six — so the two disagreeing is the
     * seeded path having been taken. */
    TEST_ASSERT_FALSE(prv_is_playing_the_normal_maze());
}

/* --- the Pac-Man AI game (FR-042) ----------------------------------------- */

void test_the_ai_game_plays_itself_from_the_first_frame(void)
{
    prv_reach_the_menu();
    prv_select(SHELL_MODE_AI);
    shell_press_start();

    /* No press of anything else: the agent has Pac-Man because the game is that game. In the normal
     * maze the same assertion is false at this point, which is the difference between the two. */
    TEST_ASSERT_EQUAL_UINT(SHELL_SCREEN_GAME, shell_get_screen());
    TEST_ASSERT_TRUE(shell_is_ai_playing());
    TEST_ASSERT_TRUE(shell_has_ai_played());
    TEST_ASSERT_TRUE(prv_is_playing_the_normal_maze());
}

/* A game that let the player take over would be the normal maze under another name — and the button
 * has to agree with that, because in this game it means something else entirely (the loop). */
void test_the_ai_game_cannot_be_taken_over(void)
{
    prv_reach_the_menu();
    prv_select(SHELL_MODE_AI);
    shell_press_start();

    TEST_ASSERT_FALSE(shell_toggle_ai());
    TEST_ASSERT_TRUE(shell_is_ai_playing());
}

/* FR-041 and FR-034 together, and they pull in opposite directions: an agent's run is refused by a
 * player's table and belongs in the agent's own. Both halves in one test, because a lockout that
 * refused every table would pass the first assertion. */
void test_the_ai_games_score_goes_into_its_own_table_and_no_other(void)
{
    prv_reach_the_menu();
    prv_select(SHELL_MODE_AI);
    shell_press_start();
    prv_play_until_the_run_ends();

    TEST_ASSERT_EQUAL_UINT(SHELL_SCREEN_SCORE, shell_get_screen());
    TEST_ASSERT_NOT_EQUAL_UINT32(0U, game_session_get_score());

    TEST_ASSERT_EQUAL_UINT32(game_session_get_score(), high_score_get_best((uint8_t)SHELL_MODE_AI));
    TEST_ASSERT_EQUAL_UINT32(0U, high_score_get_best((uint8_t)SHELL_MODE_NORMAL_MAZE));
    TEST_ASSERT_EQUAL_UINT32(0U, high_score_get_best((uint8_t)SHELL_MODE_RANDOM_MAZE));
}

/* --- the endless mode (FR-043) -------------------------------------------- */

void test_a_game_started_by_hand_is_one_game(void)
{
    prv_reach_the_menu();
    prv_select(SHELL_MODE_AI);
    shell_press_start();

    TEST_ASSERT_FALSE(shell_is_infinite());
    TEST_ASSERT_EQUAL_UINT32(1U, shell_get_run_count());
}

/* The loop belongs to the agent's game: a restart in a game a person is playing would replay a run
 * they had not asked to replay. */
void test_the_loop_cannot_be_switched_on_in_a_game_a_person_plays(void)
{
    prv_reach_the_menu();
    shell_press_start();

    TEST_ASSERT_FALSE(shell_toggle_infinite());
    TEST_ASSERT_FALSE(shell_is_infinite());
}

void test_the_loop_cannot_be_switched_on_from_the_menu(void)
{
    prv_reach_the_menu();
    prv_select(SHELL_MODE_AI);

    /* On the menu the button means start (FR-003), so there is nothing here for the loop to be. */
    TEST_ASSERT_FALSE(shell_toggle_infinite());
    TEST_ASSERT_FALSE(shell_is_infinite());
}

void test_the_loop_starts_the_next_run_instead_of_returning_to_the_menu(void)
{
    prv_reach_the_menu();
    prv_select(SHELL_MODE_AI);
    shell_press_start();

    TEST_ASSERT_TRUE(shell_toggle_infinite());
    TEST_ASSERT_TRUE(shell_is_infinite());

    prv_play_until_the_run_ends();

    /* The score screen still has its two seconds: the run's result is worth seeing, and the loop is
     * about not needing a person, not about not showing them anything. */
    TEST_ASSERT_EQUAL_UINT(SHELL_SCREEN_SCORE, shell_get_screen());

    (void)prv_advance(SHELL_SCORE_MS);
    (void)prv_advance(0U);

    TEST_ASSERT_EQUAL_UINT(SHELL_SCREEN_GAME, shell_get_screen());
    TEST_ASSERT_EQUAL_UINT32(2U, shell_get_run_count());

    /* And the next run is the same game, played by the same agent: a loop that handed control back
     * would stop being the game it is looping. */
    TEST_ASSERT_TRUE(shell_is_ai_playing());
    TEST_ASSERT_TRUE(shell_is_infinite());
}

void test_switching_the_loop_off_lets_the_run_finish(void)
{
    prv_reach_the_menu();
    prv_select(SHELL_MODE_AI);
    shell_press_start();

    TEST_ASSERT_TRUE(shell_toggle_infinite());
    TEST_ASSERT_TRUE(shell_toggle_infinite());
    TEST_ASSERT_FALSE(shell_is_infinite());

    prv_play_until_the_run_ends();

    (void)prv_advance(SHELL_SCORE_MS);
    (void)prv_advance(0U);

    TEST_ASSERT_EQUAL_UINT(SHELL_SCREEN_MENU, shell_get_screen());
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

/* FR-040: the agent was evolved against the normal maze, so a random-maze run does not offer it.
 * Refused rather than allowed-but-poor, because the menu says "AI AVAILABLE" against one option
 * only and the button has to agree with the menu. */
void test_the_ai_cannot_be_toggled_in_a_random_maze_run(void)
{
    prv_reach_the_menu();
    prv_select(SHELL_MODE_RANDOM_MAZE);
    shell_press_start();

    TEST_ASSERT_EQUAL_UINT(SHELL_SCREEN_GAME, shell_get_screen());
    TEST_ASSERT_FALSE(shell_toggle_ai());
    TEST_ASSERT_FALSE(shell_is_ai_playing());
    TEST_ASSERT_FALSE(shell_has_ai_played());
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
    TEST_ASSERT_EQUAL_UINT32(0U, high_score_get_best((uint8_t)SHELL_MODE_NORMAL_MAZE));

    /* And now the same flow without the AI, which must reach the table. */
    shell_press_start();
    shell_press_start();
    prv_play_until_the_run_ends();

    TEST_ASSERT_EQUAL_UINT(SHELL_SCREEN_SCORE, shell_get_screen());
    TEST_ASSERT_NOT_EQUAL_UINT32(0U, high_score_get_best((uint8_t)SHELL_MODE_NORMAL_MAZE));
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
