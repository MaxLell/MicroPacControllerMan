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

/* A junction on the maze's open row 5 with a gap to the north, so all four directions are
 * worth considering. */
#define JUNCTION_X     (6)
#define JUNCTION_Y     (5)

/* A cell on row 5 whose north neighbour is a wall. */
#define WALLED_NORTH_X (2)
#define OPEN_CELL_X    (4)

/* The dead-end case has to be built rather than found: the arcade maze contains no stub
 * at all — every open cell in it has at least two open neighbours, which is itself worth
 * knowing and is asserted below. The rule still has to hold, because it is a property of
 * the algorithm and not of this maze, and the next maze may well have one. */
#define STUB_X         (1)
#define STUB_Y         (1)
#define STUB_SEALED_Y  (2)

static playfield_t g_playfield;

static cell_t prv_cell(int16_t in_x, int16_t in_y)
{
    const cell_t cell = {in_x, in_y};

    return cell;
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

void test_the_maze_itself_has_no_dead_end(void)
{
    /* Not a property this module needs, but the premise of the test below: the stub it
     * exercises has to be built, and this is what says so. It is also a real fact about
     * the arcade layout — there is nowhere in it to be cornered by geometry alone. */
    for (int16_t y = 0; y < PLAYFIELD_HEIGHT; ++y)
    {
        for (int16_t x = 0; x < PLAYFIELD_WIDTH; ++x)
        {
            static const direction_e k_directions[] = {DIRECTION_NORTH, DIRECTION_SOUTH, DIRECTION_EAST,
                                                       DIRECTION_WEST};
            const cell_t cell = prv_cell(x, y);
            uint8_t open_neighbours = 0U;

            if (!playfield_is_walkable(&g_playfield, cell))
            {
                continue;
            }

            for (uint8_t index = 0U; index < (sizeof(k_directions) / sizeof(k_directions[0])); ++index)
            {
                if (playfield_is_walkable(&g_playfield, playfield_step(cell, k_directions[index])))
                {
                    ++open_neighbours;
                }
            }

            TEST_ASSERT_GREATER_THAN_UINT8(1U, open_neighbours);
        }
    }
}

void test_a_dead_end_forces_the_reverse(void)
{
    /* A stub with one exit: coming in, the only step left is back out. The rule has to
     * bend or the ghost is parked there for the rest of the level.
     *
     * The stub is made by sealing one neighbour of a corner cell, because the maze has
     * none to borrow (see above). Walling a cell directly is a liberty, but the
     * alternative — keeping a second maze around purely to hold one stub — would tie this
     * module's test to a layout it does not otherwise care about. */
    const cell_t stub = prv_cell(STUB_X, STUB_Y);
    const cell_t target = prv_cell(PLAYFIELD_WIDTH - 2, PLAYFIELD_HEIGHT - 2);
    const direction_e arrived_from = DIRECTION_EAST;
    direction_e chosen;

    g_playfield.walls[STUB_SEALED_Y][STUB_X] = true;

    /* Guard the premise: exactly one open neighbour, and it is the way back. */
    TEST_ASSERT_TRUE(playfield_is_walkable(&g_playfield, playfield_step(stub, DIRECTION_EAST)));
    TEST_ASSERT_FALSE(playfield_is_walkable(&g_playfield, playfield_step(stub, DIRECTION_NORTH)));
    TEST_ASSERT_FALSE(playfield_is_walkable(&g_playfield, playfield_step(stub, DIRECTION_SOUTH)));
    TEST_ASSERT_FALSE(playfield_is_walkable(&g_playfield, playfield_step(stub, DIRECTION_WEST)));

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
