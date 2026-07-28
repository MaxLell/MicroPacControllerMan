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
#include <string.h>

/* Ceedling links from this file's includes only, not transitively — and the game sits on
 * the entire Model plus the bus. */
#include "active_object.h"
#include "agent.h"
#include "assert_probe.h"
#include "circular_buffer.h"
#include "custom_assert.h"
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

#define PELLET_POINTS             (10U)
#define POWER_PELLET_POINTS       (50U)

/* Level 1 of §10.2. Pacman starts at the bottom-centre with a pellet directly above him,
 * which is the one step every movement test here takes. */
#define PACMAN_START_X            (5)
#define PACMAN_START_Y            (7)
#define ABOVE_START_X             (5)
#define ABOVE_START_Y             (6)

#define A_TICK_SHORT_OF_A_MOVE_MS (GAME_PACMAN_MOVE_PERIOD_MS - 1U)

#define LEVEL_1                   (1U)
#define LEVEL_2                   (2U)
#define LEVEL_5                   (5U)

/* Level 1's frightened window (§10.9), transcribed so a change to the table shows up here
 * as a failing test rather than as a silently different game. */
#define LEVEL_1_FRIGHTENED_MS     (6000U)

static game_t g_game;

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

/* Move Pacman one cell north, onto whatever is on the cell above him. */
static void prv_step_north(void)
{
    game_set_direction(&g_game, DIRECTION_NORTH);
    game_tick(&g_game, GAME_PACMAN_MOVE_PERIOD_MS);
}

/* Drop a run straight onto a level, rather than play the four before it. Everyone is
 * replaced the way loading a level does it; the scatter/chase plan restarts from zero,
 * which is enough for the timings these set up. */
static void prv_jump_to_level(uint8_t in_level)
{
    g_game.level = in_level;

    playfield_load_level(&g_game.playfield, in_level);
    pacman_reset(&g_game.pacman, playfield_get_pacman_start(&g_game.playfield));

    for (uint8_t index = 0U; index < GHOST_COUNT; ++index)
    {
        ghost_reset(&g_game.ghosts[index], (ghost_personality_e)index,
                    playfield_get_pen_cell(&g_game.playfield, (uint8_t)(index % PLAYFIELD_PEN_CELL_COUNT)));
    }

    g_game.pacman_move_elapsed_ms = 0U;
    g_game.ghost_move_elapsed_ms = 0U;
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
    game_tick(&g_game, GAME_PACMAN_MOVE_PERIOD_MS);

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

/* How long a ghost takes to make its first step on the level currently loaded — its move
 * period, observed rather than read out of the table it comes from. */
static uint32_t prv_measure_ghost_period_ms(void)
{
    const cell_t start_cell = ghost_get_cell(&g_game.ghosts[GHOST_BLINKY]);
    const uint32_t step_ms = 10U;
    const uint32_t limit_ms = 1000U;
    uint32_t elapsed_ms = 0U;

    while (elapsed_ms < limit_ms)
    {
        game_tick(&g_game, step_ms);
        elapsed_ms += step_ms;

        if (!playfield_are_cells_equal(start_cell, ghost_get_cell(&g_game.ghosts[GHOST_BLINKY])))
        {
            return elapsed_ms;
        }
    }

    return 0U;
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
    const uint32_t version = game_get_snapshot(&g_game)->version;

    prv_tick_for(10U * GAME_PACMAN_MOVE_PERIOD_MS, GAME_PACMAN_MOVE_PERIOD_MS);

    TEST_ASSERT_EQUAL_UINT32(version, game_get_snapshot(&g_game)->version);
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
    const game_snapshot_t* snapshot;

    game_start(&g_game);
    snapshot = game_get_snapshot(&g_game);

    TEST_ASSERT_EQUAL_INT16(PACMAN_START_X, snapshot->pacman_cell.x);
    TEST_ASSERT_EQUAL_INT16(PACMAN_START_Y, snapshot->pacman_cell.y);
}

void test_a_second_run_starts_from_scratch(void)
{
    game_start(&g_game);
    prv_step_north();

    game_start(&g_game);

    /* The score, the lives and the maze all come back — a run must not inherit the last
     * one's pellets or points. */
    TEST_ASSERT_EQUAL_UINT32(0U, game_get_score(&g_game));
    TEST_ASSERT_EQUAL_UINT8(GAME_STARTING_LIVES, game_get_lives(&g_game));
    TEST_ASSERT_EQUAL(PLAYFIELD_PELLET_NORMAL, game_get_snapshot(&g_game)->pellets[ABOVE_START_Y][ABOVE_START_X]);
}

/* --- movement and timing (§10.1) ----------------------------------------- */

void test_pacman_stands_still_until_a_direction_is_asked_for(void)
{
    game_start(&g_game);

    prv_tick_for(10U * GAME_PACMAN_MOVE_PERIOD_MS, GAME_PACMAN_MOVE_PERIOD_MS);

    TEST_ASSERT_EQUAL_INT16(PACMAN_START_X, game_get_snapshot(&g_game)->pacman_cell.x);
    TEST_ASSERT_EQUAL_INT16(PACMAN_START_Y, game_get_snapshot(&g_game)->pacman_cell.y);
}

void test_pacman_does_not_move_before_his_period_has_elapsed(void)
{
    game_start(&g_game);

    game_set_direction(&g_game, DIRECTION_NORTH);
    game_tick(&g_game, A_TICK_SHORT_OF_A_MOVE_MS);

    TEST_ASSERT_EQUAL_INT16(PACMAN_START_Y, game_get_snapshot(&g_game)->pacman_cell.y);
}

void test_pacman_moves_one_cell_per_period(void)
{
    game_start(&g_game);

    prv_step_north();

    TEST_ASSERT_EQUAL_INT16(ABOVE_START_X, game_get_snapshot(&g_game)->pacman_cell.x);
    TEST_ASSERT_EQUAL_INT16(ABOVE_START_Y, game_get_snapshot(&g_game)->pacman_cell.y);
    TEST_ASSERT_EQUAL(DIRECTION_NORTH, game_get_snapshot(&g_game)->pacman_direction);
}

void test_leftover_time_is_carried_into_the_next_move(void)
{
    game_start(&g_game);
    game_set_direction(&g_game, DIRECTION_NORTH);

    /* Two thirds of a period at a time: the third slice completes the second period, so
     * time must accumulate rather than be discarded at each tick. */
    prv_tick_for(2U * GAME_PACMAN_MOVE_PERIOD_MS, (GAME_PACMAN_MOVE_PERIOD_MS * 2U) / 3U);

    TEST_ASSERT_EQUAL_INT16(ABOVE_START_Y - 1, game_get_snapshot(&g_game)->pacman_cell.y);
}

void test_a_direction_is_ignored_unless_a_run_is_in_progress(void)
{
    /* Still idle: the input must not be remembered and then acted on at the next start. */
    game_set_direction(&g_game, DIRECTION_NORTH);

    game_start(&g_game);
    game_tick(&g_game, GAME_PACMAN_MOVE_PERIOD_MS);

    TEST_ASSERT_EQUAL_INT16(PACMAN_START_Y, game_get_snapshot(&g_game)->pacman_cell.y);
}

/* --- pellets and scoring (§10.6) ----------------------------------------- */

void test_eating_a_pellet_scores_and_clears_the_cell(void)
{
    game_start(&g_game);

    prv_step_north();

    TEST_ASSERT_EQUAL_UINT32(PELLET_POINTS, game_get_score(&g_game));
    TEST_ASSERT_EQUAL(PLAYFIELD_PELLET_NONE, game_get_snapshot(&g_game)->pellets[ABOVE_START_Y][ABOVE_START_X]);
}

void test_a_cell_only_pays_once(void)
{
    game_start(&g_game);

    prv_step_north();
    game_set_direction(&g_game, DIRECTION_SOUTH);
    game_tick(&g_game, GAME_PACMAN_MOVE_PERIOD_MS);
    game_set_direction(&g_game, DIRECTION_NORTH);
    game_tick(&g_game, GAME_PACMAN_MOVE_PERIOD_MS);

    /* Back on the cell he cleared, having turned round on his own start cell, which never
     * had a pellet on it — so the one bite is still the only one that paid. */
    TEST_ASSERT_EQUAL_UINT32(PELLET_POINTS, game_get_score(&g_game));
}

void test_a_power_pellet_scores_fifty_and_frightens_the_ghosts(void)
{
    game_start(&g_game);
    g_game.playfield.pellets[ABOVE_START_Y][ABOVE_START_X] = PLAYFIELD_PELLET_POWER;

    prv_step_north();

    TEST_ASSERT_EQUAL_UINT32(POWER_PELLET_POINTS, game_get_score(&g_game));
    TEST_ASSERT_TRUE(game_is_frightened_active(&g_game));

    for (uint8_t index = 0U; index < GHOST_COUNT; ++index)
    {
        TEST_ASSERT_TRUE(game_get_snapshot(&g_game)->ghost_is_frightened[index]);
    }
}

void test_the_frightened_window_runs_out(void)
{
    game_start(&g_game);
    g_game.playfield.pellets[ABOVE_START_Y][ABOVE_START_X] = PLAYFIELD_PELLET_POWER;
    prv_step_north();

    /* Park Pacman where he is, so the window is the only thing this measures. */
    pacman_reset(&g_game.pacman, prv_make_cell(ABOVE_START_X, ABOVE_START_Y));

    prv_tick_for(LEVEL_1_FRIGHTENED_MS - GAME_PACMAN_MOVE_PERIOD_MS, 10U);
    TEST_ASSERT_TRUE(game_is_frightened_active(&g_game));

    prv_tick_for(GAME_PACMAN_MOVE_PERIOD_MS, 10U);
    TEST_ASSERT_FALSE(game_is_frightened_active(&g_game));
    TEST_ASSERT_FALSE(game_get_snapshot(&g_game)->ghost_is_frightened[GHOST_BLINKY]);
}

/* --- meeting a ghost (§10.7) --------------------------------------------- */

void test_walking_into_a_ghost_costs_a_life_and_resets_the_positions(void)
{
    game_start(&g_game);
    ghost_reset(&g_game.ghosts[GHOST_BLINKY], GHOST_BLINKY, prv_make_cell(ABOVE_START_X, ABOVE_START_Y));

    prv_step_north();

    TEST_ASSERT_EQUAL(GAME_STATE_RUNNING, game_get_state(&g_game));
    TEST_ASSERT_EQUAL_UINT8(GAME_STARTING_LIVES - 1U, game_get_lives(&g_game));
    TEST_ASSERT_EQUAL_INT16(PACMAN_START_Y, game_get_snapshot(&g_game)->pacman_cell.y);
}

void test_the_eaten_pellets_survive_a_lost_life(void)
{
    game_start(&g_game);
    prv_step_north();
    ghost_reset(&g_game.ghosts[GHOST_BLINKY], GHOST_BLINKY, prv_make_cell(PACMAN_START_X, PACMAN_START_Y));

    game_set_direction(&g_game, DIRECTION_SOUTH);
    game_tick(&g_game, GAME_PACMAN_MOVE_PERIOD_MS);

    TEST_ASSERT_EQUAL_UINT8(GAME_STARTING_LIVES - 1U, game_get_lives(&g_game));
    TEST_ASSERT_EQUAL(PLAYFIELD_PELLET_NONE, game_get_snapshot(&g_game)->pellets[ABOVE_START_Y][ABOVE_START_X]);
    TEST_ASSERT_EQUAL_UINT32(PELLET_POINTS, game_get_score(&g_game));
}

void test_the_last_life_ends_the_run(void)
{
    game_start(&g_game);
    g_game.lives = 1U;
    ghost_reset(&g_game.ghosts[GHOST_BLINKY], GHOST_BLINKY, prv_make_cell(ABOVE_START_X, ABOVE_START_Y));

    prv_step_north();

    TEST_ASSERT_EQUAL(GAME_STATE_OVER, game_get_state(&g_game));
    TEST_ASSERT_EQUAL_UINT8(0U, game_get_lives(&g_game));
}

void test_a_finished_run_ignores_further_time(void)
{
    game_start(&g_game);
    g_game.lives = 1U;
    ghost_reset(&g_game.ghosts[GHOST_BLINKY], GHOST_BLINKY, prv_make_cell(ABOVE_START_X, ABOVE_START_Y));
    prv_step_north();

    const uint32_t version = game_get_snapshot(&g_game)->version;

    prv_tick_for(10U * GAME_PACMAN_MOVE_PERIOD_MS, GAME_PACMAN_MOVE_PERIOD_MS);

    TEST_ASSERT_EQUAL_UINT32(version, game_get_snapshot(&g_game)->version);
}

void test_eating_a_frightened_ghost_scores_and_sends_it_back_to_the_pen(void)
{
    game_start(&g_game);
    g_game.playfield.pellets[ABOVE_START_Y][ABOVE_START_X] = PLAYFIELD_PELLET_POWER;
    prv_step_north();

    /* Frightened now, so the next meeting goes the other way (§10.5). */
    ghost_reset(&g_game.ghosts[GHOST_BLINKY], GHOST_BLINKY, prv_make_cell(PACMAN_START_X, PACMAN_START_Y));
    ghost_set_mode(&g_game.ghosts[GHOST_BLINKY], GHOST_MODE_FRIGHTENED);

    game_set_direction(&g_game, DIRECTION_SOUTH);
    game_tick(&g_game, GAME_PACMAN_MOVE_PERIOD_MS);

    TEST_ASSERT_EQUAL_UINT8(GAME_STARTING_LIVES, game_get_lives(&g_game));
    TEST_ASSERT_EQUAL_UINT32(POWER_PELLET_POINTS + 200U, game_get_score(&g_game));
}

void test_passing_through_a_ghost_still_counts_as_meeting_it(void)
{
    game_start(&g_game);

    /* The ghost is where Pacman is and Pacman is heading for where the ghost will be —
     * they swap cells in one step and would otherwise slip past each other. */
    ghost_reset(&g_game.ghosts[GHOST_BLINKY], GHOST_BLINKY, prv_make_cell(ABOVE_START_X, ABOVE_START_Y));
    ghost_set_mode(&g_game.ghosts[GHOST_BLINKY], GHOST_MODE_CHASE);

    /* Only the ghosts move in this slice, straight down onto Pacman's cell as he leaves
     * it. Blinky targets Pacman directly, so south is the step he takes. */
    prv_step_north();

    TEST_ASSERT_EQUAL_UINT8(GAME_STARTING_LIVES - 1U, game_get_lives(&g_game));
}

/* --- levels (§10.9, FR-025/027) ------------------------------------------ */

void test_clearing_a_level_loads_the_next_one_and_keeps_the_score(void)
{
    game_start(&g_game);
    prv_eat_every_pellet_except(prv_make_cell(ABOVE_START_X, ABOVE_START_Y));

    prv_step_north();

    TEST_ASSERT_EQUAL_UINT8(LEVEL_2, game_get_level(&g_game));
    TEST_ASSERT_EQUAL(GAME_STATE_RUNNING, game_get_state(&g_game));
    TEST_ASSERT_EQUAL_UINT8(GAME_STARTING_LIVES, game_get_lives(&g_game));
    TEST_ASSERT_EQUAL_UINT32(PELLET_POINTS, game_get_score(&g_game));
}

void test_a_new_level_refills_the_maze_and_replaces_the_entities(void)
{
    game_start(&g_game);
    prv_eat_every_pellet_except(prv_make_cell(ABOVE_START_X, ABOVE_START_Y));

    prv_step_north();

    TEST_ASSERT_FALSE(playfield_is_cleared(&g_game.playfield));
    TEST_ASSERT_EQUAL_INT16(PACMAN_START_X, game_get_snapshot(&g_game)->pacman_cell.x);
    TEST_ASSERT_EQUAL_INT16(PACMAN_START_Y, game_get_snapshot(&g_game)->pacman_cell.y);
}

void test_clearing_the_last_level_wins_the_run(void)
{
    game_start(&g_game);

    /* Jump to the final maze rather than play four of them. */
    g_game.level = LEVEL_5;
    playfield_load_level(&g_game.playfield, LEVEL_5);
    pacman_reset(&g_game.pacman, playfield_get_pacman_start(&g_game.playfield));
    prv_eat_every_pellet_except(prv_make_cell(ABOVE_START_X, ABOVE_START_Y));

    prv_step_north();

    TEST_ASSERT_EQUAL(GAME_STATE_WON, game_get_state(&g_game));
    TEST_ASSERT_EQUAL_UINT8(LEVEL_5, game_get_level(&g_game));
}

void test_the_last_level_has_no_frightened_window(void)
{
    game_start(&g_game);
    g_game.level = LEVEL_5;
    playfield_load_level(&g_game.playfield, LEVEL_5);
    pacman_reset(&g_game.pacman, playfield_get_pacman_start(&g_game.playfield));
    g_game.playfield.pellets[ABOVE_START_Y][ABOVE_START_X] = PLAYFIELD_PELLET_POWER;

    prv_step_north();

    /* Still worth the points, just no window to cash them in (§10.9). */
    TEST_ASSERT_EQUAL_UINT32(POWER_PELLET_POINTS, game_get_score(&g_game));
    TEST_ASSERT_FALSE(game_is_frightened_active(&g_game));
}

/* --- the difficulty curve (§10.9, FR-026) -------------------------------- */

void test_the_ghosts_get_faster_every_level(void)
{
    uint32_t previous_period_ms = 0U;

    for (uint8_t level = PLAYFIELD_FIRST_LEVEL; level <= PLAYFIELD_LEVEL_COUNT; ++level)
    {
        uint32_t period_ms;

        game_start(&g_game);
        prv_jump_to_level(level);

        period_ms = prv_measure_ghost_period_ms();

        TEST_ASSERT_NOT_EQUAL_UINT32(0U, period_ms);

        if (level > PLAYFIELD_FIRST_LEVEL)
        {
            /* Measured by watching a ghost move, not read out of the table it comes from —
             * so a typo there fails here rather than only showing up as a level that plays
             * easier than the one before it. */
            TEST_ASSERT_LESS_THAN_UINT32(previous_period_ms, period_ms);
        }

        previous_period_ms = period_ms;
    }
}

void test_pacmans_speed_is_the_one_thing_that_does_not_change(void)
{
    /* §10.1: the levels get harder by speeding the ghosts up, never by slowing him down. */
    for (uint8_t level = PLAYFIELD_FIRST_LEVEL; level <= PLAYFIELD_LEVEL_COUNT; ++level)
    {
        cell_t start_cell;
        direction_e direction;

        game_start(&g_game);
        prv_jump_to_level(level);

        start_cell = pacman_get_cell(&g_game.pacman);
        direction = prv_find_a_way_out_with_a_pellet();
        game_set_direction(&g_game, direction);

        game_tick(&g_game, A_TICK_SHORT_OF_A_MOVE_MS);
        TEST_ASSERT_TRUE(playfield_are_cells_equal(start_cell, pacman_get_cell(&g_game.pacman)));

        game_tick(&g_game, 1U);
        TEST_ASSERT_FALSE(playfield_are_cells_equal(start_cell, pacman_get_cell(&g_game.pacman)));
    }
}

void test_the_frightened_window_gets_shorter_every_level(void)
{
    uint32_t previous_duration_ms = 0U;

    for (uint8_t level = PLAYFIELD_FIRST_LEVEL; level < PLAYFIELD_LEVEL_COUNT; ++level)
    {
        uint32_t duration_ms;

        game_start(&g_game);
        prv_jump_to_level(level);

        duration_ms = prv_measure_frightened_ms();

        TEST_ASSERT_NOT_EQUAL_UINT32(0U, duration_ms);

        if (level > PLAYFIELD_FIRST_LEVEL)
        {
            TEST_ASSERT_LESS_THAN_UINT32(previous_duration_ms, duration_ms);
        }

        previous_duration_ms = duration_ms;
    }

    /* The last level has no window at all, which is the end of the same curve rather than
     * an exception to it. */
    game_start(&g_game);
    prv_jump_to_level(PLAYFIELD_LEVEL_COUNT);
    prv_eat_a_power_pellet();

    TEST_ASSERT_FALSE(game_is_frightened_active(&g_game));
}

/* --- the published frame (R-007) ----------------------------------------- */

void test_each_tick_publishes_a_new_frame(void)
{
    const uint32_t version = game_get_snapshot(&g_game)->version;

    game_start(&g_game);
    game_tick(&g_game, GAME_PACMAN_MOVE_PERIOD_MS);

    TEST_ASSERT_GREATER_THAN_UINT32(version, game_get_snapshot(&g_game)->version);
}

void test_the_previous_frame_is_left_intact_while_the_next_one_is_built(void)
{
    const game_snapshot_t* first;
    uint32_t first_version;

    game_start(&g_game);
    first = game_get_snapshot(&g_game);
    first_version = first->version;

    game_tick(&g_game, GAME_PACMAN_MOVE_PERIOD_MS);

    /* Double-buffered, so the frame a renderer is holding is not the one being written. */
    TEST_ASSERT_NOT_EQUAL(first, game_get_snapshot(&g_game));
    TEST_ASSERT_EQUAL_UINT32(first_version, first->version);
}

void test_the_frame_reports_the_run(void)
{
    const game_snapshot_t* snapshot;

    game_start(&g_game);
    prv_step_north();
    snapshot = game_get_snapshot(&g_game);

    TEST_ASSERT_EQUAL_UINT8(LEVEL_1, snapshot->level);
    TEST_ASSERT_EQUAL_UINT8(GAME_STARTING_LIVES, snapshot->lives);
    TEST_ASSERT_EQUAL_UINT32(PELLET_POINTS, snapshot->score);
    TEST_ASSERT_EQUAL(GAME_STATE_RUNNING, snapshot->state);
}

/* --- preconditions ------------------------------------------------------- */

void test_a_null_game_asserts(void)
{
    ASSERT_PROBE_EXPECT(game_init(NULL), "inout_game != NULL");
    ASSERT_PROBE_EXPECT(game_start(NULL), "inout_game != NULL");
    ASSERT_PROBE_EXPECT(game_tick(NULL, 1U), "inout_game != NULL");
    ASSERT_PROBE_EXPECT(game_set_direction(NULL, DIRECTION_NORTH), "inout_game != NULL");
    ASSERT_PROBE_EXPECT((void)game_get_snapshot(NULL), "in_game != NULL");
    ASSERT_PROBE_EXPECT((void)game_get_state(NULL), "in_game != NULL");
    ASSERT_PROBE_EXPECT((void)game_get_score(NULL), "in_game != NULL");
    ASSERT_PROBE_EXPECT((void)game_get_lives(NULL), "in_game != NULL");
    ASSERT_PROBE_EXPECT((void)game_get_level(NULL), "in_game != NULL");
    ASSERT_PROBE_EXPECT((void)game_is_frightened_active(NULL), "in_game != NULL");
}
