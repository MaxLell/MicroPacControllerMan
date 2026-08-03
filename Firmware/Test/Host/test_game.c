/*
 * Unit tests for App/game.
 *
 * Game is the only module that owns the whole Model, so these are the closest thing to
 * playing the game without a screen: time goes in as milliseconds, and what comes out is a
 * snapshot, a score and a state. That is exactly the seam VT-INT-010/014/017 need — a run,
 * a lost life and a cleared level, all driven deterministically and in an instant.
 *
 * Two liberties are taken with the Model, both deliberate. Pellets are removed straight
 * from `game.playfield` to set up a nearly-cleared level, and ghosts are placed straight
 * onto a cell to force a meeting. Playing into those states through movement alone would
 * take thousands of ticks and depend on four chase algorithms staying exactly as they are
 * today, which would make these tests fail for reasons that have nothing to do with the
 * game rules they check.
 *
 * Pacman only moves once a direction has been asked for, so a test that never calls
 * #game_set_direction has a stationary Pacman and a fully predictable maze.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Ceedling links from this file's includes only, not transitively — and the game sits on
 * the entire Model plus the bus. */
#include "active_object.h"
#include "agent.h"
#include "assert_probe.h"
#include "circular_buffer.h"
#include "custom_assert.h"
#include "difficulty.h"
#include "game.h"
#include "ghost.h"
#include "ghost_path.h"
#include "msg.h"
#include "msg_broker.h"
#include "msg_queue.h"
#include "pacman.h"
#include "playfield.h"
#include "score.h"
#include "unity.h"

#define PELLET_POINTS         (10U)
#define POWER_PELLET_POINTS   (50U)

/* The arcade sets Pacman down in the short corridor below the ghost house, walled above
 * and below, so the only way off his start cell is sideways (§10.2). West is the side with
 * a pellet on it, and that single step is what nearly every movement test here takes. */
#define PACMAN_START_X        (13)
#define PACMAN_START_Y        (23)
#define STEPPED_X             (12)
#define STEPPED_Y             (23)
#define STEP_DIRECTION        DIRECTION_WEST
#define BACK_DIRECTION        DIRECTION_EAST

#define LEVEL_1               (1U)
#define LEVEL_2               (2U)
#define LEVEL_5               (5U)

/* Level 1's frightened window (§10.9), transcribed so a change to the table shows up here
 * as a failing test rather than as a silently different game. */
#define LEVEL_1_FRIGHTENED_MS (6000U)

static game_t g_game;

/* How long Pacman's *next* step will take.
 *
 * There is no one answer any more: the period is the level's, he is quicker while the
 * ghosts are blue, and he is slower on the step after a mouthful (§10.9). The tests ask
 * the same table the game does rather than keep a copy of the numbers, so a change to the
 * table moves both together — what they are really asserting is the behaviour around it. */
static uint32_t prv_pacman_period_ms(void)
{
    difficulty_t difficulty;

    difficulty_get(g_game.level, &difficulty);

    if (g_game.frightened_remaining_ms > 0U)
    {
        return g_game.did_pacman_eat_last_step ? difficulty.pacman_frightened_eating_period_ms
                                               : difficulty.pacman_frightened_period_ms;
    }

    return g_game.did_pacman_eat_last_step ? difficulty.pacman_eating_period_ms : difficulty.pacman_period_ms;
}

static cell_t prv_make_cell(int16_t in_x, int16_t in_y)
{
    const cell_t cell = {in_x, in_y};

    return cell;
}

/* Everything except one cell eaten, so the next bite clears the level. */
static void prv_eat_every_pellet_except(cell_t in_kept_cell)
{
    for (int16_t y = 0; y < PLAYFIELD_HEIGHT; ++y)
    {
        for (int16_t x = 0; x < PLAYFIELD_WIDTH; ++x)
        {
            const cell_t cell = prv_make_cell(x, y);

            if (playfield_are_cells_equal(cell, in_kept_cell))
            {
                continue;
            }

            (void)playfield_eat_pellet(&g_game.playfield, cell);
        }
    }
}

/* Move Pacman one cell off his start, onto whatever is on the cell beside him. */
static void prv_step_onto_the_pellet(void)
{
    game_set_direction(&g_game, STEP_DIRECTION);
    game_tick(&g_game, prv_pacman_period_ms());
}

/* Drop a run straight onto a level, rather than play the ones before it. Everyone is
 * replaced the way loading a level does it; the scatter/chase plan restarts from zero,
 * which is enough for the timings these set up. */
static void prv_jump_to_level(uint8_t in_level)
{
    g_game.level = in_level;

    difficulty_get(in_level, &g_game.difficulty);
    playfield_load(&g_game.playfield);
    pacman_reset(&g_game.pacman, playfield_get_pacman_start(&g_game.playfield));

    for (uint8_t index = 0U; index < GHOST_COUNT; ++index)
    {
        const cell_t start = playfield_get_ghost_start(&g_game.playfield, index);

        ghost_reset(&g_game.ghosts[index], (ghost_personality_e)index, start,
                    playfield_is_house(&g_game.playfield, start));
        g_game.ghost_move_elapsed_ms[index] = 0U;

        /* Loading a level lets out everyone whose dot limit is already zero (§10.4), and a
         * test that jumps straight to a level has to do the same or its ghosts never move. */
        if (index != (uint8_t)GHOST_INKY && index != (uint8_t)GHOST_CLYDE)
        {
            ghost_release_from_house(&g_game.ghosts[index]);
        }
    }

    /* Inky and Clyde only from the level their limit falls to zero. */
    if (g_game.difficulty.inky_dot_limit == 0U)
    {
        ghost_release_from_house(&g_game.ghosts[GHOST_INKY]);
    }

    if (g_game.difficulty.clyde_dot_limit == 0U)
    {
        ghost_release_from_house(&g_game.ghosts[GHOST_CLYDE]);
    }

    g_game.pacman_move_elapsed_ms = 0U;
    g_game.did_pacman_eat_last_step = false;
    g_game.frightened_remaining_ms = 0U;
    g_game.phase_index = 0U;
    g_game.phase_remaining_ms = 0U;
}

/* Advance in small slices, the way the super-loop does, rather than in one huge jump — the
 * timers are meant to survive being fed real frame times. */
static void prv_tick_for(uint32_t in_total_ms, uint32_t in_step_ms)
{
    for (uint32_t elapsed = 0U; elapsed < in_total_ms; elapsed += in_step_ms)
    {
        game_tick(&g_game, in_step_ms);
    }
}

/* The maze differs per level, so "the way out" does too. Any neighbouring cell with a
 * pellet on it will do — Pacman has to move to eat, and eating a cell that never held a
 * pellet would put the maze's remaining count out of step with its contents. */
static direction_e prv_find_a_way_out_with_a_pellet(void)
{
    static const direction_e k_directions[] = {DIRECTION_NORTH, DIRECTION_SOUTH, DIRECTION_EAST, DIRECTION_WEST};
    const cell_t cell = pacman_get_cell(&g_game.pacman);

    for (uint8_t index = 0U; index < (sizeof(k_directions) / sizeof(k_directions[0])); ++index)
    {
        const cell_t neighbour = playfield_step(cell, k_directions[index]);

        if (playfield_is_walkable(&g_game.playfield, neighbour)
            && (playfield_get_pellet(&g_game.playfield, neighbour) != PLAYFIELD_PELLET_NONE))
        {
            return k_directions[index];
        }
    }

    return DIRECTION_NONE;
}

/* Eat a power pellet on whatever level is loaded, then park Pacman where he landed so the
 * only thing still running is the window itself. */
static void prv_eat_a_power_pellet(void)
{
    const direction_e direction = prv_find_a_way_out_with_a_pellet();
    cell_t target;

    TEST_ASSERT_NOT_EQUAL(DIRECTION_NONE, direction);

    target = playfield_step(pacman_get_cell(&g_game.pacman), direction);
    g_game.playfield.pellets[target.y][target.x] = PLAYFIELD_PELLET_POWER;

    game_set_direction(&g_game, direction);
    game_tick(&g_game, prv_pacman_period_ms());

    pacman_reset(&g_game.pacman, pacman_get_cell(&g_game.pacman));
}

/* How long the ghosts stay edible on the level currently loaded. */
static uint32_t prv_measure_frightened_ms(void)
{
    const uint32_t step_ms = 10U;
    const uint32_t limit_ms = 60000U;
    uint32_t elapsed_ms = 0U;

    prv_eat_a_power_pellet();

    while (game_is_frightened_active(&g_game) && (elapsed_ms < limit_ms))
    {
        game_tick(&g_game, step_ms);
        elapsed_ms += step_ms;
    }

    return elapsed_ms;
}

/* How long a ghost takes to make its first step from where it stands — its move period,
 * observed rather than read out of the table it comes from. Resolution is the 10 ms slice,
 * which is finer than any two periods this distinguishes between. */
static uint32_t prv_measure_ghost_period_ms(uint8_t in_index)
{
    const cell_t start_cell = ghost_get_cell(&g_game.ghosts[in_index]);
    const uint32_t step_ms = 10U;
    const uint32_t limit_ms = 1000U;
    uint32_t elapsed_ms = 0U;

    while (elapsed_ms < limit_ms)
    {
        game_tick(&g_game, step_ms);
        elapsed_ms += step_ms;

        if (!playfield_are_cells_equal(start_cell, ghost_get_cell(&g_game.ghosts[in_index])))
        {
            return elapsed_ms;
        }
    }

    return 0U;
}

/* Leave `in_count` pellets in the maze, which is how Cruise Elroy is armed (§10.9). The
 * cell above Pacman is spared last so the tests that then step him still have something to
 * eat, and so the level cannot clear itself out from under them. */
static void prv_leave_this_many_pellets(uint16_t in_count)
{
    for (int16_t y = 0; y < PLAYFIELD_HEIGHT; ++y)
    {
        for (int16_t x = 0; x < PLAYFIELD_WIDTH; ++x)
        {
            const cell_t cell = prv_make_cell(x, y);

            if (playfield_get_remaining_pellet_count(&g_game.playfield) <= in_count)
            {
                return;
            }

            if (playfield_are_cells_equal(cell, prv_make_cell(STEPPED_X, STEPPED_Y)))
            {
                continue;
            }

            (void)playfield_eat_pellet(&g_game.playfield, cell);
        }
    }
}

void setUp(void)
{
    assert_probe_begin();

    memset(&g_game, 0, sizeof(g_game));
    game_init(&g_game);
}

void tearDown(void)
{
    assert_probe_end();
}

/* The state is handed out by value now, so the tests take a copy each time they look.
 * That is the point of DEC-016 — nothing here can hold a reference into the game. */
static msg_game_state_t prv_state(void)
{
    msg_game_state_t state;

    game_get_state_message(&g_game, &state);

    return state;
}

/* Somewhere for the precondition probe to point at; never read. */
static msg_game_state_t g_probe_state;

static bool prv_is_ghost_frightened(uint8_t in_index)
{
    return (prv_state().frightened_ghosts & (uint8_t)(1U << in_index)) != 0U;
}

/* --- starting up --------------------------------------------------------- */

void test_an_initialized_game_is_idle(void)
{
    TEST_ASSERT_EQUAL(GAME_STATE_IDLE, game_get_state(&g_game));
    TEST_ASSERT_EQUAL_UINT8(0U, game_get_lives(&g_game));
    TEST_ASSERT_EQUAL_UINT32(0U, game_get_score(&g_game));
    TEST_ASSERT_EQUAL_UINT8(LEVEL_1, game_get_level(&g_game));
}

void test_an_idle_game_ignores_time(void)
{
    const msg_game_state_t before = prv_state();
    msg_game_state_t after;

    prv_tick_for(10U * prv_pacman_period_ms(), prv_pacman_period_ms());

    after = prv_state();

    /* Compared as bytes rather than field by field, because "nothing at all happened" is
     * the claim — a field added later should be covered by it without being remembered. */
    TEST_ASSERT_EQUAL_MEMORY(&before, &after, sizeof(before));
}

void test_starting_a_run_gives_a_full_set_of_lives_at_level_one(void)
{
    game_start(&g_game);

    TEST_ASSERT_EQUAL(GAME_STATE_RUNNING, game_get_state(&g_game));
    TEST_ASSERT_EQUAL_UINT8(GAME_STARTING_LIVES, game_get_lives(&g_game));
    TEST_ASSERT_EQUAL_UINT8(LEVEL_1, game_get_level(&g_game));
    TEST_ASSERT_EQUAL_UINT32(0U, game_get_score(&g_game));
}

void test_starting_a_run_puts_pacman_on_the_level_start_cell(void)
{
    msg_game_state_t state;

    game_start(&g_game);
    state = prv_state();

    TEST_ASSERT_EQUAL_UINT8(PACMAN_START_X, state.pacman.column);
    TEST_ASSERT_EQUAL_UINT8(PACMAN_START_Y, state.pacman.row);
}

void test_a_second_run_starts_from_scratch(void)
{
    game_start(&g_game);
    prv_step_onto_the_pellet();

    game_start(&g_game);

    /* The score, the lives and the maze all come back — a run must not inherit the last
     * one's pellets or points. */
    TEST_ASSERT_EQUAL_UINT32(0U, game_get_score(&g_game));
    TEST_ASSERT_EQUAL_UINT8(GAME_STARTING_LIVES, game_get_lives(&g_game));
    TEST_ASSERT_TRUE(msg_cell_bitmap_get(prv_state().has_pellet, STEPPED_X, STEPPED_Y));
}

/* --- movement and timing (§10.1) ----------------------------------------- */

void test_pacman_stands_still_until_a_direction_is_asked_for(void)
{
    game_start(&g_game);

    prv_tick_for(10U * prv_pacman_period_ms(), prv_pacman_period_ms());

    TEST_ASSERT_EQUAL_INT16(PACMAN_START_X, prv_state().pacman.column);
    TEST_ASSERT_EQUAL_INT16(PACMAN_START_Y, prv_state().pacman.row);
}

void test_pacman_does_not_move_before_his_period_has_elapsed(void)
{
    game_start(&g_game);

    game_set_direction(&g_game, STEP_DIRECTION);
    game_tick(&g_game, (prv_pacman_period_ms() - 1U));

    TEST_ASSERT_EQUAL_INT16(PACMAN_START_Y, prv_state().pacman.row);
}

void test_pacman_moves_one_cell_per_period(void)
{
    game_start(&g_game);

    prv_step_onto_the_pellet();

    TEST_ASSERT_EQUAL_INT16(STEPPED_X, prv_state().pacman.column);
    TEST_ASSERT_EQUAL_INT16(STEPPED_Y, prv_state().pacman.row);
    TEST_ASSERT_EQUAL(STEP_DIRECTION, prv_state().pacman.direction);
}

void test_leftover_time_is_carried_into_the_next_move(void)
{
    difficulty_t difficulty;
    uint32_t first_step_ms;
    uint32_t second_step_ms;

    game_start(&g_game);
    difficulty_get(LEVEL_1, &difficulty);

    /* The two steps are not the same length: the first eats the pellet beside him and the
     * second is slower for it (§10.9). Budgeting twice the first would come up short, and
     * the test would fail for the wrong reason. */
    first_step_ms = difficulty.pacman_period_ms;
    second_step_ms = difficulty.pacman_eating_period_ms;

    game_set_direction(&g_game, STEP_DIRECTION);

    /* Two thirds of a step at a time, so no single slice ever completes one: time has to
     * accumulate across ticks rather than be discarded at each. */
    prv_tick_for(first_step_ms + second_step_ms, (first_step_ms * 2U) / 3U);

    TEST_ASSERT_EQUAL_INT16(STEPPED_X - 1, prv_state().pacman.column);
}

void test_a_direction_is_ignored_unless_a_run_is_in_progress(void)
{
    /* Still idle: the input must not be remembered and then acted on at the next start. */
    game_set_direction(&g_game, STEP_DIRECTION);

    game_start(&g_game);
    game_tick(&g_game, prv_pacman_period_ms());

    TEST_ASSERT_EQUAL_INT16(PACMAN_START_Y, prv_state().pacman.row);
}

/* --- pellets and scoring (§10.6) ----------------------------------------- */

void test_eating_a_pellet_scores_and_clears_the_cell(void)
{
    game_start(&g_game);

    prv_step_onto_the_pellet();

    TEST_ASSERT_EQUAL_UINT32(PELLET_POINTS, game_get_score(&g_game));
    TEST_ASSERT_FALSE(msg_cell_bitmap_get(prv_state().has_pellet, STEPPED_X, STEPPED_Y));
}

void test_a_cell_only_pays_once(void)
{
    game_start(&g_game);

    prv_step_onto_the_pellet();
    game_set_direction(&g_game, BACK_DIRECTION);
    game_tick(&g_game, prv_pacman_period_ms());
    game_set_direction(&g_game, STEP_DIRECTION);
    game_tick(&g_game, prv_pacman_period_ms());

    /* Back on the cell he cleared, having turned round on his own start cell, which never
     * had a pellet on it — so the one bite is still the only one that paid. */
    TEST_ASSERT_EQUAL_UINT32(PELLET_POINTS, game_get_score(&g_game));
}

void test_a_power_pellet_scores_fifty_and_frightens_the_ghosts(void)
{
    game_start(&g_game);
    g_game.playfield.pellets[STEPPED_Y][STEPPED_X] = PLAYFIELD_PELLET_POWER;

    prv_step_onto_the_pellet();

    TEST_ASSERT_EQUAL_UINT32(POWER_PELLET_POINTS, game_get_score(&g_game));
    TEST_ASSERT_TRUE(game_is_frightened_active(&g_game));

    for (uint8_t index = 0U; index < GHOST_COUNT; ++index)
    {
        TEST_ASSERT_TRUE(prv_is_ghost_frightened(index));
    }
}

void test_the_frightened_window_runs_out(void)
{
    /* A margin, split off the end of the window: enough to tell "still running" from "just
     * expired" without depending on how long Pacman's step happened to take. */
    const uint32_t margin_ms = 100U;

    game_start(&g_game);
    g_game.playfield.pellets[STEPPED_Y][STEPPED_X] = PLAYFIELD_PELLET_POWER;

    prv_step_onto_the_pellet();

    /* Park Pacman where he is, so the window is the only thing this measures. The step that
     * ate the pellet cost the window nothing: the timers advance before movement, so it
     * starts full. */
    pacman_reset(&g_game.pacman, prv_make_cell(STEPPED_X, STEPPED_Y));

    prv_tick_for(LEVEL_1_FRIGHTENED_MS - margin_ms, 10U);
    TEST_ASSERT_TRUE(game_is_frightened_active(&g_game));

    prv_tick_for(margin_ms, 10U);
    TEST_ASSERT_FALSE(game_is_frightened_active(&g_game));
    TEST_ASSERT_FALSE(prv_is_ghost_frightened(GHOST_BLINKY));
}

/* --- meeting a ghost (§10.7) --------------------------------------------- */

void test_walking_into_a_ghost_costs_a_life_and_resets_the_positions(void)
{
    game_start(&g_game);
    ghost_reset(&g_game.ghosts[GHOST_BLINKY], GHOST_BLINKY, prv_make_cell(STEPPED_X, STEPPED_Y), false);

    prv_step_onto_the_pellet();

    TEST_ASSERT_EQUAL(GAME_STATE_RUNNING, game_get_state(&g_game));
    TEST_ASSERT_EQUAL_UINT8(GAME_STARTING_LIVES - 1U, game_get_lives(&g_game));
    TEST_ASSERT_EQUAL_INT16(PACMAN_START_Y, prv_state().pacman.row);
}

void test_the_eaten_pellets_survive_a_lost_life(void)
{
    game_start(&g_game);
    prv_step_onto_the_pellet();
    ghost_reset(&g_game.ghosts[GHOST_BLINKY], GHOST_BLINKY, prv_make_cell(PACMAN_START_X, PACMAN_START_Y), false);

    game_set_direction(&g_game, BACK_DIRECTION);
    game_tick(&g_game, prv_pacman_period_ms());

    TEST_ASSERT_EQUAL_UINT8(GAME_STARTING_LIVES - 1U, game_get_lives(&g_game));
    TEST_ASSERT_FALSE(msg_cell_bitmap_get(prv_state().has_pellet, STEPPED_X, STEPPED_Y));
    TEST_ASSERT_EQUAL_UINT32(PELLET_POINTS, game_get_score(&g_game));
}

void test_the_last_life_ends_the_run(void)
{
    game_start(&g_game);
    g_game.lives = 1U;
    ghost_reset(&g_game.ghosts[GHOST_BLINKY], GHOST_BLINKY, prv_make_cell(STEPPED_X, STEPPED_Y), false);

    prv_step_onto_the_pellet();

    TEST_ASSERT_EQUAL(GAME_STATE_OVER, game_get_state(&g_game));
    TEST_ASSERT_EQUAL_UINT8(0U, game_get_lives(&g_game));
}

void test_a_finished_run_ignores_further_time(void)
{
    game_start(&g_game);
    g_game.lives = 1U;
    ghost_reset(&g_game.ghosts[GHOST_BLINKY], GHOST_BLINKY, prv_make_cell(STEPPED_X, STEPPED_Y), false);
    prv_step_onto_the_pellet();

    const msg_game_state_t before = prv_state();
    msg_game_state_t after;

    prv_tick_for(10U * prv_pacman_period_ms(), prv_pacman_period_ms());

    after = prv_state();

    TEST_ASSERT_EQUAL_MEMORY(&before, &after, sizeof(before));
}

void test_eating_a_frightened_ghost_scores_and_sends_it_back_to_the_pen(void)
{
    game_start(&g_game);
    g_game.playfield.pellets[STEPPED_Y][STEPPED_X] = PLAYFIELD_PELLET_POWER;
    prv_step_onto_the_pellet();

    /* Frightened now, so the next meeting goes the other way (§10.5). */
    ghost_reset(&g_game.ghosts[GHOST_BLINKY], GHOST_BLINKY, prv_make_cell(PACMAN_START_X, PACMAN_START_Y), false);
    ghost_set_mode(&g_game.ghosts[GHOST_BLINKY], GHOST_MODE_FRIGHTENED);

    game_set_direction(&g_game, BACK_DIRECTION);
    game_tick(&g_game, prv_pacman_period_ms());

    TEST_ASSERT_EQUAL_UINT8(GAME_STARTING_LIVES, game_get_lives(&g_game));
    TEST_ASSERT_EQUAL_UINT32(POWER_PELLET_POINTS + 200U, game_get_score(&g_game));
}

void test_passing_through_a_ghost_still_counts_as_meeting_it(void)
{
    game_start(&g_game);

    /* The ghost is where Pacman is and Pacman is heading for where the ghost will be —
     * they swap cells in one step and would otherwise slip past each other. */
    ghost_reset(&g_game.ghosts[GHOST_BLINKY], GHOST_BLINKY, prv_make_cell(STEPPED_X, STEPPED_Y), false);
    ghost_set_mode(&g_game.ghosts[GHOST_BLINKY], GHOST_MODE_CHASE);

    /* Only the ghosts move in this slice, straight down onto Pacman's cell as he leaves
     * it. Blinky targets Pacman directly, so south is the step he takes. */
    prv_step_onto_the_pellet();

    TEST_ASSERT_EQUAL_UINT8(GAME_STARTING_LIVES - 1U, game_get_lives(&g_game));
}

/* --- levels (§10.9, FR-025/027) ------------------------------------------ */

void test_clearing_a_level_loads_the_next_one_and_keeps_the_score(void)
{
    game_start(&g_game);
    prv_eat_every_pellet_except(prv_make_cell(STEPPED_X, STEPPED_Y));

    prv_step_onto_the_pellet();

    TEST_ASSERT_EQUAL_UINT8(LEVEL_2, game_get_level(&g_game));
    TEST_ASSERT_EQUAL(GAME_STATE_RUNNING, game_get_state(&g_game));
    TEST_ASSERT_EQUAL_UINT8(GAME_STARTING_LIVES, game_get_lives(&g_game));
    TEST_ASSERT_EQUAL_UINT32(PELLET_POINTS, game_get_score(&g_game));
}

void test_a_new_level_refills_the_maze_and_replaces_the_entities(void)
{
    game_start(&g_game);
    prv_eat_every_pellet_except(prv_make_cell(STEPPED_X, STEPPED_Y));

    prv_step_onto_the_pellet();

    TEST_ASSERT_FALSE(playfield_is_cleared(&g_game.playfield));
    TEST_ASSERT_EQUAL_INT16(PACMAN_START_X, prv_state().pacman.column);
    TEST_ASSERT_EQUAL_INT16(PACMAN_START_Y, prv_state().pacman.row);
}

void test_clearing_the_last_level_wins_the_run(void)
{
    game_start(&g_game);

    /* Jump to the last level rather than play the twenty before it. */
    prv_jump_to_level(DIFFICULTY_FINAL_LEVEL);
    prv_eat_every_pellet_except(prv_make_cell(STEPPED_X, STEPPED_Y));

    prv_step_onto_the_pellet();

    TEST_ASSERT_EQUAL(GAME_STATE_WON, game_get_state(&g_game));
    TEST_ASSERT_EQUAL_UINT8(DIFFICULTY_FINAL_LEVEL, game_get_level(&g_game));
}

void test_the_last_level_has_no_frightened_window(void)
{
    game_start(&g_game);
    prv_jump_to_level(DIFFICULTY_FINAL_LEVEL);
    g_game.playfield.pellets[STEPPED_Y][STEPPED_X] = PLAYFIELD_PELLET_POWER;

    prv_step_onto_the_pellet();

    /* Still worth the points, just no window to cash them in (§10.9). */
    TEST_ASSERT_EQUAL_UINT32(POWER_PELLET_POINTS, game_get_score(&g_game));
    TEST_ASSERT_FALSE(game_is_frightened_active(&g_game));
}

/* --- the difficulty curve (§10.9, FR-026) -------------------------------- */

void test_the_ghosts_never_get_slower_as_the_levels_rise(void)
{
    uint32_t previous_period_ms = 0U;

    /* The arcade's curve is a staircase, not a slope: levels 2, 3 and 4 share a speed, and
     * from 5 on nothing changes again. So what holds all the way up is "never slower",
     * which is the claim worth defending — a table typo that made a later level easier
     * would break it, and none of the flat stretches do.
     *
     * Measured by watching a ghost move rather than by reading the table it came from, so
     * a mistake in the wiring fails here too and not only a mistake in the numbers. */
    for (uint8_t level = DIFFICULTY_FIRST_LEVEL; level <= DIFFICULTY_FINAL_LEVEL; ++level)
    {
        uint32_t period_ms;

        game_start(&g_game);
        prv_jump_to_level(level);

        period_ms = prv_measure_ghost_period_ms(GHOST_BLINKY);

        TEST_ASSERT_NOT_EQUAL_UINT32(0U, period_ms);

        if (level > DIFFICULTY_FIRST_LEVEL)
        {
            TEST_ASSERT_LESS_OR_EQUAL_UINT32(previous_period_ms, period_ms);
        }

        previous_period_ms = period_ms;
    }
}

void test_an_ordinary_ghost_never_outruns_pacman_before_the_last_level(void)
{
    /* The fact the whole design rests on, and the one that surprises people: for twenty
     * levels no ghost is ever faster than Pacman in a straight line. They top out at 95 %
     * of full speed and he reaches 100 %. What closes on him is the *margin* — comfortable
     * on level 1, all but gone from level 5 — plus four of them at once. Cruise Elroy is
     * the one exception, and it is the test below. */
    for (uint8_t level = DIFFICULTY_FIRST_LEVEL; level < DIFFICULTY_FINAL_LEVEL; ++level)
    {
        char message[64];

        game_start(&g_game);
        prv_jump_to_level(level);

        (void)snprintf(message, sizeof(message), "level %u: a plain ghost outruns Pacman", level);
        TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(prv_pacman_period_ms(), prv_measure_ghost_period_ms(GHOST_PINKY),
                                                message);
    }
}

void test_the_last_level_finally_takes_pacmans_speed_away(void)
{
    /* Level 21 is the one place the arcade slows *him* down instead of speeding them up:
     * he drops back to 90 % while the ghosts stay at 95 %, so for the first and only time
     * every one of them is quicker than he is. It is the last thing the curve does, and it
     * is why 21 is a finish line worth having. */
    game_start(&g_game);
    prv_jump_to_level(DIFFICULTY_FINAL_LEVEL);

    TEST_ASSERT_LESS_THAN_UINT32(prv_pacman_period_ms(), prv_measure_ghost_period_ms(GHOST_PINKY));
}

void test_the_margin_over_the_ghosts_all_but_disappears_by_level_five(void)
{
    uint32_t level_1_margin_ms;
    uint32_t level_5_margin_ms;

    game_start(&g_game);
    prv_jump_to_level(LEVEL_1);
    level_1_margin_ms = prv_measure_ghost_period_ms(GHOST_PINKY) - prv_pacman_period_ms();

    game_start(&g_game);
    prv_jump_to_level(LEVEL_5);
    level_5_margin_ms = prv_measure_ghost_period_ms(GHOST_PINKY) - prv_pacman_period_ms();

    TEST_ASSERT_LESS_THAN_UINT32(level_1_margin_ms, level_5_margin_ms);
}

void test_only_cruise_elroy_ever_outruns_pacman(void)
{
    difficulty_t difficulty;

    difficulty_get(LEVEL_1, &difficulty);

    game_start(&g_game);
    prv_leave_this_many_pellets(difficulty.elroy2_pellets_left);

    /* Blinky at his second Elroy stage is the one thing in the maze quicker than Pacman —
     * which is why the last handful of pellets is where a level is actually lost. */
    TEST_ASSERT_LESS_THAN_UINT32(prv_pacman_period_ms(), prv_measure_ghost_period_ms(GHOST_BLINKY));
}

void test_pacman_is_slower_on_the_step_after_a_mouthful(void)
{
    difficulty_t difficulty;

    game_start(&g_game);
    difficulty_get(LEVEL_1, &difficulty);

    /* Nothing eaten yet. */
    TEST_ASSERT_EQUAL_UINT32(difficulty.pacman_period_ms, prv_pacman_period_ms());

    prv_step_onto_the_pellet();

    /* He just ate the pellet above his start, so the next step costs him more (§10.9) —
     * which is why a cleared corridor is an escape route and a full one is not. */
    TEST_ASSERT_EQUAL_UINT32(difficulty.pacman_eating_period_ms, prv_pacman_period_ms());
    TEST_ASSERT_GREATER_THAN_UINT32(difficulty.pacman_period_ms, difficulty.pacman_eating_period_ms);

    /* His own start cell never held a pellet, so stepping back onto it makes him quick
     * again rather than leaving him slow for the rest of the level. */
    game_set_direction(&g_game, BACK_DIRECTION);
    game_tick(&g_game, prv_pacman_period_ms());

    TEST_ASSERT_EQUAL_UINT32(difficulty.pacman_period_ms, prv_pacman_period_ms());
}

void test_the_frightened_window_shrinks_and_is_gone_by_the_last_level(void)
{
    uint32_t level_1_ms;
    uint32_t level_5_ms;

    game_start(&g_game);
    prv_jump_to_level(LEVEL_1);
    level_1_ms = prv_measure_frightened_ms();

    game_start(&g_game);
    prv_jump_to_level(LEVEL_5);
    level_5_ms = prv_measure_frightened_ms();

    TEST_ASSERT_NOT_EQUAL_UINT32(0U, level_1_ms);
    TEST_ASSERT_LESS_THAN_UINT32(level_1_ms, level_5_ms);

    /* It does not shrink monotonically — the arcade hands a long window back at 6, 10 and
     * 14, as a breather — so the claim that holds end to end is that by the last level
     * there is none at all. A power pellet is then 50 points and nothing more. */
    game_start(&g_game);
    prv_jump_to_level(DIFFICULTY_FINAL_LEVEL);
    prv_eat_a_power_pellet();

    TEST_ASSERT_FALSE(game_is_frightened_active(&g_game));
}

/* --- who is in the house, and when they come out (§10.4) ------------------ */

/* Eat exactly `in_count` pellets, by putting Pacman next to one and letting him take a
 * step onto it, over and over.
 *
 * A bigger liberty than the rest of this file takes, and deliberate: these tests are about
 * *when the other three leave the house*, and making Pacman walk there would decide them by
 * how the maze is shaped instead. Walking was tried first and does not work — the corridor
 * he starts in holds fourteen pellets and then he paces an empty row for ever.
 *
 * Approached from the bottom of the maze, which is the far side from the ghost house. */
static void prv_eat_pellets(uint16_t in_count)
{
    static const direction_e k_directions[] = {DIRECTION_NORTH, DIRECTION_SOUTH, DIRECTION_EAST, DIRECTION_WEST};

    for (uint16_t eaten = 0U; eaten < in_count; ++eaten)
    {
        bool has_eaten = false;

        for (int16_t y = PLAYFIELD_HEIGHT - 1; (y >= 0) && !has_eaten; --y)
        {
            for (int16_t x = 0; (x < PLAYFIELD_WIDTH) && !has_eaten; ++x)
            {
                const cell_t pellet = prv_make_cell(x, y);

                if (playfield_get_pellet(&g_game.playfield, pellet) == PLAYFIELD_PELLET_NONE)
                {
                    continue;
                }

                for (uint8_t index = 0U; (index < 4U) && !has_eaten; ++index)
                {
                    const cell_t approach = playfield_step(pellet, k_directions[index]);

                    if (!playfield_is_walkable(&g_game.playfield, approach)
                        || playfield_is_house(&g_game.playfield, approach))
                    {
                        continue;
                    }

                    pacman_reset(&g_game.pacman, approach);
                    game_set_direction(&g_game, playfield_get_opposite_direction(k_directions[index]));
                    game_tick(&g_game, prv_pacman_period_ms());

                    has_eaten = playfield_get_pellet(&g_game.playfield, pellet) == PLAYFIELD_PELLET_NONE;
                }
            }
        }

        TEST_ASSERT_TRUE_MESSAGE(has_eaten, "ran out of reachable pellets");
        TEST_ASSERT_EQUAL_MESSAGE(GAME_STATE_RUNNING, game_get_state(&g_game), "a ghost ended the run mid-count");
    }
}

/* Shut Blinky in the house so he cannot end the run while a dot counter is being watched.
 *
 * A liberty with the Model, and the same kind the file's header already owns up to: these
 * tests are about *when the other three come out*, and Blinky hunting Pacman across ninety
 * pellets would decide them by killing him instead. He is not in the release preference
 * order, so parking him there leaves him there. */
static void prv_shut_blinky_in(void)
{
    ghost_reset(&g_game.ghosts[GHOST_BLINKY], GHOST_BLINKY,
                playfield_get_ghost_start(&g_game.playfield, (uint8_t)GHOST_PINKY), true);
}

void test_only_blinky_and_pinky_are_out_when_a_run_begins(void)
{
    /* §10.4: Blinky starts outside altogether, Pinky's dot limit is zero so he leaves as the
     * level begins, and Inky and Clyde have to be eaten out. All four leaving at once was
     * what made level 1 feel like level 10. */
    game_start(&g_game);

    TEST_ASSERT_FALSE(ghost_is_in_house(&g_game.ghosts[GHOST_BLINKY]));
    TEST_ASSERT_FALSE(ghost_is_waiting_in_house(&g_game.ghosts[GHOST_PINKY]));
    TEST_ASSERT_TRUE(ghost_is_waiting_in_house(&g_game.ghosts[GHOST_INKY]));
    TEST_ASSERT_TRUE(ghost_is_waiting_in_house(&g_game.ghosts[GHOST_CLYDE]));
}

void test_inky_comes_out_at_his_dot_limit_and_clyde_later(void)
{
    difficulty_t difficulty;

    difficulty_get(LEVEL_1, &difficulty);
    game_start(&g_game);
    prv_shut_blinky_in();

    /* One short of Inky's limit: still shut in. Counting is what releases him, not time. */
    prv_eat_pellets((uint16_t)(difficulty.inky_dot_limit - 1U));
    TEST_ASSERT_TRUE(ghost_is_waiting_in_house(&g_game.ghosts[GHOST_INKY]));

    prv_eat_pellets(1U);
    TEST_ASSERT_FALSE(ghost_is_waiting_in_house(&g_game.ghosts[GHOST_INKY]));

    /* Clyde's counter only starts once Inky's has stopped, so his limit is a further
     * stretch rather than a total from the beginning of the level. */
    TEST_ASSERT_TRUE(ghost_is_waiting_in_house(&g_game.ghosts[GHOST_CLYDE]));
    prv_eat_pellets(difficulty.clyde_dot_limit);
    TEST_ASSERT_FALSE(ghost_is_waiting_in_house(&g_game.ghosts[GHOST_CLYDE]));
}

void test_standing_still_cannot_keep_the_ghosts_locked_up(void)
{
    /* Without the idle timer a player could stop eating and keep three of them indoors for
     * the whole level, which the arcade explicitly guards against. */
    difficulty_t difficulty;

    difficulty_get(LEVEL_1, &difficulty);
    game_start(&g_game);

    TEST_ASSERT_TRUE(ghost_is_waiting_in_house(&g_game.ghosts[GHOST_INKY]));

    /* Pacman is never given a direction, so not a single pellet is eaten. */
    prv_tick_for(difficulty.house_idle_limit_ms + 100U, 10U);

    TEST_ASSERT_FALSE(ghost_is_waiting_in_house(&g_game.ghosts[GHOST_INKY]));
}

void test_a_lost_life_switches_to_the_global_counter(void)
{
    /* §10.4: after a death the personal counters are set aside for a global one, so the
     * restart is paced the same however late in the level it happened. */
    game_start(&g_game);
    ghost_reset(&g_game.ghosts[GHOST_BLINKY], GHOST_BLINKY, prv_make_cell(STEPPED_X, STEPPED_Y), false);

    prv_step_onto_the_pellet();

    TEST_ASSERT_EQUAL_UINT8(GAME_STARTING_LIVES - 1U, game_get_lives(&g_game));
    TEST_ASSERT_TRUE(g_game.is_global_dot_counter_active);
    TEST_ASSERT_TRUE(ghost_is_waiting_in_house(&g_game.ghosts[GHOST_PINKY]));

    /* And Pinky now waits for seven dots rather than leaving at once. */
    prv_shut_blinky_in();
    prv_eat_pellets(7U);
    TEST_ASSERT_FALSE(ghost_is_waiting_in_house(&g_game.ghosts[GHOST_PINKY]));
}

/* --- Cruise Elroy (§10.9) ------------------------------------------------- */

void test_cruise_elroy_speeds_blinky_up_as_the_maze_empties(void)
{
    difficulty_t difficulty;
    uint32_t full_maze_ms;
    uint32_t stage_1_ms;
    uint32_t stage_2_ms;

    difficulty_get(LEVEL_1, &difficulty);

    game_start(&g_game);
    full_maze_ms = prv_measure_ghost_period_ms(GHOST_BLINKY);

    game_start(&g_game);
    prv_leave_this_many_pellets(difficulty.elroy1_pellets_left);
    stage_1_ms = prv_measure_ghost_period_ms(GHOST_BLINKY);

    game_start(&g_game);
    prv_leave_this_many_pellets(difficulty.elroy2_pellets_left);
    stage_2_ms = prv_measure_ghost_period_ms(GHOST_BLINKY);

    /* Two steps, not one ramp: he wakes at the first threshold and again at the second.
     * This is what makes the last twenty pellets of a level the dangerous ones. */
    TEST_ASSERT_LESS_THAN_UINT32(full_maze_ms, stage_1_ms);
    TEST_ASSERT_LESS_THAN_UINT32(stage_1_ms, stage_2_ms);
}

void test_only_blinky_gets_cruise_elroy(void)
{
    difficulty_t difficulty;
    uint32_t pinky_full_ms;
    uint32_t pinky_empty_ms;

    difficulty_get(LEVEL_1, &difficulty);

    game_start(&g_game);
    pinky_full_ms = prv_measure_ghost_period_ms(GHOST_PINKY);

    game_start(&g_game);
    prv_leave_this_many_pellets(difficulty.elroy2_pellets_left);
    pinky_empty_ms = prv_measure_ghost_period_ms(GHOST_PINKY);

    /* The other three keep their pace all the way to the last pellet. */
    TEST_ASSERT_EQUAL_UINT32(pinky_full_ms, pinky_empty_ms);
}

void test_cruise_elroy_keeps_blinky_hunting_through_scatter(void)
{
    difficulty_t difficulty;

    difficulty_get(LEVEL_1, &difficulty);

    /* The plan opens with a scatter phase, so on a full maze everyone goes home. */
    game_start(&g_game);
    game_tick(&g_game, 10U);

    TEST_ASSERT_EQUAL(GHOST_MODE_SCATTER, g_game.ghosts[GHOST_BLINKY].mode);
    TEST_ASSERT_EQUAL(GHOST_MODE_SCATTER, g_game.ghosts[GHOST_PINKY].mode);

    /* Once Elroy is awake he does not: the breather the others take is exactly when he
     * keeps coming, which is the whole character of the end of a level. */
    game_start(&g_game);
    prv_leave_this_many_pellets(difficulty.elroy1_pellets_left);
    game_tick(&g_game, 10U);

    TEST_ASSERT_EQUAL(GHOST_MODE_CHASE, g_game.ghosts[GHOST_BLINKY].mode);
    TEST_ASSERT_EQUAL(GHOST_MODE_SCATTER, g_game.ghosts[GHOST_PINKY].mode);
}

/* --- the tunnel (§10.9) --------------------------------------------------- */

void test_a_ghost_crawls_through_the_tunnel(void)
{
    const cell_t tunnel_cell = prv_make_cell(2, 14);
    uint32_t open_maze_ms;
    uint32_t tunnel_ms;

    game_start(&g_game);
    open_maze_ms = prv_measure_ghost_period_ms(GHOST_PINKY);

    game_start(&g_game);
    TEST_ASSERT_TRUE(playfield_is_tunnel(&g_game.playfield, tunnel_cell));

    ghost_reset(&g_game.ghosts[GHOST_PINKY], GHOST_PINKY, tunnel_cell, false);
    tunnel_ms = prv_measure_ghost_period_ms(GHOST_PINKY);

    /* The one stretch where Pacman can reliably shake one off. */
    TEST_ASSERT_GREATER_THAN_UINT32(open_maze_ms, tunnel_ms);
}

/* --- the closing warning (§10.9) ------------------------------------------ */

void test_the_frightened_ghosts_flash_before_the_window_closes(void)
{
    difficulty_t difficulty;
    uint32_t warning_ms;
    bool has_flashed = false;

    difficulty_get(LEVEL_1, &difficulty);
    warning_ms = (uint32_t)difficulty.frightened_flash_count * 2U * GAME_FRIGHTENED_FLASH_HALF_PERIOD_MS;

    game_start(&g_game);
    prv_eat_a_power_pellet();

    /* Early in the window there is no warning yet — a flash that ran the whole time would
     * tell the player nothing. */
    TEST_ASSERT_TRUE(game_is_frightened_active(&g_game));
    TEST_ASSERT_FALSE(prv_state().are_frightened_ghosts_flashing);

    prv_tick_for(difficulty.frightened_duration_ms - warning_ms, 10U);

    /* Inside the warning it alternates, so the test watches for a light half rather than
     * assuming which one it lands on. */
    while (game_is_frightened_active(&g_game) && !has_flashed)
    {
        has_flashed = prv_state().are_frightened_ghosts_flashing;
        game_tick(&g_game, 10U);
    }

    TEST_ASSERT_TRUE(has_flashed);

    /* And it stops with the window: nothing edible, nothing flashing. */
    prv_tick_for(warning_ms, 10U);

    TEST_ASSERT_FALSE(game_is_frightened_active(&g_game));
    TEST_ASSERT_FALSE(prv_state().are_frightened_ghosts_flashing);
}

/* --- the state handed to the view (DEC-016) ------------------------------ */

void test_the_state_reports_the_run(void)
{
    msg_game_state_t state;

    game_start(&g_game);
    prv_step_onto_the_pellet();
    state = prv_state();

    TEST_ASSERT_EQUAL_UINT8(LEVEL_1, state.level);
    TEST_ASSERT_EQUAL_UINT8(GAME_STARTING_LIVES, state.lives);
    TEST_ASSERT_EQUAL_UINT32(PELLET_POINTS, state.score);
}

void test_the_state_is_a_copy_the_caller_may_scribble_on(void)
{
    msg_game_state_t first;
    msg_game_state_t second;

    game_start(&g_game);
    game_get_state_message(&g_game, &first);

    memset(&first, 0xFF, sizeof(first));
    game_get_state_message(&g_game, &second);

    /* The whole point of withdrawing R-007: what the caller holds is theirs, and the game
     * is not reachable through it. */
    TEST_ASSERT_EQUAL_UINT8(PACMAN_START_X, second.pacman.column);
}

/* --- interpolation (10 §10.1) -------------------------------------------- */

void test_a_blocked_step_leaves_the_actor_standing_on_its_cell(void)
{
    game_start(&g_game);

    /* Row 1 of the maze is the top corridor, open from column 1 to the wall at column 0, so
     * walking west from column 2 reaches column 1 and then stops — still *facing* west,
     * which is what §10.1 says a blocked entity does. Its movement timer keeps running, and
     * a running fraction would then have the view slide him in from column 2 again, over
     * and over, on the spot. "Arrived" is what says he is simply standing there. */
    pacman_reset(&g_game.pacman, prv_make_cell(2, 1));
    game_set_direction(&g_game, DIRECTION_WEST);
    game_tick(&g_game, prv_pacman_period_ms());

    TEST_ASSERT_EQUAL_UINT8(1U, prv_state().pacman.column);
    TEST_ASSERT_EQUAL(DIRECTION_WEST, prv_state().pacman.direction);

    game_tick(&g_game, prv_pacman_period_ms());
    game_tick(&g_game, prv_pacman_period_ms() / 2U);

    TEST_ASSERT_EQUAL(DIRECTION_WEST, prv_state().pacman.direction);
    TEST_ASSERT_EQUAL_UINT8(MSG_CELL_PROGRESS_ARRIVED, prv_state().pacman.progress);
}

void test_a_step_is_a_full_sweep_from_nothing_to_arrived(void)
{
    const uint32_t period_ms = prv_pacman_period_ms();
    cell_progress_t just_after_the_step;
    cell_progress_t half_way;
    cell_progress_t just_before_the_next;

    game_start(&g_game);
    game_set_direction(&g_game, STEP_DIRECTION);

    /* The step lands him on the next cell, and none of it has been run off yet: the view
     * still draws him on the cell he left. */
    game_tick(&g_game, period_ms);
    just_after_the_step = prv_state().pacman.progress;

    game_tick(&g_game, period_ms / 2U);
    half_way = prv_state().pacman.progress;

    game_tick(&g_game, (period_ms / 2U) - 1U);
    just_before_the_next = prv_state().pacman.progress;

    /* Nought to arrived across one period, and no further: the sprite may reach the cell
     * the model is on, and must never be carried past it into one the model has not
     * chosen yet. */
    TEST_ASSERT_EQUAL_UINT8(0U, just_after_the_step);
    TEST_ASSERT_GREATER_THAN_UINT8(just_after_the_step, half_way);
    TEST_ASSERT_GREATER_THAN_UINT8(half_way, just_before_the_next);
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(MSG_CELL_PROGRESS_ARRIVED, just_before_the_next);
}

/* --- preconditions ------------------------------------------------------- */

void test_a_null_game_asserts(void)
{
    ASSERT_PROBE_EXPECT(game_init(NULL), "inout_game != NULL");
    ASSERT_PROBE_EXPECT(game_start(NULL), "inout_game != NULL");
    ASSERT_PROBE_EXPECT(game_tick(NULL, 1U), "inout_game != NULL");
    ASSERT_PROBE_EXPECT(game_set_direction(NULL, DIRECTION_NORTH), "inout_game != NULL");
    ASSERT_PROBE_EXPECT(game_get_state_message(NULL, &g_probe_state), "in_game != NULL");
    ASSERT_PROBE_EXPECT(game_get_state_message(&g_game, NULL), "out_state != NULL");
    ASSERT_PROBE_EXPECT((void)game_get_state(NULL), "in_game != NULL");
    ASSERT_PROBE_EXPECT((void)game_get_score(NULL), "in_game != NULL");
    ASSERT_PROBE_EXPECT((void)game_get_lives(NULL), "in_game != NULL");
    ASSERT_PROBE_EXPECT((void)game_get_level(NULL), "in_game != NULL");
    ASSERT_PROBE_EXPECT((void)game_is_frightened_active(NULL), "in_game != NULL");
}
