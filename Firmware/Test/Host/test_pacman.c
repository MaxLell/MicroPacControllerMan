/*
 * Unit tests for App/pacman and the App/agent base it is built on.
 *
 * The queued-direction rule of §10.1 is what these are really for. It is the difference
 * between controls that feel responsive and controls that eat your inputs, and it is
 * impossible to judge from reading the code: a turn asked for slightly early must still
 * happen at the junction, and an impossible turn must be remembered rather than dropped.
 *
 * Positions are taken from the playfield rather than hard-coded, so the tests keep
 * meaning if the reference maze is ever re-drawn.
 */
#include <stdbool.h>
#include <stdint.h>

#include "agent.h"
#include "assert_probe.h"
#include "custom_assert.h"
#include "msg.h"
#include "pacman.h"
#include "playfield.h"
#include "unity.h"

#define LEVEL_1 (1U)
#define ONE_STEP (1U)
#define TWO_STEPS (2U)

/* Row 3 of the reference maze is an open corridor; row 2 above it alternates wall and
 * gap. These cells are picked from that: WALLED_NORTH has a wall above it, OPEN_NORTH a
 * gap, and both have open cells to the left and right. */
#define OPEN_CELL_X (4)
#define OPEN_CELL_Y (3)
#define CORRIDOR_START_X (1)
#define WALLED_NORTH_X (2)
#define OPEN_NORTH_X (3)
#define CORRIDOR_LAST_X (PLAYFIELD_WIDTH - 2)

static playfield_t g_playfield;
static pacman_t g_pacman;

static cell_t prv_cell(int16_t in_x, int16_t in_y)
{
    const cell_t cell = {in_x, in_y};

    return cell;
}

/* Put Pacman on a known open cell rather than his start pocket, so a test can move him
 * in more than one direction. */
static void prv_place_pacman_in_the_open(void)
{
    pacman_reset(&g_pacman, prv_cell(OPEN_CELL_X, OPEN_CELL_Y));
}

void setUp(void)
{
    assert_probe_begin();
    playfield_load_level(&g_playfield, LEVEL_1);
    pacman_reset(&g_pacman, playfield_get_pacman_start(&g_playfield));
}

void tearDown(void)
{
    assert_probe_end();
}

/* --- the agent base ------------------------------------------------------- */

void test_a_placed_agent_reports_where_it_is(void)
{
    agent_t agent;
    const cell_t cell = prv_cell(OPEN_CELL_X, OPEN_CELL_Y);

    agent_place(&agent, cell, DIRECTION_EAST);

    TEST_ASSERT_TRUE(playfield_are_cells_equal(cell, agent_get_cell(&agent)));
    TEST_ASSERT_EQUAL(DIRECTION_EAST, agent_get_direction(&agent));
}

void test_an_agent_steps_onto_an_open_cell(void)
{
    agent_t agent;

    agent_place(&agent, prv_cell(OPEN_CELL_X, OPEN_CELL_Y), DIRECTION_NONE);

    TEST_ASSERT_TRUE(agent_step(&agent, &g_playfield, DIRECTION_EAST));
    TEST_ASSERT_TRUE(playfield_are_cells_equal(prv_cell(OPEN_CELL_X + 1, OPEN_CELL_Y),
                                               agent_get_cell(&agent)));
}

void test_an_agent_blocked_by_a_wall_stays_put_but_turns_to_face_it(void)
{
    agent_t agent;

    /* Row 3 of the reference maze is a corridor with wall above and below at this cell. */
    agent_place(&agent, prv_cell(OPEN_CELL_X, OPEN_CELL_Y), DIRECTION_EAST);

    TEST_ASSERT_FALSE(agent_can_step(&agent, &g_playfield, DIRECTION_NORTH));
    TEST_ASSERT_FALSE(agent_step(&agent, &g_playfield, DIRECTION_NORTH));

    TEST_ASSERT_TRUE(playfield_are_cells_equal(prv_cell(OPEN_CELL_X, OPEN_CELL_Y),
                                               agent_get_cell(&agent)));
    /* The facing still changed — that is what lets it move the instant the way opens. */
    TEST_ASSERT_EQUAL(DIRECTION_NORTH, agent_get_direction(&agent));
}

void test_an_agent_never_steps_nowhere(void)
{
    agent_t agent;

    agent_place(&agent, prv_cell(OPEN_CELL_X, OPEN_CELL_Y), DIRECTION_EAST);

    TEST_ASSERT_FALSE(agent_step(&agent, &g_playfield, DIRECTION_NONE));
    TEST_ASSERT_EQUAL(DIRECTION_EAST, agent_get_direction(&agent));
}

void test_an_agent_looks_ahead_along_its_facing(void)
{
    agent_t agent;

    agent_place(&agent, prv_cell(OPEN_CELL_X, OPEN_CELL_Y), DIRECTION_EAST);

    TEST_ASSERT_TRUE(playfield_are_cells_equal(prv_cell(OPEN_CELL_X + TWO_STEPS, OPEN_CELL_Y),
                                               agent_get_cell_ahead(&agent, TWO_STEPS)));
}

void test_looking_ahead_wraps_through_a_tunnel(void)
{
    agent_t agent;
    const cell_t mouth = prv_cell(0, PLAYFIELD_HEIGHT / 2);

    agent_place(&agent, mouth, DIRECTION_WEST);

    TEST_ASSERT_TRUE(playfield_are_cells_equal(prv_cell(PLAYFIELD_WIDTH - 1, mouth.y),
                                               agent_get_cell_ahead(&agent, ONE_STEP)));
}

/* --- Pacman: the queued direction ---------------------------------------- */

void test_a_fresh_pacman_has_no_direction_and_does_not_move(void)
{
    const cell_t start = pacman_get_cell(&g_pacman);

    TEST_ASSERT_EQUAL(DIRECTION_NONE, pacman_get_direction(&g_pacman));
    TEST_ASSERT_FALSE(pacman_advance(&g_pacman, &g_playfield));
    TEST_ASSERT_TRUE(playfield_are_cells_equal(start, pacman_get_cell(&g_pacman)));
}

void test_intent_is_taken_up_at_the_next_move_not_immediately(void)
{
    prv_place_pacman_in_the_open();

    pacman_set_intent(&g_pacman, DIRECTION_EAST);

    /* Setting the intent must not have moved him — input and movement are separate
     * (§10.1), which is what lets the player press early. */
    TEST_ASSERT_TRUE(playfield_are_cells_equal(prv_cell(OPEN_CELL_X, OPEN_CELL_Y),
                                               pacman_get_cell(&g_pacman)));

    TEST_ASSERT_TRUE(pacman_advance(&g_pacman, &g_playfield));
    TEST_ASSERT_TRUE(playfield_are_cells_equal(prv_cell(OPEN_CELL_X + 1, OPEN_CELL_Y),
                                               pacman_get_cell(&g_pacman)));
    TEST_ASSERT_EQUAL(DIRECTION_EAST, pacman_get_direction(&g_pacman));
}

void test_pacman_keeps_going_without_new_input(void)
{
    prv_place_pacman_in_the_open();

    pacman_set_intent(&g_pacman, DIRECTION_EAST);
    (void)pacman_advance(&g_pacman, &g_playfield);
    (void)pacman_advance(&g_pacman, &g_playfield);

    TEST_ASSERT_TRUE(playfield_are_cells_equal(prv_cell(OPEN_CELL_X + 2, OPEN_CELL_Y),
                                               pacman_get_cell(&g_pacman)));
}

void test_an_impossible_turn_is_remembered_until_it_becomes_possible(void)
{
    /* The rule that makes the controls forgiving, and the one worth proving: ask for a
     * turn into a wall, keep hands off the controls, and it must happen by itself at the
     * first cell where it becomes possible. */
    pacman_reset(&g_pacman, prv_cell(CORRIDOR_START_X, OPEN_CELL_Y));
    pacman_set_intent(&g_pacman, DIRECTION_EAST);
    (void)pacman_advance(&g_pacman, &g_playfield);

    /* Now on a cell with a wall to the north. */
    TEST_ASSERT_TRUE(playfield_are_cells_equal(prv_cell(WALLED_NORTH_X, OPEN_CELL_Y),
                                               pacman_get_cell(&g_pacman)));

    pacman_set_intent(&g_pacman, DIRECTION_NORTH);
    (void)pacman_advance(&g_pacman, &g_playfield);

    /* North was a wall, so he carried on east — and the request is still pending. */
    TEST_ASSERT_TRUE(playfield_are_cells_equal(prv_cell(OPEN_NORTH_X, OPEN_CELL_Y),
                                               pacman_get_cell(&g_pacman)));
    TEST_ASSERT_EQUAL(DIRECTION_EAST, pacman_get_direction(&g_pacman));

    /* No further input: the pending turn fires on its own, because here north is open. */
    (void)pacman_advance(&g_pacman, &g_playfield);

    TEST_ASSERT_EQUAL(DIRECTION_NORTH, pacman_get_direction(&g_pacman));
    TEST_ASSERT_TRUE(playfield_are_cells_equal(prv_cell(OPEN_NORTH_X, OPEN_CELL_Y - 1),
                                               pacman_get_cell(&g_pacman)));
}

void test_pacman_stops_against_a_wall_and_waits_facing_it(void)
{
    /* §10.1's "stopped against a wall until the direction changes": he was moving east,
     * the corridor ends, and he holds position still pointing east. */
    pacman_reset(&g_pacman, prv_cell(CORRIDOR_LAST_X - 1, OPEN_CELL_Y));
    pacman_set_intent(&g_pacman, DIRECTION_EAST);

    TEST_ASSERT_TRUE(pacman_advance(&g_pacman, &g_playfield));
    TEST_ASSERT_TRUE(playfield_are_cells_equal(prv_cell(CORRIDOR_LAST_X, OPEN_CELL_Y),
                                               pacman_get_cell(&g_pacman)));

    TEST_ASSERT_FALSE(pacman_advance(&g_pacman, &g_playfield));
    TEST_ASSERT_TRUE(playfield_are_cells_equal(prv_cell(CORRIDOR_LAST_X, OPEN_CELL_Y),
                                               pacman_get_cell(&g_pacman)));
    TEST_ASSERT_EQUAL(DIRECTION_EAST, pacman_get_direction(&g_pacman));
}

void test_a_blocked_intent_does_not_change_the_facing(void)
{
    /* §10.1 is precise: the queued direction becomes current only *if* it is not
     * blocked. So asking for the impossible must leave the facing alone rather than
     * turning him into the wall. */
    pacman_reset(&g_pacman, prv_cell(CORRIDOR_START_X, OPEN_CELL_Y));
    pacman_set_intent(&g_pacman, DIRECTION_EAST);
    (void)pacman_advance(&g_pacman, &g_playfield);

    /* Standing where north is a wall. */
    TEST_ASSERT_TRUE(playfield_are_cells_equal(prv_cell(WALLED_NORTH_X, OPEN_CELL_Y),
                                               pacman_get_cell(&g_pacman)));

    pacman_set_intent(&g_pacman, DIRECTION_NORTH);
    (void)pacman_advance(&g_pacman, &g_playfield);

    TEST_ASSERT_EQUAL(DIRECTION_EAST, pacman_get_direction(&g_pacman));
}

void test_an_empty_intent_cannot_cancel_a_pending_turn(void)
{
    prv_place_pacman_in_the_open();

    pacman_set_intent(&g_pacman, DIRECTION_EAST);
    pacman_set_intent(&g_pacman, DIRECTION_NONE);

    TEST_ASSERT_TRUE(pacman_advance(&g_pacman, &g_playfield));
    TEST_ASSERT_EQUAL(DIRECTION_EAST, pacman_get_direction(&g_pacman));
}

void test_pacman_may_reverse_freely(void)
{
    /* Unlike a ghost (§10.1), Pacman has no no-reverse rule. */
    prv_place_pacman_in_the_open();

    pacman_set_intent(&g_pacman, DIRECTION_EAST);
    (void)pacman_advance(&g_pacman, &g_playfield);

    pacman_set_intent(&g_pacman, DIRECTION_WEST);
    (void)pacman_advance(&g_pacman, &g_playfield);

    TEST_ASSERT_TRUE(playfield_are_cells_equal(prv_cell(OPEN_CELL_X, OPEN_CELL_Y),
                                               pacman_get_cell(&g_pacman)));
}

void test_pacman_travels_through_a_tunnel(void)
{
    const cell_t mouth = prv_cell(0, PLAYFIELD_HEIGHT / 2);

    pacman_reset(&g_pacman, mouth);
    pacman_set_intent(&g_pacman, DIRECTION_WEST);

    TEST_ASSERT_TRUE(pacman_advance(&g_pacman, &g_playfield));
    TEST_ASSERT_TRUE(playfield_are_cells_equal(prv_cell(PLAYFIELD_WIDTH - 1, mouth.y),
                                               pacman_get_cell(&g_pacman)));
}

void test_pacman_escapes_his_start_pocket(void)
{
    /* §10.2 promises the start pocket is not a trap. In the reference maze it opens both
     * north and south, the southern one being the vertical tunnel — so this also
     * exercises FR-012 from a real position. */
    pacman_set_intent(&g_pacman, DIRECTION_SOUTH);

    TEST_ASSERT_TRUE(pacman_advance(&g_pacman, &g_playfield));
}

void test_resetting_pacman_clears_his_intent(void)
{
    prv_place_pacman_in_the_open();
    pacman_set_intent(&g_pacman, DIRECTION_EAST);

    pacman_reset(&g_pacman, prv_cell(OPEN_CELL_X, OPEN_CELL_Y));

    TEST_ASSERT_FALSE(pacman_advance(&g_pacman, &g_playfield));
    TEST_ASSERT_EQUAL(DIRECTION_NONE, pacman_get_direction(&g_pacman));
}

/* --- preconditions ------------------------------------------------------- */

void test_a_null_pacman_asserts(void)
{
    ASSERT_PROBE_EXPECT(pacman_set_intent(NULL, DIRECTION_EAST), "inout_pacman != NULL");
}

void test_a_null_agent_asserts(void)
{
    ASSERT_PROBE_EXPECT(agent_place(NULL, prv_cell(0, 0), DIRECTION_NONE), "inout_agent != NULL");
}
