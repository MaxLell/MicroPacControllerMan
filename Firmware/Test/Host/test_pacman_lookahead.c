/*
 * Unit tests for App/pacman_lookahead.
 *
 * One of these matters more than the rest, and it is not the one about playing well.
 *
 * `test_deciding_changes_nothing_about_the_run_it_decides_for` plays the same run twice from the
 * same seed — once plainly, once with a search run before every single tick — and requires the two
 * to end in the same place. It is there because the failure it catches is silent: the jitter of
 * FR-044 is drawn from one generator shared by the whole program, so a search that let its clones
 * draw would spend the numbers the *played* game was going to get. Nothing would crash. The ghosts
 * would simply come out of the house at different dots depending on whether anybody was thinking,
 * and on the host it would take FR-114's replayable episode with it.
 *
 * The rest ask the ordinary questions: is the answer a move Pacman may make, does the search go
 * where the food is, does it decline a corridor with death down it, and does it stop when it runs
 * out of budget instead of overrunning the frame.
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* Ceedling links from this file's includes only, not transitively, so the modules the search
 * stands on are named here as well as the one under test. */
#include "active_object.h"
#include "agent.h"
#include "assert_probe.h"
#include "circular_buffer.h"
#include "custom_assert.h"
#include "difficulty.h"
#include "game.h"
#include "ghost.h"
#include "ghost_path.h"
#include "maze_gen.h"
#include "mock_rng_bsp.h"
#include "msg.h"
#include "msg_broker.h"
#include "msg_queue.h"
#include "pacman.h"
#include "pacman_lookahead.h"
#include "playfield.h"
#include "score.h"
#include "unity.h"

/* Both are `static` file-scope rather than local: a `game_t` is about 15 kB and a test that put
 * two of them on the stack would be a test about stack size. */
static game_t g_game;
static game_t g_reference;

/*! \brief Every random number the game has asked for since the test began.
 *
 * The point of the counter is #test_a_search_draws_no_random_numbers: the generator is one shared
 * thing, so "did the search draw" is answerable exactly rather than by comparing outcomes. */
static uint32_t g_draw_count;

/* A draw that is different every time and depends on nothing. Different every time so that an
 * extra draw shifts everything after it — a stub returning a constant would let a search spend
 * numbers without any test noticing. */
static uint32_t prv_count_a_draw(uint32_t in_span, int in_call_count)
{
    (void)in_call_count;

    ++g_draw_count;

    return (in_span == 0U) ? 0U : (g_draw_count % in_span);
}

void setUp(void)
{
    assert_probe_begin();

    g_draw_count = 0U;

    rng_bsp_get_below_Stub(prv_count_a_draw);
    rng_bsp_get_u32_IgnoreAndReturn(0U);

    game_init(&g_game);
    game_init(&g_reference);
}

void tearDown(void)
{
    assert_probe_end();
}

static void prv_start_without_ghosts(game_t* const inout_game)
{
    static playfield_map_t map;
    game_config_t config;

    game_get_default_config(&config);
    config.has_ghosts = false;

    playfield_get_arcade_map(&map);
    game_start_on_map_configured(inout_game, &map, &config);
}

/* The directions Pacman may take from where he stands, in a fixed order. */
static uint8_t prv_collect_open_directions(const game_t* const in_game, direction_e* const out_directions)
{
    static const direction_e k_order[] = {DIRECTION_NORTH, DIRECTION_SOUTH, DIRECTION_EAST, DIRECTION_WEST};
    uint8_t count = 0U;
    uint8_t index;

    for (index = 0U; index < 4U; ++index)
    {
        if (pacman_may_step(&in_game->pacman, game_get_playfield(in_game), k_order[index]))
        {
            out_directions[count] = k_order[index];
            ++count;
        }
    }

    return count;
}

/* --- it answers with a move that exists ----------------------------------- */

void test_the_direction_it_returns_is_one_pacman_may_take(void)
{
    game_start_on_normal_maze(&g_game);

    const direction_e chosen = pacman_lookahead_decide(&g_game);

    TEST_ASSERT_NOT_EQUAL(DIRECTION_NONE, chosen);
    TEST_ASSERT_TRUE(pacman_may_step(&g_game.pacman, game_get_playfield(&g_game), chosen));
}

void test_a_run_that_is_not_running_has_no_direction(void)
{
    /* Never started. "Which way" has no answer, and inventing one would have the caller steer a
     * game that is not being played. */
    TEST_ASSERT_EQUAL(DIRECTION_NONE, pacman_lookahead_decide(&g_game));
}

/* --- it does not disturb the run it is deciding for ------------------------ */

/* The test the module's design is answerable to. Without #game_freeze_timings the searching run
 * pulls words out of the shared generator and its ghosts are paced differently from the plain
 * one's — which is a wrong game rather than a slow one. */
void test_deciding_changes_nothing_about_the_run_it_decides_for(void)
{
    static const direction_e k_pattern[] = {DIRECTION_WEST, DIRECTION_NORTH, DIRECTION_EAST, DIRECTION_SOUTH};
    uint16_t step;

    /* Played plainly. */
    game_start_on_normal_maze(&g_reference);

    for (step = 0U; step < 400U; ++step)
    {
        game_set_direction(&g_reference, k_pattern[step % 4U]);
        game_tick(&g_reference, 16U);
    }

    /* Played identically, with a search thrown away before every tick. */
    game_start_on_normal_maze(&g_game);

    for (step = 0U; step < 400U; ++step)
    {
        (void)pacman_lookahead_decide(&g_game);

        game_set_direction(&g_game, k_pattern[step % 4U]);
        game_tick(&g_game, 16U);
    }

    TEST_ASSERT_EQUAL_UINT32(game_get_score(&g_reference), game_get_score(&g_game));
    TEST_ASSERT_EQUAL_UINT8(game_get_lives(&g_reference), game_get_lives(&g_game));
    TEST_ASSERT_EQUAL_UINT8(game_get_level(&g_reference), game_get_level(&g_game));
    TEST_ASSERT_TRUE(playfield_are_cells_equal(game_get_pacman_cell(&g_reference), game_get_pacman_cell(&g_game)));

    for (uint8_t index = 0U; index < GHOST_COUNT; ++index)
    {
        TEST_ASSERT_TRUE(
            playfield_are_cells_equal(game_get_ghost_cell(&g_reference, index), game_get_ghost_cell(&g_game, index)));
    }
}

/* The same property stated exactly rather than by outcome. The test above would also pass a search
 * that drew random numbers and happened not to change anything over four hundred ticks; this one
 * counts, and the answer has to be none. */
void test_a_search_draws_no_random_numbers(void)
{
    uint16_t step;

    game_start_on_normal_maze(&g_game);

    /* Far enough in that a search has phases part-way through and ghosts on the board — a search
     * over a game that has only just begun would draw nothing whatever it did. */
    for (step = 0U; step < 200U; ++step)
    {
        game_set_direction(&g_game, DIRECTION_WEST);
        game_tick(&g_game, 16U);
    }

    const uint32_t drawn_before = g_draw_count;

    (void)pacman_lookahead_decide(&g_game);

    TEST_ASSERT_EQUAL_UINT32(drawn_before, g_draw_count);
}

/* --- it plays for score --------------------------------------------------- */

/* Not "it plays well" — that is what a whole-run measurement is for. This asks the narrower
 * question the evaluation function is built on: does a branch that eats beat a branch that does
 * not. The ghosts are off so that the only thing separating the two players is where the pellets
 * are. */
void test_it_eats_more_than_a_player_walking_one_way(void)
{
    uint16_t step;

    prv_start_without_ghosts(&g_reference);

    for (step = 0U; step < 600U; ++step)
    {
        game_set_direction(&g_reference, DIRECTION_WEST);
        game_tick(&g_reference, 16U);
    }

    prv_start_without_ghosts(&g_game);

    for (step = 0U; step < 600U; ++step)
    {
        game_set_direction(&g_game, pacman_lookahead_decide(&g_game));
        game_tick(&g_game, 16U);
    }

    TEST_ASSERT_GREATER_THAN_UINT32(game_get_score(&g_reference), game_get_score(&g_game));
}

/* --- it declines a corridor with a ghost down it -------------------------- */

/* Deterministic without knowing the maze: whichever ways out Pacman has, a killing ghost is put a
 * few cells down the first of them and nowhere near the others. The branch that walks into him
 * costs a life, which no amount of score in this search can pay for, so the search has to pick
 * something else. */
void test_it_does_not_walk_into_a_ghost_it_can_see_coming(void)
{
    direction_e open_directions[4];
    uint8_t index;

    game_start_on_normal_maze(&g_game);

    const uint8_t open_count = prv_collect_open_directions(&g_game, open_directions);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT8_MESSAGE(2U, open_count, "the start cell is meant to be a corridor");

    const direction_e deadly = open_directions[0];

    /* Three cells down that way, which is inside what a depth-3 search reaches and outside the
     * cell he is standing on. */
    cell_t ambush = game_get_pacman_cell(&g_game);

    for (index = 0U; index < 3U; ++index)
    {
        const cell_t next = playfield_wrap_cell(playfield_step(ambush, deadly));

        TEST_ASSERT_TRUE_MESSAGE(playfield_is_walkable(game_get_playfield(&g_game), next),
                                 "the corridor is meant to run at least three cells that way");
        ambush = next;
    }

    /* Blinky on the ambush cell and the other three shut in the house, so the only danger on the
     * board is the one this test put there. */
    ghost_reset(&g_game.ghosts[GHOST_BLINKY], GHOST_BLINKY, ambush, false);

    /* Hunting, not scattering — and it has to be said through the *phase*, because `game` writes
     * every ghost's mode from the schedule on every tick and a mode set on the ghost is gone by
     * the next one. A level opens on a scatter phase, in which Blinky heads for his corner, so an
     * ambush laid at the start of a run is a ghost walking *away*: measured, the search rightly
     * went that way and ate. The phases alternate scatter, chase, scatter, so an odd index is a
     * chase, and it is given more time than the search can use so it cannot turn over mid-search.
     */
    g_game.phase_index = 1U;
    g_game.phase_remaining_ms = 60000U;

    for (index = 1U; index < GHOST_COUNT; ++index)
    {
        ghost_reset(&g_game.ghosts[index], (ghost_personality_e)index,
                    playfield_get_ghost_start(game_get_playfield(&g_game), index), true);
    }

    TEST_ASSERT_NOT_EQUAL(deadly, pacman_lookahead_decide(&g_game));
}

/* --- it does not spend the budget watching a wall (RF-019) ---------------- */

/* A leg sets one direction and lets the game steer, so a corridor that bends strands Pacman — and
 * the walk used to have no way of noticing except to wait out
 * #PACMAN_LOOKAHEAD_MAX_CELL_TICKS. Over FR-037's twenty runs that was **17.8 % of every tick the
 * search simulated** ([M6 §15.5](../../../Docu/Design/M6-Pacman-AI.md)).
 *
 * Stated as work per tick, because that is what was being wasted and what a regression would take
 * back. Measured in this fixture: the same budget walks **43** cells of future where waiting the
 * backstop out walked **35**. Forty is between the two with room on both sides — and both numbers
 * are the same search over the same maze, so what separates them is only the waiting.
 */
void test_a_leg_ends_the_moment_pacman_is_stuck(void)
{
    pacman_lookahead_report_t report;

    game_start_on_normal_maze(&g_game);

    (void)pacman_lookahead_decide_within(&g_game, PACMAN_LOOKAHEAD_MAX_DEPTH, PACMAN_LOOKAHEAD_DEFAULT_TICK_BUDGET,
                                         &report);

    TEST_ASSERT_GREATER_OR_EQUAL_UINT16_MESSAGE(40U, report.simulated_cells,
                                                "the budget is going on a Pacman who has already stopped moving");
}

/* --- thinking across frames (M6 §15.6) ------------------------------------ */

/* The property the whole anytime interface stands or falls on: **being interrupted must not change
 * the answer.** A search paid for in slices is only worth having if a slice boundary is invisible
 * to it — otherwise the player's behaviour would depend on how the frames happened to fall, and
 * every measurement taken with one slice size would be about a different player.
 *
 * Stated against the one-shot call, which is the same search run without ever being asked to stop.
 */
void test_a_search_paid_for_in_slices_answers_what_one_paid_for_at_once_does(void)
{
    static const uint16_t k_slices[] = {1U, 7U, 64U, 350U};
    uint16_t index;

    game_start_on_normal_maze(&g_game);

    /* Far enough in that the position is a real one: ghosts out, pellets gone in places, a search
     * with something to choose between. */
    for (uint16_t step = 0U; step < 300U; ++step)
    {
        game_set_direction(&g_game, DIRECTION_WEST);
        game_tick(&g_game, 16U);
    }

    const direction_e at_once =
        pacman_lookahead_decide_within(&g_game, PACMAN_LOOKAHEAD_MAX_DEPTH, PACMAN_LOOKAHEAD_DEFAULT_TICK_BUDGET, NULL);

    for (index = 0U; index < (uint16_t)(sizeof(k_slices) / sizeof(k_slices[0])); ++index)
    {
        pacman_lookahead_restart(&g_game, PACMAN_LOOKAHEAD_MAX_DEPTH, PACMAN_LOOKAHEAD_DEFAULT_TICK_BUDGET);

        /* A slice of one tick cuts the search between two ticks of a single leg, which is the
         * hardest place for it to be put down and picked up again. */
        while (pacman_lookahead_think(k_slices[index]))
        {
            /* until there is nothing left that would change the answer */
        }

        TEST_ASSERT_EQUAL_MESSAGE(at_once, pacman_lookahead_get_direction(),
                                  "a slice boundary changed what the search decided");
    }
}

/* Given the frames a cell really lasts, the search reaches the depth it could never afford inside
 * one — which is the entire point of paying for it in slices. */
void test_thinking_across_frames_reaches_deeper_than_thinking_in_one(void)
{
    pacman_lookahead_report_t in_one_frame;
    pacman_lookahead_report_t across_frames;
    uint8_t frame;

    game_start_on_normal_maze(&g_game);

    (void)pacman_lookahead_decide_within(&g_game, PACMAN_LOOKAHEAD_MAX_DEPTH, PACMAN_LOOKAHEAD_DEFAULT_TICK_BUDGET,
                                         &in_one_frame);

    pacman_lookahead_restart(&g_game, PACMAN_LOOKAHEAD_MAX_DEPTH, PACMAN_LOOKAHEAD_ANYTIME_TICK_BUDGET);

    /* Ten, because a cell lasts 10.6 frames at level-1 speed and the shortest measured is far more
     * than one. */
    for (frame = 0U; frame < 10U; ++frame)
    {
        (void)pacman_lookahead_think(PACMAN_LOOKAHEAD_FRAME_SLICE_TICKS);
    }

    pacman_lookahead_get_report(&across_frames);

    TEST_ASSERT_GREATER_THAN_UINT8_MESSAGE(in_one_frame.reached_depth, across_frames.reached_depth,
                                           "ten frames of thinking bought no more look-ahead than one");
    TEST_ASSERT_NOT_EQUAL(DIRECTION_NONE, pacman_lookahead_get_direction());
}

/* There has to be a legal answer from the very first slice, because the caller sets a direction
 * every frame and a cell can be over in one. */
void test_the_first_slice_already_answers(void)
{
    game_start_on_normal_maze(&g_game);

    pacman_lookahead_restart(&g_game, PACMAN_LOOKAHEAD_MAX_DEPTH, PACMAN_LOOKAHEAD_ANYTIME_TICK_BUDGET);

    const direction_e chosen = pacman_lookahead_get_direction();

    TEST_ASSERT_NOT_EQUAL_MESSAGE(DIRECTION_NONE, chosen, "a search that has not thought yet still owes a move");
    TEST_ASSERT_TRUE(pacman_may_step(&g_game.pacman, game_get_playfield(&g_game), chosen));
}

void test_a_run_that_is_not_running_is_not_thought_about(void)
{
    /* Never started. Restarting on it must not leave `think` with work to do, and the answer to
     * "which way" is still that there is no way. */
    pacman_lookahead_restart(&g_game, PACMAN_LOOKAHEAD_MAX_DEPTH, PACMAN_LOOKAHEAD_ANYTIME_TICK_BUDGET);

    TEST_ASSERT_FALSE(pacman_lookahead_think(PACMAN_LOOKAHEAD_FRAME_SLICE_TICKS));
    TEST_ASSERT_EQUAL(DIRECTION_NONE, pacman_lookahead_get_direction());
}

/* The same promise #test_a_search_draws_no_random_numbers makes, made again for the interface that
 * is actually in the game — and it is the sharper case, because this one holds the root for as long
 * as a cell lasts while the played game ticks underneath it. */
void test_thinking_across_frames_draws_no_random_numbers(void)
{
    uint16_t step;

    game_start_on_normal_maze(&g_game);

    for (step = 0U; step < 200U; ++step)
    {
        game_set_direction(&g_game, DIRECTION_WEST);
        game_tick(&g_game, 16U);
    }

    pacman_lookahead_restart(&g_game, PACMAN_LOOKAHEAD_MAX_DEPTH, PACMAN_LOOKAHEAD_ANYTIME_TICK_BUDGET);

    const uint32_t drawn_before = g_draw_count;

    for (step = 0U; step < 10U; ++step)
    {
        (void)pacman_lookahead_think(PACMAN_LOOKAHEAD_FRAME_SLICE_TICKS);
    }

    TEST_ASSERT_EQUAL_UINT32(drawn_before, g_draw_count);
}

/* --- it stops when the budget is gone ------------------------------------- */

void test_the_report_says_what_the_search_spent(void)
{
    pacman_lookahead_report_t report;

    game_start_on_normal_maze(&g_game);

    (void)pacman_lookahead_decide_within(&g_game, PACMAN_LOOKAHEAD_MAX_DEPTH, PACMAN_LOOKAHEAD_DEFAULT_TICK_BUDGET,
                                         &report);

    TEST_ASSERT_GREATER_THAN_UINT16(0U, report.simulated_ticks);
    TEST_ASSERT_GREATER_THAN_UINT16(0U, report.simulated_cells);
    TEST_ASSERT_GREATER_THAN_UINT16(0U, report.examined_legs);

    /* Never more than it was given, whatever else it did. That is the promise the frame budget
     * rests on, and it is the one assertion here that must not be relaxed: the default budget is
     * *below* what a full depth-3 search wants, on purpose, because the board priced a simulated
     * tick at about 36 us and a frame has 13 ms. So a search of the shipped depth at the shipped
     * budget routinely truncates — that is the design, not a shortfall, and the test says so
     * rather than asserting the comfortable thing.
     *
     * Ticks, not cells: a branch walking Pacman into a wall spends ticks and reaches no cell, so a
     * cell count is the useful work rather than the work. */
    TEST_ASSERT_LESS_OR_EQUAL_UINT16(PACMAN_LOOKAHEAD_DEFAULT_TICK_BUDGET, report.simulated_ticks);
}

/* A budget too small to finish must be *spent*, not overrun, and the caller must be told. A frame
 * that silently ran four times its allowance is the failure this guards. */
void test_a_budget_too_small_truncates_and_says_so(void)
{
    pacman_lookahead_report_t report;

    game_start_on_normal_maze(&g_game);

    const direction_e chosen = pacman_lookahead_decide_within(&g_game, PACMAN_LOOKAHEAD_MAX_DEPTH, 4U, &report);

    TEST_ASSERT_LESS_OR_EQUAL_UINT16(4U, report.simulated_ticks);
    TEST_ASSERT_TRUE(report.was_truncated);

    /* Truncated is not the same as useless: it still answers with the best of what it managed to
     * look at, and that answer still has to be a legal move. */
    TEST_ASSERT_NOT_EQUAL(DIRECTION_NONE, chosen);
    TEST_ASSERT_TRUE(pacman_may_step(&g_game.pacman, game_get_playfield(&g_game), chosen));
}

void test_a_deeper_search_looks_at_more(void)
{
    pacman_lookahead_report_t shallow;
    pacman_lookahead_report_t deep;

    game_start_on_normal_maze(&g_game);

    /* Budgeted generously, because the question here is what the *depth* changes. At the shipped
     * budget both searches stop at the same cell count and the comparison would be measuring the
     * budget — which is its own test, below. */
    (void)pacman_lookahead_decide_within(&g_game, 1U, 4000U, &shallow);
    (void)pacman_lookahead_decide_within(&g_game, PACMAN_LOOKAHEAD_MAX_DEPTH, 4000U, &deep);

    TEST_ASSERT_EQUAL_UINT8(1U, shallow.reached_depth);
    TEST_ASSERT_EQUAL_UINT8(PACMAN_LOOKAHEAD_MAX_DEPTH, deep.reached_depth);
    TEST_ASSERT_GREATER_THAN_UINT16(shallow.examined_legs, deep.examined_legs);
}

/* What the budget a frame can actually pay for buys, pinned rather than assumed — and it is less
 * than the depth suggests. The board prices a simulated tick at about 36 us and a frame has 13 ms
 * spare, so 500 ticks; deepening spends them on depth 1 first, finishes depth 2 out of what is
 * left, and does not finish depth 3. **The shipped player looks two junctions ahead**, not the
 * three its ceiling allows, and that is the honest headline. Nothing here is broken; what would be
 * broken is believing the other number. If this moves it is because a tick got cheaper or dearer,
 * which is worth being told about rather than absorbing. */
void test_the_shipped_budget_reaches_two_junctions(void)
{
    pacman_lookahead_report_t report;

    game_start_on_normal_maze(&g_game);

    (void)pacman_lookahead_decide_within(&g_game, PACMAN_LOOKAHEAD_MAX_DEPTH, PACMAN_LOOKAHEAD_DEFAULT_TICK_BUDGET,
                                         &report);

    TEST_ASSERT_EQUAL_UINT8(2U, report.reached_depth);
    TEST_ASSERT_TRUE(report.was_truncated);
}

/* --- preconditions -------------------------------------------------------- */

void test_deciding_about_nothing_asserts(void)
{
    game_start_on_normal_maze(&g_game);

    ASSERT_PROBE_EXPECT(pacman_lookahead_decide_within(NULL, 1U, 8U, NULL), "in_game != NULL");
    ASSERT_PROBE_EXPECT(pacman_lookahead_decide_within(&g_game, 0U, 8U, NULL), "in_depth > 0U");
    ASSERT_PROBE_EXPECT(pacman_lookahead_decide_within(&g_game, PACMAN_LOOKAHEAD_MAX_DEPTH + 1U, 8U, NULL),
                        "in_depth <= PACMAN_LOOKAHEAD_MAX_DEPTH");
    ASSERT_PROBE_EXPECT(pacman_lookahead_decide_within(&g_game, 1U, 0U, NULL), "in_tick_budget > 0U");

    ASSERT_PROBE_EXPECT(pacman_lookahead_restart(NULL, 1U, 8U), "in_game != NULL");
    ASSERT_PROBE_EXPECT(pacman_lookahead_restart(&g_game, 0U, 8U), "in_depth > 0U");
    ASSERT_PROBE_EXPECT(pacman_lookahead_restart(&g_game, PACMAN_LOOKAHEAD_MAX_DEPTH + 1U, 8U),
                        "in_depth <= PACMAN_LOOKAHEAD_MAX_DEPTH");
    ASSERT_PROBE_EXPECT(pacman_lookahead_restart(&g_game, 1U, 0U), "in_tick_budget > 0U");
    ASSERT_PROBE_EXPECT((void)pacman_lookahead_think(0U), "in_slice_ticks > 0U");
    ASSERT_PROBE_EXPECT(pacman_lookahead_get_report(NULL), "out_report != NULL");
}
