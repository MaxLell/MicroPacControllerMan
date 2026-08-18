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
#include "mock_rng_bsp.h"
#include "mock_systick_bsp.h"
#include "msg.h"
#include "msg_broker.h"
#include "msg_queue.h"
#include "pacman.h"
#include "pacman_lookahead.h"
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

#define TEST_START_TICK          (1000U)
#define TEST_ERASED_BYTE         (0xFFU)

/* Everything the menu draws above this row is the masthead and nothing else. It is `shell.c`'s first
 * option row less the 4 px the cursor is raised by, so the cursor's own top does not count as ink.
 * Copied rather than exported, because a getter for it would exist for this one test. */
#define TEST_MENU_BAND_BOTTOM    (172)

/* And the rows the maze page writes its scores on. The first is the row the masthead's cast stands on
 * and the second is out of its reach, which is what makes the pair worth comparing. The score field is
 * a place digit, two spaces and seven digits, centred as a block. Copied from `shell.c` for the same
 * reason as the band above. */
#define TEST_MENU_FIRST_SCORE_Y  (96)
#define TEST_MENU_SCORE_PITCH    (16)
#define TEST_SCORE_FIELD_GLYPHS  (10)
#define TEST_SCORE_NUMBER_GLYPHS (7)

static uint32_t g_now_ms;
static uint32_t g_region_count;
static uint8_t g_page[FLASH_BSP_BLOCK_SIZE];

/* The panel as the shell left it. Kept from the presenting stub because that is the only place a test
 * is handed it, and pixels are the only honest way to ask what survived a redraw. */
static const framebuffer_t* g_panel;

static void prv_count_region(const framebuffer_t* in_framebuffer, int16_t in_x, int16_t in_y, int16_t in_width,
                             int16_t in_height, int in_call_count)
{
    g_panel = in_framebuffer;

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
/* Walk the menu's pages the way a player would, and stop on the last one without confirming it.
 *
 * The menu is a list and a confirm, three pages deep (DEC-056), so this drives the cursor and the
 * centre key rather than setting anything: what a test wants to know is that the *pushes* get there.
 * It stops short of the final confirm so a caller can assert about the menu before the run begins.
 */
static void prv_walk_to(shell_player_e in_player, shell_maze_e in_maze, bool in_is_endless)
{
    if (shell_get_screen() != SHELL_SCREEN_MENU)
    {
        prv_reach_the_menu();
    }

    while (shell_press_back())
    {
        /* Back to the first page, wherever a previous test left it. */
    }

    TEST_ASSERT_EQUAL_UINT(SHELL_MENU_PAGE_PLAYER, shell_get_menu_page());

    if (shell_get_selected_player() != in_player)
    {
        shell_move_selection((in_player == SHELL_PLAYER_MACHINE) ? DIRECTION_SOUTH : DIRECTION_NORTH);
    }

    TEST_ASSERT_EQUAL_UINT(in_player, shell_get_selected_player());

    shell_press_start();

    TEST_ASSERT_EQUAL_UINT(SHELL_MENU_PAGE_MAZE, shell_get_menu_page());

    if (shell_get_selected_maze() != in_maze)
    {
        shell_move_selection((in_maze == SHELL_MAZE_RANDOM) ? DIRECTION_SOUTH : DIRECTION_NORTH);
    }

    TEST_ASSERT_EQUAL_UINT(in_maze, shell_get_selected_maze());

    if (in_player != SHELL_PLAYER_MACHINE)
    {
        /* A person's path ends here: there is no endless page to reach. */
        TEST_ASSERT_FALSE_MESSAGE(in_is_endless, "a person's run cannot loop");

        return;
    }

    shell_press_start();

    TEST_ASSERT_EQUAL_UINT(SHELL_MENU_PAGE_ENDLESS, shell_get_menu_page());

    if (shell_is_infinite() != in_is_endless)
    {
        shell_move_selection(in_is_endless ? DIRECTION_SOUTH : DIRECTION_NORTH);
    }

    TEST_ASSERT_EQUAL(in_is_endless, shell_is_infinite());
}

/* The same walk, confirmed, so the run is under way. */
static void prv_start_a_run(shell_player_e in_player, shell_maze_e in_maze, bool in_is_endless)
{
    prv_walk_to(in_player, in_maze, in_is_endless);

    shell_press_start();

    TEST_ASSERT_EQUAL_UINT(SHELL_SCREEN_GAME, shell_get_screen());
}

/* Make the search play badly, on purpose, for the whole of this file.
 *
 * **These tests are about the shell's flow, not about how well the search plays** — the lockout, the
 * loop, which table a run reaches. With the weights the firmware ships, an AI run reaches level six
 * and takes tens of millions of simulated ticks to finish; a test that played one to the end ran for
 * twelve minutes before it was killed. Zero weights make every position worth the same, so the
 * search falls back to the first way out of each cell and the run ends in the time a test may have.
 *
 * It is the shipped code either way, driven through its own public setter — not a stub, and not a
 * second implementation of anything. */
static void prv_make_the_search_hopeless(void)
{
    pacman_lookahead_weights_t hopeless = {0};

    hopeless.death = 1;

    pacman_lookahead_set_weights(&hopeless);
}

void setUp(void)
{
    prv_make_the_search_hopeless();

    g_now_ms = TEST_START_TICK;
    g_region_count = 0U;
    (void)memset(g_page, TEST_ERASED_BYTE, sizeof(g_page));

    /* The random source is the mocking boundary, and zero is the useful answer: `game` asks it for
     * an offset inside a span, so zero means "the shortest timing the jitter allows" — a value, not a
     * variation, which is what a test wants. A test that needs the *nominal* timing switches the
     * jitter off in `game_config_t` instead; one that is about the jitter itself says what it
     * returns. */
    rng_bsp_get_below_IgnoreAndReturn(0U);
    rng_bsp_get_u32_IgnoreAndReturn(0U);
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

    /* Two confirms, because the menu is a list and a confirm per page (DEC-056): who plays, then
     * which maze, and the second one begins the run. */
    shell_press_start();
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
    prv_start_a_run(SHELL_PLAYER_PERSON, SHELL_MAZE_CLASSIC, false);

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
    prv_start_a_run(SHELL_PLAYER_PERSON, SHELL_MAZE_CLASSIC, false);

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

    TEST_ASSERT_EQUAL_UINT32(0U, high_score_get_best((uint8_t)SHELL_MAZE_CLASSIC));

    prv_start_a_run(SHELL_PLAYER_PERSON, SHELL_MAZE_CLASSIC, false);
    prv_play_until_the_run_ends();

    /* Both halves asserted, because a table that refuses everything would pass the second one
     * alone. This test used to leave Pacman standing still and compare his score against the
     * table — and a standing Pacman scores nothing, so it was comparing zero with zero. Pacman
     * starts on an empty cell: he has to be pushed somewhere to eat at all. */
    TEST_ASSERT_NOT_EQUAL_UINT32(0U, game_session_get_score());
    TEST_ASSERT_EQUAL_UINT32(game_session_get_score(), high_score_get_best((uint8_t)SHELL_MAZE_CLASSIC));
}

/* --- the choice of maze (FR-040) ------------------------------------------ */

/* The menu opens on the first page with `PLAY` highlighted, which is the choice a player who pushes
 * nothing at all gets. */
void test_the_menu_opens_on_play(void)
{
    prv_reach_the_menu();

    TEST_ASSERT_EQUAL_UINT(SHELL_MENU_PAGE_PLAYER, shell_get_menu_page());
    TEST_ASSERT_EQUAL_UINT(0U, shell_get_selected_option());
    TEST_ASSERT_EQUAL_UINT(SHELL_PLAYER_PERSON, shell_get_selected_player());
    TEST_ASSERT_EQUAL_UINT(SHELL_MAZE_CLASSIC, shell_get_selected_maze());
}

/* Up and down move within a page and stop at its ends; the highlighted option *is* the choice, so
 * moving the cursor decides and the centre key only leaves the page. */
void test_the_stick_moves_within_a_page_and_stops_at_its_ends(void)
{
    prv_reach_the_menu();

    /* Up at the top does nothing. */
    shell_move_selection(DIRECTION_NORTH);

    TEST_ASSERT_EQUAL_UINT(0U, shell_get_selected_option());
    TEST_ASSERT_EQUAL_UINT(SHELL_PLAYER_PERSON, shell_get_selected_player());

    shell_move_selection(DIRECTION_SOUTH);

    TEST_ASSERT_EQUAL_UINT(1U, shell_get_selected_option());
    TEST_ASSERT_EQUAL_UINT(SHELL_PLAYER_MACHINE, shell_get_selected_player());

    /* Down at the bottom does nothing either. */
    shell_move_selection(DIRECTION_SOUTH);

    TEST_ASSERT_EQUAL_UINT(1U, shell_get_selected_option());
}

/* The centre key moves on a page at a time, and each path is as long as it needs to be: a person
 * picks a maze and goes, the machine picks a maze and then whether to loop. */
void test_the_centre_key_walks_the_pages_and_the_paths_differ(void)
{
    prv_reach_the_menu();

    shell_press_start();

    TEST_ASSERT_EQUAL_UINT(SHELL_MENU_PAGE_MAZE, shell_get_menu_page());

    /* A person's second confirm starts the run — there is no endless page on this path. */
    shell_press_start();

    TEST_ASSERT_EQUAL_UINT(SHELL_SCREEN_GAME, shell_get_screen());
    TEST_ASSERT_FALSE(shell_is_ai_playing());

    prv_play_until_the_run_ends();
    shell_press_start();

    /* And the machine's path has one page more. */
    prv_reach_the_menu();
    shell_move_selection(DIRECTION_SOUTH);
    shell_press_start();

    TEST_ASSERT_EQUAL_UINT(SHELL_MENU_PAGE_MAZE, shell_get_menu_page());

    shell_press_start();

    TEST_ASSERT_EQUAL_UINT(SHELL_MENU_PAGE_ENDLESS, shell_get_menu_page());
    TEST_ASSERT_EQUAL_UINT(SHELL_SCREEN_MENU, shell_get_screen());

    shell_press_start();

    TEST_ASSERT_EQUAL_UINT(SHELL_SCREEN_GAME, shell_get_screen());
    TEST_ASSERT_TRUE(shell_is_ai_playing());
}

/* The board button steps back, and what was chosen on the way in survives the trip.
 *
 * Without a way back, picking `AI` by accident is a trap — every other key goes forwards. And a page
 * that reopened at the top would silently undo a choice, which is worse than not being able to leave.
 */
void test_the_button_steps_back_and_the_choices_survive(void)
{
    prv_reach_the_menu();

    /* Nowhere to go back to on the first page. */
    TEST_ASSERT_FALSE(shell_press_back());

    prv_walk_to(SHELL_PLAYER_MACHINE, SHELL_MAZE_RANDOM, true);

    TEST_ASSERT_EQUAL_UINT(SHELL_MENU_PAGE_ENDLESS, shell_get_menu_page());

    TEST_ASSERT_TRUE(shell_press_back());
    TEST_ASSERT_EQUAL_UINT(SHELL_MENU_PAGE_MAZE, shell_get_menu_page());
    TEST_ASSERT_EQUAL_UINT(SHELL_MAZE_RANDOM, shell_get_selected_maze());
    TEST_ASSERT_EQUAL_UINT(1U, shell_get_selected_option());

    TEST_ASSERT_TRUE(shell_press_back());
    TEST_ASSERT_EQUAL_UINT(SHELL_MENU_PAGE_PLAYER, shell_get_menu_page());
    TEST_ASSERT_EQUAL_UINT(SHELL_PLAYER_MACHINE, shell_get_selected_player());

    TEST_ASSERT_FALSE(shell_press_back());

    /* Forward again, and nothing moved. */
    shell_press_start();

    TEST_ASSERT_EQUAL_UINT(SHELL_MAZE_RANDOM, shell_get_selected_maze());

    shell_press_start();

    TEST_ASSERT_TRUE(shell_is_infinite());
}

/* Choosing a person disarms the loop, because a person's path never reaches the page that could. */
void test_choosing_a_person_switches_the_loop_off(void)
{
    prv_reach_the_menu();
    prv_walk_to(SHELL_PLAYER_MACHINE, SHELL_MAZE_CLASSIC, true);

    TEST_ASSERT_TRUE(shell_is_infinite());

    while (shell_press_back())
    {
        /* back to the first page */
    }

    shell_move_selection(DIRECTION_NORTH);

    TEST_ASSERT_EQUAL_UINT(SHELL_PLAYER_PERSON, shell_get_selected_player());
    TEST_ASSERT_FALSE(shell_is_infinite());
}

/* All four combinations of player and maze are reachable, and the two mazes are the two tables
 * (FR-041) — the machine's runs have no table at all since DEC-056. */
void test_all_four_combinations_are_reachable(void)
{
    prv_reach_the_menu();

    for (uint8_t player = 0U; player < (uint8_t)SHELL_PLAYER_COUNT; ++player)
    {
        for (uint8_t maze = 0U; maze < (uint8_t)SHELL_MAZE_COUNT; ++maze)
        {
            prv_walk_to((shell_player_e)player, (shell_maze_e)maze, false);

            TEST_ASSERT_EQUAL_UINT(player, shell_get_selected_player());
            TEST_ASSERT_EQUAL_UINT(maze, shell_get_selected_maze());
        }
    }
}

/* Left steps back a page — the same thing the board button does, because the stick already points the
 * way the pages go. Right does nothing: confirming is the centre key's job, and a key that quietly
 * took the highlighted option would make a nudged stick start a game. */
void test_left_steps_back_and_right_does_nothing(void)
{
    prv_reach_the_menu();

    /* On the first page there is nowhere to go back to, so left is as inert as right. */
    shell_move_selection(DIRECTION_WEST);
    shell_move_selection(DIRECTION_EAST);

    TEST_ASSERT_EQUAL_UINT(SHELL_MENU_PAGE_PLAYER, shell_get_menu_page());
    TEST_ASSERT_EQUAL_UINT(0U, shell_get_selected_option());

    prv_walk_to(SHELL_PLAYER_MACHINE, SHELL_MAZE_RANDOM, true);

    TEST_ASSERT_EQUAL_UINT(SHELL_MENU_PAGE_ENDLESS, shell_get_menu_page());

    /* Right stays put where there *is* something to leave, which is the half that matters. */
    shell_move_selection(DIRECTION_EAST);

    TEST_ASSERT_EQUAL_UINT(SHELL_MENU_PAGE_ENDLESS, shell_get_menu_page());

    shell_move_selection(DIRECTION_WEST);

    TEST_ASSERT_EQUAL_UINT(SHELL_MENU_PAGE_MAZE, shell_get_menu_page());
    TEST_ASSERT_EQUAL_UINT(SHELL_MAZE_RANDOM, shell_get_selected_maze());

    shell_move_selection(DIRECTION_WEST);

    TEST_ASSERT_EQUAL_UINT(SHELL_MENU_PAGE_PLAYER, shell_get_menu_page());
    TEST_ASSERT_EQUAL_UINT(SHELL_PLAYER_MACHINE, shell_get_selected_player());
}

/* Steering during a run must not change what the run is. The maze is what the high-score table is
 * chosen by (FR-041), so a stick pushed south mid-game moving it would file the score under the other
 * maze. */
void test_the_selection_cannot_be_moved_during_a_run(void)
{
    prv_start_a_run(SHELL_PLAYER_PERSON, SHELL_MAZE_CLASSIC, false);

    shell_move_selection(DIRECTION_SOUTH);

    TEST_ASSERT_EQUAL_UINT(SHELL_MAZE_CLASSIC, shell_get_selected_maze());
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

    /* **Moving the cursor is one rectangle, not a screen.** It costs less than a redrawn menu by a
     * wide margin because neither a row's text nor the scores change when the cursor moves — only
     * changing a row's *value* does, which is why `g_are_rows_drawn` is a flag of its own. */
    TEST_ASSERT_GREATER_THAN_UINT32(0U, move_regions);
    TEST_ASSERT_LESS_THAN_UINT32(menu_regions / 2U, move_regions);
}

/* How much ink the panel carries in a band of rows. */
static uint32_t prv_count_ink_in_rows(int16_t in_first_y, int16_t in_last_y)
{
    uint32_t lit = 0U;

    TEST_ASSERT_NOT_NULL(g_panel);

    for (int16_t y = in_first_y; y < in_last_y; ++y)
    {
        for (int16_t x = 0; x < FRAMEBUFFER_WIDTH; ++x)
        {
            if (g_panel->pixels[y][x] != FRAMEBUFFER_COLOR_BLACK)
            {
                ++lit;
            }
        }
    }

    return lit;
}

/* Whether two bands of the panel hold exactly the same pixels. */
static bool prv_do_bands_match(int16_t in_first_y, int16_t in_second_y, int16_t in_x, int16_t in_width,
                               int16_t in_height)
{
    TEST_ASSERT_NOT_NULL(g_panel);

    for (int16_t row = 0; row < in_height; ++row)
    {
        for (int16_t column = 0; column < in_width; ++column)
        {
            if (g_panel->pixels[in_first_y + row][in_x + column] != g_panel->pixels[in_second_y + row][in_x + column])
            {
                return false;
            }
        }
    }

    return true;
}

/* What the menu has above its two options. On the first page that is the masthead and nothing else;
 * on the endless page it should be nothing at all. */
static uint32_t prv_count_ink_above_the_options(void)
{
    return prv_count_ink_in_rows(0, TEST_MENU_BAND_BOTTOM);
}

/* `PAC-MAN` and the cast belong to the **first** page of the menu and to no page after it: once PLAY
 * or AI has been chosen, the pages that follow are questions to be read.
 *
 * Measured in the panel's own pixels rather than in display items, because the mistake this defends
 * against is one item painting over another. The masthead's title shares its row with `HIGH SCORES`
 * and its cast shares one with the first score, so what survives is decided by the order the three are
 * sent in — and counting items would call every order correct. The endless page is the one worth
 * comparing against: it has no heading and no table either, so anything left in the band is the
 * masthead that was not wiped.
 *
 * Walking back matters as much as walking in. A page has to come back carrying what it carries. */
void test_the_masthead_is_on_the_first_page_only(void)
{
    prv_reach_the_menu();

    const uint32_t on_the_first_page = prv_count_ink_above_the_options();

    TEST_ASSERT_GREATER_THAN_UINT32(0U, on_the_first_page);

    /* The AI's path, because it is the one with all three pages. */
    shell_move_selection(DIRECTION_SOUTH);
    shell_press_start();

    TEST_ASSERT_EQUAL_UINT(SHELL_MENU_PAGE_MAZE, shell_get_menu_page());
    TEST_ASSERT_TRUE(prv_advance(0U));

    shell_press_start();

    TEST_ASSERT_EQUAL_UINT(SHELL_MENU_PAGE_ENDLESS, shell_get_menu_page());
    TEST_ASSERT_TRUE(prv_advance(0U));

    TEST_ASSERT_EQUAL_UINT32(0U, prv_count_ink_above_the_options());

    TEST_ASSERT_TRUE(shell_press_back());
    TEST_ASSERT_TRUE(prv_advance(0U));
    TEST_ASSERT_TRUE(shell_press_back());
    TEST_ASSERT_TRUE(prv_advance(0U));

    TEST_ASSERT_EQUAL_UINT(SHELL_MENU_PAGE_PLAYER, shell_get_menu_page());
    TEST_ASSERT_EQUAL_UINT32(on_the_first_page, prv_count_ink_above_the_options());
}

/* The other half of the wipe: it must take the masthead away **without** taking the page underneath
 * with it.
 *
 * The maze page writes `HIGH SCORES` on the masthead's title row and its first score on the cast's
 * row, so the wipe has to go in before that text and not after it. After it is the tidier-looking
 * code — one call with a flag instead of two guarded ones — and it silently eats a score line. Only a
 * person's path is worth asking, because the AI's maze page has no table to lose (DEC-056).
 */
void test_leaving_the_first_page_wipes_the_masthead_and_not_the_scores(void)
{
    const sprite_t* const zero = sprite_set_get(sprite_set_get_glyph('0'));
    const int16_t number_x = (int16_t)(((FRAMEBUFFER_WIDTH - (TEST_SCORE_FIELD_GLYPHS * zero->width)) / 2)
                                       + ((TEST_SCORE_FIELD_GLYPHS - TEST_SCORE_NUMBER_GLYPHS) * zero->width));
    const int16_t number_width = (int16_t)(TEST_SCORE_NUMBER_GLYPHS * zero->width);

    prv_reach_the_menu();

    /* PLAY is already the highlighted option, so this is one push onward. */
    shell_press_start();

    TEST_ASSERT_EQUAL_UINT(SHELL_MENU_PAGE_MAZE, shell_get_menu_page());
    TEST_ASSERT_TRUE(prv_advance(0U));

    TEST_ASSERT_GREATER_THAN_UINT32(
        0U, prv_count_ink_in_rows(TEST_MENU_FIRST_SCORE_Y, (int16_t)(TEST_MENU_FIRST_SCORE_Y + zero->height)));

    /* **Intact, not merely present.** A wipe that goes in after this page's text eats the cast's own
     * footprint out of the first score and leaves the gaps between the five figures standing, so a
     * count of ink survives it. What does not survive is this: the three places all read 0 on a fresh
     * table, so the row standing where the cast stood has to render exactly like the row below it,
     * which the cast never reaches. Compared to the right of the place digit, which is the one part
     * the two rows do not share. */
    TEST_ASSERT_TRUE_MESSAGE(prv_do_bands_match(TEST_MENU_FIRST_SCORE_Y,
                                                (int16_t)(TEST_MENU_FIRST_SCORE_Y + TEST_MENU_SCORE_PITCH), number_x,
                                                number_width, (int16_t)zero->height),
                             "the first score row lost pixels the second one kept");
}

void test_the_normal_maze_option_plays_the_arcade_maze(void)
{
    prv_start_a_run(SHELL_PLAYER_PERSON, SHELL_MAZE_CLASSIC, false);

    TEST_ASSERT_EQUAL_UINT(GAME_STATE_RUNNING, game_session_get_state());
    TEST_ASSERT_TRUE(prv_is_playing_the_normal_maze());
}

void test_the_random_maze_option_plays_a_generated_maze(void)
{
    prv_reach_the_menu();
    prv_start_a_run(SHELL_PLAYER_PERSON, SHELL_MAZE_RANDOM, false);

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
    prv_start_a_run(SHELL_PLAYER_MACHINE, SHELL_MAZE_CLASSIC, false);

    /* No press of anything else: the agent has Pac-Man because the game is that game. In the normal
     * maze the same assertion is false at this point, which is the difference between the two. */
    TEST_ASSERT_EQUAL_UINT(SHELL_SCREEN_GAME, shell_get_screen());
    TEST_ASSERT_TRUE(shell_is_ai_playing());
    TEST_ASSERT_TRUE(shell_has_ai_played());
    TEST_ASSERT_TRUE(prv_is_playing_the_normal_maze());
}

/* A machine's run reaches **no** table at all (FR-034, DEC-056). Its own table is gone: a run nobody
 * played is not a score anybody set. Both mazes are checked, because a lockout that only covered the
 * one the run was played on would look right from that side. */
void test_a_machines_score_reaches_no_table_at_all(void)
{
    prv_start_a_run(SHELL_PLAYER_MACHINE, SHELL_MAZE_CLASSIC, false);
    prv_play_until_the_run_ends();

    /* It has to have *scored*, or a refused zero would look exactly like a working lockout. */
    TEST_ASSERT_NOT_EQUAL_UINT32(0U, game_session_get_score());
    TEST_ASSERT_EQUAL_UINT32(0U, high_score_get_best((uint8_t)SHELL_MAZE_CLASSIC));
    TEST_ASSERT_EQUAL_UINT32(0U, high_score_get_best((uint8_t)SHELL_MAZE_RANDOM));
}

/* --- the endless mode (FR-043) -------------------------------------------- */

void test_a_game_started_by_hand_is_one_game(void)
{
    prv_reach_the_menu();
    prv_start_a_run(SHELL_PLAYER_MACHINE, SHELL_MAZE_CLASSIC, false);

    TEST_ASSERT_FALSE(shell_is_infinite());
    TEST_ASSERT_EQUAL_UINT32(1U, shell_get_run_count());
}

void test_the_loop_starts_the_next_run_instead_of_returning_to_the_menu(void)
{
    prv_reach_the_menu();
    prv_start_a_run(SHELL_PLAYER_MACHINE, SHELL_MAZE_CLASSIC, true);

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
    prv_walk_to(SHELL_PLAYER_MACHINE, SHELL_MAZE_CLASSIC, true);

    /* Switched off again on the page that switched it on, which is the only place it lives. */
    shell_move_selection(DIRECTION_NORTH);

    TEST_ASSERT_FALSE(shell_is_infinite());

    shell_press_start();
    prv_play_until_the_run_ends();

    (void)prv_advance(SHELL_SCORE_MS);
    (void)prv_advance(0U);

    TEST_ASSERT_EQUAL_UINT(SHELL_SCREEN_MENU, shell_get_screen());
}

/* --- the AI takeover (FR-030/033/034) ------------------------------------- */

/* --- which agent the AI game uses ----------------------------------------- */

/* The two halves in one test on purpose: a lockout that simply broke high scores altogether would
 * pass the first assertion and fail the second. */
void test_an_ai_run_is_kept_out_of_a_persons_table_and_a_player_run_is_not(void)
{
    prv_reach_the_menu();
    prv_start_a_run(SHELL_PLAYER_MACHINE, SHELL_MAZE_CLASSIC, false);

    prv_play_until_the_run_ends();

    TEST_ASSERT_EQUAL_UINT(SHELL_SCREEN_SCORE, shell_get_screen());

    /* The AI's run has to have *scored* for the lockout to mean anything: a refused zero would
     * look exactly like a working lockout. */
    TEST_ASSERT_NOT_EQUAL_UINT32(0U, game_session_get_score());
    TEST_ASSERT_EQUAL_UINT32(0U, high_score_get_best((uint8_t)SHELL_MAZE_CLASSIC));

    /* And now a person's game, which must reach that table. */
    shell_press_start();
    prv_start_a_run(SHELL_PLAYER_PERSON, SHELL_MAZE_CLASSIC, false);
    prv_play_until_the_run_ends();

    TEST_ASSERT_EQUAL_UINT(SHELL_SCREEN_SCORE, shell_get_screen());
    TEST_ASSERT_NOT_EQUAL_UINT32(0U, high_score_get_best((uint8_t)SHELL_MAZE_CLASSIC));
}

/* Every new run begins under player control, whatever the last one did — including the latch FR-034
 * reads, which would otherwise keep a person's next run out of the table for ever. */
void test_a_new_run_starts_under_player_control(void)
{
    prv_reach_the_menu();
    prv_start_a_run(SHELL_PLAYER_MACHINE, SHELL_MAZE_CLASSIC, false);

    TEST_ASSERT_TRUE(shell_is_ai_playing());

    prv_play_until_the_run_ends();

    shell_press_start();
    prv_start_a_run(SHELL_PLAYER_PERSON, SHELL_MAZE_CLASSIC, false);

    TEST_ASSERT_EQUAL_UINT(SHELL_SCREEN_GAME, shell_get_screen());
    TEST_ASSERT_FALSE(shell_is_ai_playing());
    TEST_ASSERT_FALSE(shell_has_ai_played());
}
