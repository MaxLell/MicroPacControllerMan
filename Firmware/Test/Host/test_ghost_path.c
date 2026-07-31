/*
 * Unit tests for App/ghost_path.
 *
 * Two things are worth nailing down here, because both are invisible in play and both
 * would be silently lost if the algorithm is ever swapped for A*:
 *
 * - **The tie-break order.** §10.4 fixes it as Up, Left, Down, Right. Get it wrong and
 *   the ghosts still chase perfectly well — they just do it differently, so no test that
 *   only checks "it got closer" would notice, and the behaviour would quietly change
 *   under the swap.
 * - **The dead-end escape.** §10.1 forbids reversing, but a stub corridor leaves no
 *   alternative. Without the exception a ghost parks in a dead end for the rest of the
 *   level, which looks like a broken ghost rather than a rule.
 *
 * The tests state *what* the algorithm must achieve, never how, so they survive the move
 * to A* unchanged.
 */
#include <stdbool.h>
#include <stdint.h>

#include "assert_probe.h"
#include "custom_assert.h"
#include "ghost_path.h"
#include "msg.h"
#include "playfield.h"
#include "unity.h"

#define LEVEL_1        (1U)

/* A junction on the reference maze's open row 3 with a gap to the north, so all four
 * directions are worth considering. */
#define JUNCTION_X     (3)
#define JUNCTION_Y     (3)

/* A cell on row 3 whose north neighbour is a wall. */
#define WALLED_NORTH_X (2)
#define OPEN_CELL_X    (4)

/* Level 5 was authored with genuine dead-end stubs; the reference maze has none, being a
 * fully cross-linked grid. (3,7) there has exactly one open neighbour, to the west. */
#define LEVEL_5        (5U)
#define DEAD_END_X     (3)
#define DEAD_END_Y     (7)

static playfield_t g_playfield;

static cell_t prv_cell(int16_t in_x, int16_t in_y)
{
    const cell_t cell = {in_x, in_y};

    return cell;
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

/* --- heading towards a target -------------------------------------------- */

void test_it_steps_towards_a_target_to_the_east(void)
{
    const cell_t from = prv_cell(JUNCTION_X, JUNCTION_Y);
    const cell_t target = prv_cell(PLAYFIELD_WIDTH - 2, JUNCTION_Y);

    TEST_ASSERT_EQUAL(DIRECTION_EAST, ghost_path_find_step_towards(&g_playfield, from, target, DIRECTION_NONE));
}

void test_it_steps_towards_a_target_to_the_west(void)
{
    const cell_t from = prv_cell(JUNCTION_X, JUNCTION_Y);
    const cell_t target = prv_cell(1, JUNCTION_Y);

    TEST_ASSERT_EQUAL(DIRECTION_WEST, ghost_path_find_step_towards(&g_playfield, from, target, DIRECTION_NONE));
}

void test_it_steps_towards_a_target_to_the_north(void)
{
    const cell_t from = prv_cell(JUNCTION_X, JUNCTION_Y);
    const cell_t target = prv_cell(JUNCTION_X, 1);

    TEST_ASSERT_EQUAL(DIRECTION_NORTH, ghost_path_find_step_towards(&g_playfield, from, target, DIRECTION_NONE));
}

void test_it_never_steps_into_a_wall(void)
{
    /* Aim north from a cell whose north neighbour is a wall: it must pick something else
     * rather than walk into it. */
    const cell_t from = prv_cell(WALLED_NORTH_X, JUNCTION_Y);
    const cell_t target = prv_cell(WALLED_NORTH_X, 0);
    const direction_e chosen = ghost_path_find_step_towards(&g_playfield, from, target, DIRECTION_NONE);

    TEST_ASSERT_NOT_EQUAL(DIRECTION_NORTH, chosen);
    TEST_ASSERT_TRUE(playfield_is_walkable(&g_playfield, playfield_step(from, chosen)));
}

void test_a_target_on_the_current_cell_still_yields_a_legal_step(void)
{
    /* Degenerate but reachable in play: Blinky standing on Pacman for one tick. */
    const cell_t from = prv_cell(JUNCTION_X, JUNCTION_Y);
    const direction_e chosen = ghost_path_find_step_towards(&g_playfield, from, from, DIRECTION_NONE);

    TEST_ASSERT_NOT_EQUAL(DIRECTION_NONE, chosen);
    TEST_ASSERT_TRUE(playfield_is_walkable(&g_playfield, playfield_step(from, chosen)));
}

void test_an_off_maze_target_is_allowed(void)
{
    /* §10.4's Pinky and Inky routinely aim outside the grid; that must not assert or
     * return nothing. */
    const cell_t from = prv_cell(JUNCTION_X, JUNCTION_Y);
    const cell_t target = prv_cell(-20, -20);

    TEST_ASSERT_NOT_EQUAL(DIRECTION_NONE, ghost_path_find_step_towards(&g_playfield, from, target, DIRECTION_NONE));
}

/* --- the tie-break order (§10.4) ----------------------------------------- */

void test_ties_break_up_before_left(void)
{
    /* From the junction, aim at a target equally far north and west. Up must win. */
    const cell_t from = prv_cell(JUNCTION_X, JUNCTION_Y);
    const cell_t north_neighbour = playfield_step(from, DIRECTION_NORTH);
    const cell_t west_neighbour = playfield_step(from, DIRECTION_WEST);
    const cell_t target = prv_cell((int16_t)(from.x - 4), (int16_t)(from.y - 4));

    /* Guard the premise: both must be open and equidistant, or the test proves nothing. */
    TEST_ASSERT_TRUE(playfield_is_walkable(&g_playfield, north_neighbour));
    TEST_ASSERT_TRUE(playfield_is_walkable(&g_playfield, west_neighbour));
    TEST_ASSERT_EQUAL_UINT16(playfield_get_distance(north_neighbour, target),
                             playfield_get_distance(west_neighbour, target));

    TEST_ASSERT_EQUAL(DIRECTION_NORTH, ghost_path_find_step_towards(&g_playfield, from, target, DIRECTION_NONE));
}

void test_ties_break_left_before_down_and_right(void)
{
    /* A far-off target straight above a cell whose north is walled leaves West, Down and
     * Right all exactly equidistant, so the order alone decides and Left must take it.
     * The target is deliberately outside the maze, which §10.4 permits. */
    const cell_t from = prv_cell(OPEN_CELL_X, JUNCTION_Y);
    const cell_t west_neighbour = playfield_step(from, DIRECTION_WEST);
    const cell_t south_neighbour = playfield_step(from, DIRECTION_SOUTH);
    const cell_t east_neighbour = playfield_step(from, DIRECTION_EAST);
    const cell_t target = prv_cell(from.x, (int16_t)(from.y - 9));

    /* Guard the premise, so a redrawn maze fails loudly instead of passing vacuously. */
    TEST_ASSERT_FALSE(playfield_is_walkable(&g_playfield, playfield_step(from, DIRECTION_NORTH)));
    TEST_ASSERT_EQUAL_UINT16(playfield_get_distance(west_neighbour, target),
                             playfield_get_distance(south_neighbour, target));
    TEST_ASSERT_EQUAL_UINT16(playfield_get_distance(west_neighbour, target),
                             playfield_get_distance(east_neighbour, target));

    TEST_ASSERT_EQUAL(DIRECTION_WEST, ghost_path_find_step_towards(&g_playfield, from, target, DIRECTION_NONE));
}

/* --- the no-reverse rule (§10.1) ----------------------------------------- */

void test_the_forbidden_direction_is_avoided_when_there_is_a_choice(void)
{
    /* Target to the west, but west is where it came from: it must pick something else
     * even though that is worse. */
    const cell_t from = prv_cell(JUNCTION_X, JUNCTION_Y);
    const cell_t target = prv_cell(1, JUNCTION_Y);

    TEST_ASSERT_NOT_EQUAL(DIRECTION_WEST, ghost_path_find_step_towards(&g_playfield, from, target, DIRECTION_WEST));
}

void test_a_dead_end_forces_the_reverse(void)
{
    /* A stub with one exit: coming in, the only step left is back out. The rule has to
     * bend or the ghost is parked there for the rest of the level. */
    const cell_t stub = prv_cell(DEAD_END_X, DEAD_END_Y);
    const cell_t target = prv_cell(1, 1);
    const direction_e arrived_from = DIRECTION_WEST;
    direction_e chosen;

    playfield_load_level(&g_playfield, LEVEL_5);

    /* Guard the premise: exactly one open neighbour, and it is the way back. */
    TEST_ASSERT_TRUE(playfield_is_walkable(&g_playfield, playfield_step(stub, DIRECTION_WEST)));
    TEST_ASSERT_FALSE(playfield_is_walkable(&g_playfield, playfield_step(stub, DIRECTION_NORTH)));
    TEST_ASSERT_FALSE(playfield_is_walkable(&g_playfield, playfield_step(stub, DIRECTION_SOUTH)));
    TEST_ASSERT_FALSE(playfield_is_walkable(&g_playfield, playfield_step(stub, DIRECTION_EAST)));

    chosen = ghost_path_find_step_towards(&g_playfield, stub, target, arrived_from);

    TEST_ASSERT_EQUAL(arrived_from, chosen);
}

/* --- fleeing (§10.5) ----------------------------------------------------- */

void test_fleeing_moves_away_from_the_cell_to_avoid(void)
{
    const cell_t from = prv_cell(JUNCTION_X, JUNCTION_Y);
    const cell_t pacman_cell = prv_cell(1, JUNCTION_Y);
    const direction_e chosen = ghost_path_find_step_away_from(&g_playfield, from, pacman_cell, DIRECTION_NONE);
    const cell_t neighbour = playfield_step(from, chosen);

    TEST_ASSERT_GREATER_THAN_UINT16(playfield_get_distance(from, pacman_cell),
                                    playfield_get_distance(neighbour, pacman_cell));
}

void test_fleeing_and_seeking_disagree_from_the_same_cell(void)
{
    /* The clearest statement that the two senses are genuinely opposite. */
    const cell_t from = prv_cell(JUNCTION_X, JUNCTION_Y);
    const cell_t pacman_cell = prv_cell(1, JUNCTION_Y);

    TEST_ASSERT_NOT_EQUAL(ghost_path_find_step_towards(&g_playfield, from, pacman_cell, DIRECTION_NONE),
                          ghost_path_find_step_away_from(&g_playfield, from, pacman_cell, DIRECTION_NONE));
}

void test_fleeing_uses_the_same_tie_break_order(void)
{
    const cell_t from = prv_cell(JUNCTION_X, JUNCTION_Y);
    const cell_t north_neighbour = playfield_step(from, DIRECTION_NORTH);
    const cell_t west_neighbour = playfield_step(from, DIRECTION_WEST);
    /* Equally far from both neighbours, so the order decides. */
    const cell_t avoid = prv_cell((int16_t)(from.x + 4), (int16_t)(from.y + 4));

    TEST_ASSERT_EQUAL_UINT16(playfield_get_distance(north_neighbour, avoid),
                             playfield_get_distance(west_neighbour, avoid));

    TEST_ASSERT_EQUAL(DIRECTION_NORTH, ghost_path_find_step_away_from(&g_playfield, from, avoid, DIRECTION_NONE));
}

void test_fleeing_never_steps_into_a_wall(void)
{
    const cell_t from = prv_cell(WALLED_NORTH_X, JUNCTION_Y);
    const cell_t avoid = prv_cell(WALLED_NORTH_X, (int16_t)(JUNCTION_Y + 3));
    const direction_e chosen = ghost_path_find_step_away_from(&g_playfield, from, avoid, DIRECTION_NONE);

    TEST_ASSERT_TRUE(playfield_is_walkable(&g_playfield, playfield_step(from, chosen)));
}

/* --- statelessness ------------------------------------------------------- */

void test_the_same_question_always_gets_the_same_answer(void)
{
    /* The library keeps nothing between calls, so repetition must not drift. If a future
     * A* implementation caches anything, this is what catches a stale cache. */
    const cell_t from = prv_cell(JUNCTION_X, JUNCTION_Y);
    const cell_t target = prv_cell(1, 1);
    const direction_e first = ghost_path_find_step_towards(&g_playfield, from, target, DIRECTION_NONE);

    for (uint8_t repeat = 0U; repeat < 5U; ++repeat)
    {
        TEST_ASSERT_EQUAL(first, ghost_path_find_step_towards(&g_playfield, from, target, DIRECTION_NONE));
    }
}

/* --- preconditions ------------------------------------------------------- */

void test_a_null_playfield_asserts(void)
{
    const cell_t from = prv_cell(JUNCTION_X, JUNCTION_Y);

    ASSERT_PROBE_EXPECT(ghost_path_find_step_towards(NULL, from, from, DIRECTION_NONE), "in_playfield != NULL");
}
