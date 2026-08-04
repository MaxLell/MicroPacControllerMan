/*
 * test_game_session.c
 *
 * The frame loop three callers share, and the two traps in it.
 *
 * The display port is mocked and the tick is driven, so "the frame is due" is stated
 * rather than waited for. What is checked here is the *pacing and the draining* — the
 * rules are `test_game`'s, the pixels are `test_game_view`'s and `test_render`'s.
 */

#include "unity.h"

#include <stdbool.h>
#include <stdint.h>

/* Ceedling picks the sources to link from the includes it sees here, so the whole frame
 * path has to be named even where this file calls none of it directly. */
#include "active_object.h"
#include "agent.h"
#include "assert_probe.h"
#include "circular_buffer.h"
#include "custom_assert.h"
#include "difficulty.h"
#include "framebuffer.h"
#include "game.h"
#include "game_session.h"
#include "game_view.h"
#include "gfx.h"
#include "ghost.h"
#include "ghost_path.h"
#include "maze_gen.h"
#include "mock_display.h"
#include "mock_systick_bsp.h"
#include "msg.h"
#include "msg_broker.h"
#include "msg_queue.h"
#include "pacman.h"
#include "playfield.h"
#include "render.h"
#include "score.h"
#include "sprite.h"
#include "sprite_set.h"
#include "sw_timer.h"

/* Compiled in only under TEST — see sw_timer.c and game_session.c. */
void sw_timer_test_reset(void);
void game_session_test_reset(void);

/* ==========================================================================
 * fixtures
 * ========================================================================= */

#define TEST_START_TICK              (1000U)

/* A handover sends the whole field; a steady frame sends the actors. The bound only has
 * to separate the two, so it is generous on purpose: what would break this test is the
 * field still trickling out frames later, not a sprite more or less. */
#define TEST_STEADY_MAX_REGION_COUNT (32U)

static uint32_t g_now_ms;
static uint32_t g_region_count;

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

/* Move the clock, then let the timers see it. The session's timer is the only one
 * running, so this is exactly "time passes". */
static void prv_advance(uint32_t in_elapsed_ms)
{
    g_now_ms += in_elapsed_ms;

    systick_bsp_get_tick_IgnoreAndReturn(g_now_ms);
    sw_timer_process();
}

/* One frame's worth of drawing, counted in regions presented. */
static uint32_t prv_run_one_frame(void)
{
    g_region_count = 0U;

    prv_advance(GAME_SESSION_FRAME_PERIOD_MS);

    TEST_ASSERT_TRUE(game_session_service());

    return g_region_count;
}

void setUp(void)
{
    g_now_ms = TEST_START_TICK;
    g_region_count = 0U;

    display_init_Ignore();
    display_present_Ignore();
    display_service_Ignore();
    display_present_region_Stub(prv_count_region);

    systick_bsp_get_tick_IgnoreAndReturn(g_now_ms);

    sw_timer_test_reset();
    game_session_test_reset();
    sw_timer_init();

    game_session_init();
}

void tearDown(void)
{
}

/* ==========================================================================
 * tests
 * ========================================================================= */

void test_no_frame_is_drawn_before_one_is_due(void)
{
    TEST_ASSERT_FALSE(game_session_service());

    prv_advance(GAME_SESSION_FRAME_PERIOD_MS - 1U);

    TEST_ASSERT_FALSE(game_session_service());
}

void test_a_frame_is_drawn_once_the_period_has_passed(void)
{
    prv_advance(GAME_SESSION_FRAME_PERIOD_MS);

    TEST_ASSERT_TRUE(game_session_service());

    /* And exactly once: the loop calls this far more often than the panel is redrawn. */
    TEST_ASSERT_FALSE(game_session_service());
}

/* The regression for the blank screen. `sw_timer` is one-shot, so the frame callback has
 * to re-arm its own timer; when it did not, exactly one frame ever ran — and the first
 * frame is the field handover, which deliberately draws no actors, so the panel showed a
 * maze and then nothing at all, for ever.
 */
void test_the_frames_keep_coming(void)
{
    const uint32_t expected_frame_count = 10U;
    uint32_t frame_count = 0U;

    for (uint32_t frame = 0U; frame < expected_frame_count; ++frame)
    {
        prv_advance(GAME_SESSION_FRAME_PERIOD_MS);

        if (game_session_service())
        {
            ++frame_count;
        }
    }

    TEST_ASSERT_EQUAL_UINT32(expected_frame_count, frame_count);
}

/* Start a run on the arcade's own maze, so a test may say which way Pacman can walk. The
 * production path is `game_session_start`, with a seed and a generated maze; that one is
 * exercised by `test_a_seeded_run_hands_over_a_generated_maze` below. */
static void prv_start_run(void)
{
    playfield_map_t map;

    playfield_get_arcade_map(&map);
    game_session_start_on_map(&map);
}

/* The other trap: a level hands the whole field over across several display lists, and a
 * frame that drained one of them would leave the panel half drawn. The handover therefore
 * has to finish inside the frame that starts it.
 */
void test_the_field_is_handed_over_in_a_single_frame(void)
{
    uint32_t handover_region_count;
    uint32_t steady_region_count;

    prv_start_run();

    handover_region_count = prv_run_one_frame();

    /* Two more frames, so what is measured is a settled game rather than the tail of the
     * handover. */
    (void)prv_run_one_frame();
    steady_region_count = prv_run_one_frame();

    TEST_ASSERT_GREATER_THAN_UINT32(TEST_STEADY_MAX_REGION_COUNT, handover_region_count);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(TEST_STEADY_MAX_REGION_COUNT, steady_region_count);
}

void test_starting_a_run_puts_the_game_at_the_beginning(void)
{
    prv_start_run();

    TEST_ASSERT_EQUAL_UINT(GAME_STATE_RUNNING, game_session_get_state());
    TEST_ASSERT_EQUAL_UINT8(GAME_STARTING_LIVES, game_session_get_lives());
    TEST_ASSERT_EQUAL_UINT8(1U, game_session_get_level());
    TEST_ASSERT_EQUAL_UINT32(0U, game_session_get_score());
}

/* The production path: a seed rather than a map, so the maze is generated. What is checked is
 * that the whole chain still works when nobody knows what the maze looks like — the game
 * generates one, the view derives its appearance, and the field is handed over in one frame.
 * The maze reaching the view is the part that could silently not happen: it cannot ride inside
 * the state message, so the session has to notice the level changing and hand it over itself.
 */
void test_a_seeded_run_hands_over_a_generated_maze(void)
{
    uint32_t handover_region_count;
    uint32_t steady_region_count;

    game_session_start(20260804U);

    TEST_ASSERT_EQUAL_UINT(GAME_STATE_RUNNING, game_session_get_state());

    handover_region_count = prv_run_one_frame();

    (void)prv_run_one_frame();
    steady_region_count = prv_run_one_frame();

    /* A whole field went out, and then frames settled to actor-sized ones — which together say
     * the view had a maze to draw. Without one it would have drawn nothing at all. */
    TEST_ASSERT_GREATER_THAN_UINT32(TEST_STEADY_MAX_REGION_COUNT, handover_region_count);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(TEST_STEADY_MAX_REGION_COUNT, steady_region_count);
}

/* A direction is a request, not a move: it is granted at the first cell where the turn is
 * possible (§10.1). Whether it is granted is `test_pacman`'s business — what matters here
 * is that the session passes it on rather than swallowing it, so points appear.
 */
void test_the_direction_reaches_the_game(void)
{
    prv_start_run();

    game_session_set_direction(DIRECTION_WEST);

    for (uint32_t frame = 0U; frame < 100U; ++frame)
    {
        (void)prv_run_one_frame();
    }

    TEST_ASSERT_GREATER_THAN_UINT32(0U, game_session_get_score());
}
