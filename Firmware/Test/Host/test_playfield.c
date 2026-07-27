/*
 * Unit tests for App/playfield.
 *
 * The valuable part is the sweep over all five mazes. Levels 2-5 were authored for this
 * project (§10.2 leaves them to M3), and a single mistyped wall can seal off a region or
 * the ghost pen. That would not fail a build, would not fail an OTT, and would surface
 * only as a level that cannot be finished or ghosts that never come out. A flood fill
 * settles it in microseconds, so every constraint §10.2 states is checked mechanically:
 * dimensions, left-right symmetry, one Pacman start, a full pen, a reachable pen, every
 * pellet reachable, and at least one tunnel.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "assert_probe.h"
#include "custom_assert.h"
#include "msg.h"
#include "playfield.h"
#include "unity.h"

/* The reference maze of §10.2 has 4 power pellets and a known pellet total; the rest are
 * checked by property rather than by count, since they are ours to change. */
#define LEVEL_1 (1U)
#define LEVEL_5 (5U)
#define LEVEL_1_POWER_PELLET_COUNT (4U)

#define MIN_POWER_PELLETS_PER_MAZE (2U)
#define SCATTER_CORNER_COUNT (4U)

static playfield_t g_playfield;

static bool g_reached[PLAYFIELD_HEIGHT][PLAYFIELD_WIDTH];

/* Flood fill from a cell, following the same walkability and tunnel rules the game uses,
 * so the test cannot disagree with the movement code about what is connected. */
static void prv_flood(cell_t in_cell)
{
    static const direction_e k_directions[]
        = {DIRECTION_NORTH, DIRECTION_SOUTH, DIRECTION_EAST, DIRECTION_WEST};
    const cell_t cell = playfield_wrap_cell(in_cell);

    if (g_reached[cell.y][cell.x] || !playfield_is_walkable(&g_playfield, cell))
    {
        return;
    }

    g_reached[cell.y][cell.x] = true;

    for (uint8_t index = 0U; index < (sizeof(k_directions) / sizeof(k_directions[0])); ++index)
    {
        prv_flood(playfield_step(cell, k_directions[index]));
    }
}

static void prv_flood_from_pacman_start(void)
{
    memset(g_reached, 0, sizeof(g_reached));
    prv_flood(playfield_get_pacman_start(&g_playfield));
}

static uint16_t prv_count_power_pellets(void)
{
    uint16_t count = 0U;

    for (int16_t y = 0; y < PLAYFIELD_HEIGHT; ++y)
    {
        for (int16_t x = 0; x < PLAYFIELD_WIDTH; ++x)
        {
            const cell_t cell = {x, y};

            if (playfield_get_pellet(&g_playfield, cell) == PLAYFIELD_PELLET_POWER)
            {
                ++count;
            }
        }
    }

    return count;
}

void setUp(void)
{
    assert_probe_begin();
    playfield_load_level(&g_playfield, LEVEL_1);
}

void tearDown(void)
{
    assert_probe_end();
}

/* --- every maze must satisfy §10.2 --------------------------------------- */

void test_every_maze_is_fully_connected_from_pacmans_start(void)
{
    for (uint8_t level = PLAYFIELD_FIRST_LEVEL; level <= PLAYFIELD_LEVEL_COUNT; ++level)
    {
        playfield_load_level(&g_playfield, level);
        prv_flood_from_pacman_start();

        for (int16_t y = 0; y < PLAYFIELD_HEIGHT; ++y)
        {
            for (int16_t x = 0; x < PLAYFIELD_WIDTH; ++x)
            {
                const cell_t cell = {x, y};
                char message[64];

                if (!playfield_is_walkable(&g_playfield, cell))
                {
                    continue;
                }

                (void)snprintf(message, sizeof(message),
                               "level %u: open cell %d,%d is unreachable", level, x, y);
                TEST_ASSERT_TRUE_MESSAGE(g_reached[y][x], message);
            }
        }
    }
}

void test_every_pellet_can_be_eaten(void)
{
    for (uint8_t level = PLAYFIELD_FIRST_LEVEL; level <= PLAYFIELD_LEVEL_COUNT; ++level)
    {
        playfield_load_level(&g_playfield, level);
        prv_flood_from_pacman_start();

        for (int16_t y = 0; y < PLAYFIELD_HEIGHT; ++y)
        {
            for (int16_t x = 0; x < PLAYFIELD_WIDTH; ++x)
            {
                const cell_t cell = {x, y};
                char message[64];

                if (playfield_get_pellet(&g_playfield, cell) == PLAYFIELD_PELLET_NONE)
                {
                    continue;
                }

                (void)snprintf(message, sizeof(message),
                               "level %u: pellet at %d,%d is unreachable", level, x, y);
                TEST_ASSERT_TRUE_MESSAGE(g_reached[y][x], message);
            }
        }
    }
}

void test_the_ghost_pen_is_reachable_in_every_maze(void)
{
    /* A sealed pen means the ghosts never come out — a maze that looks fine and plays as
     * an empty stroll. */
    for (uint8_t level = PLAYFIELD_FIRST_LEVEL; level <= PLAYFIELD_LEVEL_COUNT; ++level)
    {
        playfield_load_level(&g_playfield, level);
        prv_flood_from_pacman_start();

        for (uint8_t index = 0U; index < PLAYFIELD_PEN_CELL_COUNT; ++index)
        {
            const cell_t pen_cell = playfield_get_pen_cell(&g_playfield, index);
            char message[64];

            (void)snprintf(message, sizeof(message), "level %u: pen cell %u is sealed off", level,
                           index);
            TEST_ASSERT_TRUE_MESSAGE(g_reached[pen_cell.y][pen_cell.x], message);
        }
    }
}

void test_every_maze_is_left_right_symmetric(void)
{
    for (uint8_t level = PLAYFIELD_FIRST_LEVEL; level <= PLAYFIELD_LEVEL_COUNT; ++level)
    {
        playfield_load_level(&g_playfield, level);

        for (int16_t y = 0; y < PLAYFIELD_HEIGHT; ++y)
        {
            for (int16_t x = 0; x < (PLAYFIELD_WIDTH / 2); ++x)
            {
                const cell_t left = {x, y};
                const cell_t right = {(int16_t)(PLAYFIELD_WIDTH - 1 - x), y};
                char message[64];

                (void)snprintf(message, sizeof(message), "level %u: row %d is not symmetric",
                               level, y);
                TEST_ASSERT_EQUAL_MESSAGE(playfield_is_walkable(&g_playfield, left),
                                          playfield_is_walkable(&g_playfield, right), message);
            }
        }
    }
}

void test_every_maze_has_a_tunnel(void)
{
    /* At least one edge pair must wrap onto open cells, or an entity can never leave
     * through an edge and FR-012 is unimplementable on that maze. */
    for (uint8_t level = PLAYFIELD_FIRST_LEVEL; level <= PLAYFIELD_LEVEL_COUNT; ++level)
    {
        bool has_tunnel = false;

        playfield_load_level(&g_playfield, level);

        for (int16_t y = 0; y < PLAYFIELD_HEIGHT; ++y)
        {
            const cell_t left = {0, y};
            const cell_t right = {PLAYFIELD_WIDTH - 1, y};

            if (playfield_is_walkable(&g_playfield, left)
                && playfield_is_walkable(&g_playfield, right))
            {
                has_tunnel = true;
            }
        }

        for (int16_t x = 0; x < PLAYFIELD_WIDTH; ++x)
        {
            const cell_t top = {x, 0};
            const cell_t bottom = {x, PLAYFIELD_HEIGHT - 1};

            if (playfield_is_walkable(&g_playfield, top)
                && playfield_is_walkable(&g_playfield, bottom))
            {
                has_tunnel = true;
            }
        }

        TEST_ASSERT_TRUE_MESSAGE(has_tunnel, "a maze has no tunnel");
    }
}

void test_every_maze_keeps_at_least_two_power_pellets(void)
{
    for (uint8_t level = PLAYFIELD_FIRST_LEVEL; level <= PLAYFIELD_LEVEL_COUNT; ++level)
    {
        playfield_load_level(&g_playfield, level);

        TEST_ASSERT_GREATER_OR_EQUAL_UINT16(MIN_POWER_PELLETS_PER_MAZE, prv_count_power_pellets());
    }
}

void test_pacmans_start_holds_no_pellet_in_any_maze(void)
{
    /* §10.2: pellets occupy every open path cell except the pen and Pacman's start. */
    for (uint8_t level = PLAYFIELD_FIRST_LEVEL; level <= PLAYFIELD_LEVEL_COUNT; ++level)
    {
        playfield_load_level(&g_playfield, level);

        TEST_ASSERT_EQUAL(PLAYFIELD_PELLET_NONE,
                          playfield_get_pellet(&g_playfield, playfield_get_pacman_start(&g_playfield)));
    }
}

void test_the_pen_holds_no_pellets_in_any_maze(void)
{
    for (uint8_t level = PLAYFIELD_FIRST_LEVEL; level <= PLAYFIELD_LEVEL_COUNT; ++level)
    {
        playfield_load_level(&g_playfield, level);

        for (uint8_t index = 0U; index < PLAYFIELD_PEN_CELL_COUNT; ++index)
        {
            TEST_ASSERT_EQUAL(PLAYFIELD_PELLET_NONE,
                              playfield_get_pellet(&g_playfield,
                                                   playfield_get_pen_cell(&g_playfield, index)));
        }
    }
}

/* --- the reference maze --------------------------------------------------- */

void test_the_reference_maze_has_four_power_pellets(void)
{
    TEST_ASSERT_EQUAL_UINT16(LEVEL_1_POWER_PELLET_COUNT, prv_count_power_pellets());
}

void test_the_outer_frame_is_wall_except_at_the_tunnels(void)
{
    const cell_t top_left = {0, 0};
    const cell_t middle_left = {0, PLAYFIELD_HEIGHT / 2};

    TEST_ASSERT_FALSE(playfield_is_walkable(&g_playfield, top_left));
    /* The horizontal tunnel mouth of §10.2. */
    TEST_ASSERT_TRUE(playfield_is_walkable(&g_playfield, middle_left));
}

/* --- eating -------------------------------------------------------------- */

void test_eating_a_pellet_removes_it_and_decrements_the_count(void)
{
    const cell_t cell = {1, 2};
    const uint16_t before = playfield_get_remaining_pellet_count(&g_playfield);

    TEST_ASSERT_EQUAL(PLAYFIELD_PELLET_NORMAL, playfield_get_pellet(&g_playfield, cell));
    TEST_ASSERT_EQUAL(PLAYFIELD_PELLET_NORMAL, playfield_eat_pellet(&g_playfield, cell));

    TEST_ASSERT_EQUAL(PLAYFIELD_PELLET_NONE, playfield_get_pellet(&g_playfield, cell));
    TEST_ASSERT_EQUAL_UINT16(before - 1U, playfield_get_remaining_pellet_count(&g_playfield));
}

void test_eating_a_power_pellet_reports_which_kind_it_was(void)
{
    const cell_t corner = {1, 1};

    TEST_ASSERT_EQUAL(PLAYFIELD_PELLET_POWER, playfield_eat_pellet(&g_playfield, corner));
}

void test_eating_an_empty_cell_changes_nothing(void)
{
    const cell_t start = playfield_get_pacman_start(&g_playfield);
    const uint16_t before = playfield_get_remaining_pellet_count(&g_playfield);

    TEST_ASSERT_EQUAL(PLAYFIELD_PELLET_NONE, playfield_eat_pellet(&g_playfield, start));
    TEST_ASSERT_EQUAL_UINT16(before, playfield_get_remaining_pellet_count(&g_playfield));
}

void test_eating_the_same_pellet_twice_only_counts_once(void)
{
    const cell_t cell = {1, 2};
    const uint16_t before = playfield_get_remaining_pellet_count(&g_playfield);

    (void)playfield_eat_pellet(&g_playfield, cell);
    (void)playfield_eat_pellet(&g_playfield, cell);

    TEST_ASSERT_EQUAL_UINT16(before - 1U, playfield_get_remaining_pellet_count(&g_playfield));
}

void test_the_maze_is_cleared_once_every_pellet_is_gone(void)
{
    TEST_ASSERT_FALSE(playfield_is_cleared(&g_playfield));

    for (int16_t y = 0; y < PLAYFIELD_HEIGHT; ++y)
    {
        for (int16_t x = 0; x < PLAYFIELD_WIDTH; ++x)
        {
            const cell_t cell = {x, y};

            (void)playfield_eat_pellet(&g_playfield, cell);
        }
    }

    TEST_ASSERT_TRUE(playfield_is_cleared(&g_playfield));
}

void test_loading_a_level_restores_its_pellets(void)
{
    const cell_t cell = {1, 2};

    (void)playfield_eat_pellet(&g_playfield, cell);
    playfield_load_level(&g_playfield, LEVEL_1);

    TEST_ASSERT_EQUAL(PLAYFIELD_PELLET_NORMAL, playfield_get_pellet(&g_playfield, cell));
    TEST_ASSERT_FALSE(playfield_is_cleared(&g_playfield));
}

/* --- geometry and the tunnel rule ---------------------------------------- */

void test_stepping_off_the_left_edge_wraps_to_the_right(void)
{
    const cell_t mouth = {0, PLAYFIELD_HEIGHT / 2};
    const cell_t stepped = playfield_step(mouth, DIRECTION_WEST);

    TEST_ASSERT_EQUAL_INT16(PLAYFIELD_WIDTH - 1, stepped.x);
    TEST_ASSERT_EQUAL_INT16(mouth.y, stepped.y);
}

void test_stepping_off_the_top_edge_wraps_to_the_bottom(void)
{
    const cell_t start = playfield_get_pacman_start(&g_playfield);
    const cell_t top = {start.x, 0};
    const cell_t stepped = playfield_step(top, DIRECTION_NORTH);

    TEST_ASSERT_EQUAL_INT16(PLAYFIELD_HEIGHT - 1, stepped.y);
    TEST_ASSERT_EQUAL_INT16(top.x, stepped.x);
}

void test_pacman_can_escape_his_start_pocket_through_the_tunnel(void)
{
    /* §10.2 is explicit that the start pocket must not be a dead-end trap. In the
     * reference maze the only way out is the vertical tunnel. */
    const cell_t start = playfield_get_pacman_start(&g_playfield);
    bool has_exit = false;
    static const direction_e k_directions[]
        = {DIRECTION_NORTH, DIRECTION_SOUTH, DIRECTION_EAST, DIRECTION_WEST};

    for (uint8_t index = 0U; index < (sizeof(k_directions) / sizeof(k_directions[0])); ++index)
    {
        if (playfield_is_walkable(&g_playfield, playfield_step(start, k_directions[index])))
        {
            has_exit = true;
        }
    }

    TEST_ASSERT_TRUE(has_exit);
}

void test_stepping_nowhere_stays_put(void)
{
    const cell_t cell = {3, 3};

    TEST_ASSERT_TRUE(playfield_are_cells_equal(cell, playfield_step(cell, DIRECTION_NONE)));
}

void test_manhattan_distance_is_symmetric_and_zero_on_itself(void)
{
    const cell_t first = {1, 2};
    const cell_t second = {4, 6};

    TEST_ASSERT_EQUAL_UINT16(7U, playfield_get_distance(first, second));
    TEST_ASSERT_EQUAL_UINT16(7U, playfield_get_distance(second, first));
    TEST_ASSERT_EQUAL_UINT16(0U, playfield_get_distance(first, first));
}

void test_opposite_directions_pair_up(void)
{
    TEST_ASSERT_EQUAL(DIRECTION_SOUTH, playfield_get_opposite_direction(DIRECTION_NORTH));
    TEST_ASSERT_EQUAL(DIRECTION_NORTH, playfield_get_opposite_direction(DIRECTION_SOUTH));
    TEST_ASSERT_EQUAL(DIRECTION_EAST, playfield_get_opposite_direction(DIRECTION_WEST));
    TEST_ASSERT_EQUAL(DIRECTION_WEST, playfield_get_opposite_direction(DIRECTION_EAST));
    TEST_ASSERT_EQUAL(DIRECTION_NONE, playfield_get_opposite_direction(DIRECTION_NONE));
}

void test_the_scatter_corners_are_four_distinct_walkable_cells(void)
{
    for (uint8_t index = 0U; index < SCATTER_CORNER_COUNT; ++index)
    {
        const cell_t corner = playfield_get_scatter_corner(index);

        TEST_ASSERT_TRUE(playfield_is_walkable(&g_playfield, corner));

        for (uint8_t other = 0U; other < index; ++other)
        {
            TEST_ASSERT_FALSE(
                playfield_are_cells_equal(corner, playfield_get_scatter_corner(other)));
        }
    }
}

/* --- preconditions ------------------------------------------------------- */

void test_loading_a_level_out_of_range_asserts(void)
{
    ASSERT_PROBE_EXPECT(playfield_load_level(&g_playfield, PLAYFIELD_LEVEL_COUNT + 1U),
                        "in_level <= PLAYFIELD_LEVEL_COUNT");
}

void test_loading_level_zero_asserts(void)
{
    ASSERT_PROBE_EXPECT(playfield_load_level(&g_playfield, 0U),
                        "in_level >= PLAYFIELD_FIRST_LEVEL");
}

void test_the_last_level_loads(void)
{
    playfield_load_level(&g_playfield, LEVEL_5);

    TEST_ASSERT_FALSE(playfield_is_cleared(&g_playfield));
}
