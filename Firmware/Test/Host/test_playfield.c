/*
 * Unit tests for App/playfield.
 *
 * The valuable part is the sweep over the maze table. It is 28 x 31 characters typed out by
 * hand, and a single mistyped wall can seal off a region or the ghost pen. That would not
 * fail a build, would not fail an OTT, and would surface only as a level that cannot be
 * finished or ghosts that never come out. A flood fill settles it in microseconds, so every
 * constraint §10.2 states is checked mechanically: dimensions, left-right symmetry, one
 * Pacman start, a full pen, a reachable pen, every pellet reachable, and a tunnel.
 *
 * There used to be five mazes and this swept all of them. There is one now (FR-025 as
 * re-baselined) — difficulty comes from `difficulty`, the way the arcade does it — so the
 * sweep is over the one table, and it matters more rather than less: everything is played
 * on it.
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

/* Transcribed from §10.2, so a change to the maze shows up here as a failing test rather
 * than as a silently different game. The pellet total is what Cruise Elroy's thresholds are
 * measured against, which is why it is pinned rather than merely bounded. */
#define POWER_PELLET_COUNT   (4U)
#define TOTAL_PELLET_COUNT   (244U)

/* The tunnel row, and a cell inside each of its two mouths. */
#define TUNNEL_ROW           (14)
#define TUNNEL_WEST_X        (2)
#define TUNNEL_EAST_X        (25)

#define SCATTER_CORNER_COUNT (4U)

static playfield_t g_playfield;

static bool g_reached[PLAYFIELD_HEIGHT][PLAYFIELD_WIDTH];

/* Flood fill from a cell, following the same walkability and tunnel rules the game uses,
 * so the test cannot disagree with the movement code about what is connected. */
static void prv_flood(cell_t in_cell)
{
    static const direction_e k_directions[] = {DIRECTION_NORTH, DIRECTION_SOUTH, DIRECTION_EAST, DIRECTION_WEST};
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

/* The first cell holding a normal pellet, so the eating tests do not carry a coordinate
 * that a redrawn maze would silently invalidate. */
static cell_t prv_find_a_pellet(void)
{
    for (int16_t y = 0; y < PLAYFIELD_HEIGHT; ++y)
    {
        for (int16_t x = 0; x < PLAYFIELD_WIDTH; ++x)
        {
            const cell_t cell = {x, y};

            if (playfield_get_pellet(&g_playfield, cell) == PLAYFIELD_PELLET_NORMAL)
            {
                return cell;
            }
        }
    }

    TEST_FAIL_MESSAGE("the maze holds no normal pellet at all");

    return playfield_get_pacman_start(&g_playfield);
}

void setUp(void)
{
    assert_probe_begin();
    playfield_load(&g_playfield);
}

void tearDown(void)
{
    assert_probe_end();
}

/* --- the maze must satisfy §10.2 ----------------------------------------- */

void test_the_maze_is_fully_connected_from_pacmans_start(void)
{
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

            (void)snprintf(message, sizeof(message), "open cell %d,%d is unreachable", x, y);
            TEST_ASSERT_TRUE_MESSAGE(g_reached[y][x], message);
        }
    }
}

void test_every_pellet_can_be_eaten(void)
{
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

            (void)snprintf(message, sizeof(message), "pellet at %d,%d is unreachable", x, y);
            TEST_ASSERT_TRUE_MESSAGE(g_reached[y][x], message);
        }
    }
}

void test_the_ghost_pen_is_reachable(void)
{
    /* A sealed pen means the ghosts never come out — a maze that looks fine and plays as
     * an empty stroll. */
    prv_flood_from_pacman_start();

    for (uint8_t index = 0U; index < PLAYFIELD_PEN_CELL_COUNT; ++index)
    {
        const cell_t pen_cell = playfield_get_pen_cell(&g_playfield, index);
        char message[64];

        (void)snprintf(message, sizeof(message), "pen cell %u is sealed off", index);
        TEST_ASSERT_TRUE_MESSAGE(g_reached[pen_cell.y][pen_cell.x], message);
    }
}

void test_the_maze_is_left_right_symmetric(void)
{
    for (int16_t y = 0; y < PLAYFIELD_HEIGHT; ++y)
    {
        for (int16_t x = 0; x < (PLAYFIELD_WIDTH / 2); ++x)
        {
            const cell_t left = {x, y};
            const cell_t right = {(int16_t)(PLAYFIELD_WIDTH - 1 - x), y};
            char message[64];

            (void)snprintf(message, sizeof(message), "row %d is not symmetric", y);
            TEST_ASSERT_EQUAL_MESSAGE(playfield_is_walkable(&g_playfield, left),
                                      playfield_is_walkable(&g_playfield, right), message);
        }
    }
}

void test_the_maze_has_a_tunnel(void)
{
    /* At least one edge pair must wrap onto open cells, or an entity can never leave
     * through an edge and FR-012 is unimplementable. */
    bool has_tunnel = false;

    for (int16_t y = 0; y < PLAYFIELD_HEIGHT; ++y)
    {
        const cell_t left = {0, y};
        const cell_t right = {PLAYFIELD_WIDTH - 1, y};

        if (playfield_is_walkable(&g_playfield, left) && playfield_is_walkable(&g_playfield, right))
        {
            has_tunnel = true;
        }
    }

    TEST_ASSERT_TRUE_MESSAGE(has_tunnel, "the maze has no tunnel");
}

void test_pacmans_start_holds_no_pellet(void)
{
    /* §10.2: pellets occupy every open path cell except the pen and Pacman's start. */
    TEST_ASSERT_EQUAL(PLAYFIELD_PELLET_NONE,
                      playfield_get_pellet(&g_playfield, playfield_get_pacman_start(&g_playfield)));
}

void test_the_pen_holds_no_pellets(void)
{
    for (uint8_t index = 0U; index < PLAYFIELD_PEN_CELL_COUNT; ++index)
    {
        TEST_ASSERT_EQUAL(PLAYFIELD_PELLET_NONE,
                          playfield_get_pellet(&g_playfield, playfield_get_pen_cell(&g_playfield, index)));
    }
}

void test_the_maze_has_four_power_pellets(void)
{
    TEST_ASSERT_EQUAL_UINT16(POWER_PELLET_COUNT, prv_count_power_pellets());
}

void test_the_pellet_total_is_the_one_cruise_elroy_is_calibrated_against(void)
{
    /* §10.9's Elroy thresholds are the arcade's absolute counts against its own 244, so
     * this being 244 exactly is what makes them mean what they meant there. If the maze is
     * ever redrawn, this is the test that says the thresholds need rethinking. */
    TEST_ASSERT_EQUAL_UINT16(TOTAL_PELLET_COUNT, playfield_get_total_pellet_count(&g_playfield));
    TEST_ASSERT_EQUAL_UINT16(TOTAL_PELLET_COUNT, playfield_get_remaining_pellet_count(&g_playfield));
}

void test_the_outer_frame_is_wall_except_at_the_tunnel(void)
{
    const cell_t top_left = {0, 0};
    const cell_t west_mouth = {0, TUNNEL_ROW};
    const cell_t east_mouth = {PLAYFIELD_WIDTH - 1, TUNNEL_ROW};

    TEST_ASSERT_FALSE(playfield_is_walkable(&g_playfield, top_left));
    TEST_ASSERT_TRUE(playfield_is_walkable(&g_playfield, west_mouth));
    TEST_ASSERT_TRUE(playfield_is_walkable(&g_playfield, east_mouth));
}

/* --- the tunnel, as a place rather than a row ---------------------------- */

void test_both_tunnel_mouths_are_marked_as_tunnel(void)
{
    const cell_t west = {TUNNEL_WEST_X, TUNNEL_ROW};
    const cell_t east = {TUNNEL_EAST_X, TUNNEL_ROW};

    TEST_ASSERT_TRUE(playfield_is_tunnel(&g_playfield, west));
    TEST_ASSERT_TRUE(playfield_is_tunnel(&g_playfield, east));
}

void test_the_corridor_that_merely_leads_to_the_tunnel_is_not_tunnel(void)
{
    /* The distinction the marking exists for: a ghost is only slowed inside the tunnel
     * itself (§10.9), not everywhere on its row. Deriving it from the row would catch the
     * junction the tunnel opens onto and hand Pacman an escape he has not earned. */
    const cell_t junction = {6, TUNNEL_ROW};

    TEST_ASSERT_TRUE(playfield_is_walkable(&g_playfield, junction));
    TEST_ASSERT_FALSE(playfield_is_tunnel(&g_playfield, junction));
}

void test_nothing_outside_the_tunnel_row_is_tunnel(void)
{
    for (int16_t y = 0; y < PLAYFIELD_HEIGHT; ++y)
    {
        for (int16_t x = 0; x < PLAYFIELD_WIDTH; ++x)
        {
            const cell_t cell = {x, y};
            char message[64];

            if (y == TUNNEL_ROW)
            {
                continue;
            }

            (void)snprintf(message, sizeof(message), "cell %d,%d is marked as tunnel", x, y);
            TEST_ASSERT_FALSE_MESSAGE(playfield_is_tunnel(&g_playfield, cell), message);
        }
    }
}

void test_a_tunnel_cell_is_walkable_and_holds_nothing_to_eat(void)
{
    const cell_t west = {TUNNEL_WEST_X, TUNNEL_ROW};

    TEST_ASSERT_TRUE(playfield_is_walkable(&g_playfield, west));
    TEST_ASSERT_EQUAL(PLAYFIELD_PELLET_NONE, playfield_get_pellet(&g_playfield, west));
}

/* --- eating -------------------------------------------------------------- */

void test_eating_a_pellet_removes_it_and_decrements_the_count(void)
{
    const cell_t cell = prv_find_a_pellet();
    const uint16_t before = playfield_get_remaining_pellet_count(&g_playfield);

    TEST_ASSERT_EQUAL(PLAYFIELD_PELLET_NORMAL, playfield_eat_pellet(&g_playfield, cell));

    TEST_ASSERT_EQUAL(PLAYFIELD_PELLET_NONE, playfield_get_pellet(&g_playfield, cell));
    TEST_ASSERT_EQUAL_UINT16(before - 1U, playfield_get_remaining_pellet_count(&g_playfield));
}

void test_eating_a_power_pellet_reports_which_kind_it_was(void)
{
    /* §10.2 puts the four power pellets one row in from the top and bottom corners, not in
     * the corners themselves — the corner cells are ordinary pellets. */
    const cell_t corner = {1, 3};

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
    const cell_t cell = prv_find_a_pellet();
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

void test_loading_restores_the_pellets(void)
{
    const cell_t cell = prv_find_a_pellet();

    (void)playfield_eat_pellet(&g_playfield, cell);
    playfield_load(&g_playfield);

    TEST_ASSERT_EQUAL(PLAYFIELD_PELLET_NORMAL, playfield_get_pellet(&g_playfield, cell));
    TEST_ASSERT_FALSE(playfield_is_cleared(&g_playfield));
    TEST_ASSERT_EQUAL_UINT16(playfield_get_total_pellet_count(&g_playfield),
                             playfield_get_remaining_pellet_count(&g_playfield));
}

/* --- geometry and the tunnel rule ---------------------------------------- */

void test_stepping_off_the_left_edge_wraps_to_the_right(void)
{
    const cell_t mouth = {0, TUNNEL_ROW};
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

void test_pacman_can_leave_his_start_cell(void)
{
    /* §10.2 is explicit that the start must not be a dead-end trap. */
    const cell_t start = playfield_get_pacman_start(&g_playfield);
    bool has_exit = false;
    static const direction_e k_directions[] = {DIRECTION_NORTH, DIRECTION_SOUTH, DIRECTION_EAST, DIRECTION_WEST};

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
            TEST_ASSERT_FALSE(playfield_are_cells_equal(corner, playfield_get_scatter_corner(other)));
        }
    }
}

/* --- preconditions ------------------------------------------------------- */

void test_a_null_playfield_asserts(void)
{
    const cell_t cell = {1, 1};

    ASSERT_PROBE_EXPECT(playfield_load(NULL), "inout_playfield != NULL");
    ASSERT_PROBE_EXPECT((void)playfield_is_walkable(NULL, cell), "in_playfield != NULL");
    ASSERT_PROBE_EXPECT((void)playfield_is_tunnel(NULL, cell), "in_playfield != NULL");
    ASSERT_PROBE_EXPECT((void)playfield_get_total_pellet_count(NULL), "in_playfield != NULL");
}

void test_asking_for_a_pen_cell_out_of_range_asserts(void)
{
    ASSERT_PROBE_EXPECT((void)playfield_get_pen_cell(&g_playfield, PLAYFIELD_PEN_CELL_COUNT),
                        "in_index < PLAYFIELD_PEN_CELL_COUNT");
}
