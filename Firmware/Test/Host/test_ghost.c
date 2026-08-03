/*
 * Unit tests for App/ghost.
 *
 * The targeting of §10.4 is what these are for. Four personalities is the whole reason
 * Pacman is interesting rather than a chase by four identical blobs, and every one of them
 * is a formula that is easy to get subtly wrong and impossible to verify by playing —
 * Inky's doubled vector especially. A wrong sign there still produces a ghost that chases
 * plausibly.
 *
 * Targets are checked directly rather than through movement, because that is where the
 * personality lives; how the step is then chosen belongs to ghost_path and is tested
 * there.
 */
#include <stdbool.h>
#include <stdint.h>

/* Ceedling links only what this file includes, not transitively: the ghost sits on
 * agent, which sits on playfield. */
#include "agent.h"
#include "assert_probe.h"
#include "custom_assert.h"
#include "ghost.h"
#include "ghost_path.h"
#include "msg.h"
#include "playfield.h"
#include "unity.h"

#define LEVEL_1           (1U)

/* Pacman parked mid-maze facing east, with Blinky somewhere behind him — the arrangement
 * §10.4's formulas are written about. */
#define PACMAN_X          (5)
#define PACMAN_Y          (5)
#define BLINKY_X          (2)
#define BLINKY_Y          (5)

/* A cell on the maze's open row 5 whose only neighbours are east and west, so "it did not
 * turn around" is a statement about the rule and not about which of three ways it picked. */
#define CORRIDOR_X        (3)
#define CORRIDOR_Y        (5)

#define PINKY_LOOK_AHEAD  (4)
#define INKY_LOOK_AHEAD   (2)
/* Eight cells, squared — the arcade's figure, compared the way the game compares it. */
#define CLYDE_SHY_SQUARED (8 * 8)

static playfield_t g_playfield;
static ghost_t g_ghost;

static cell_t prv_cell(int16_t in_x, int16_t in_y)
{
    const cell_t cell = {in_x, in_y};

    return cell;
}

static cell_t prv_pacman_cell(void)
{
    return prv_cell(PACMAN_X, PACMAN_Y);
}

static cell_t prv_blinky_cell(void)
{
    return prv_cell(BLINKY_X, BLINKY_Y);
}

/* Target for a chasing ghost of the given personality, with the standard arrangement. */
static cell_t prv_chase_target(ghost_personality_e in_personality, cell_t in_ghost_cell)
{
    ghost_reset(&g_ghost, in_personality, in_ghost_cell, false);
    ghost_set_mode(&g_ghost, GHOST_MODE_CHASE);

    return ghost_get_target(&g_ghost, &g_playfield, prv_pacman_cell(), DIRECTION_EAST, prv_blinky_cell());
}

void setUp(void)
{
    assert_probe_begin();
    playfield_load(&g_playfield);
    ghost_reset(&g_ghost, GHOST_BLINKY, prv_cell(CORRIDOR_X, CORRIDOR_Y), false);
}

void tearDown(void)
{
    assert_probe_end();
}

/* --- chase targets (§10.4) ----------------------------------------------- */

void test_blinky_aims_straight_at_pacman(void)
{
    const cell_t target = prv_chase_target(GHOST_BLINKY, prv_cell(1, 1));

    TEST_ASSERT_TRUE(playfield_are_cells_equal(prv_pacman_cell(), target));
}

void test_pinky_aims_two_cells_ahead_of_pacman(void)
{
    const cell_t target = prv_chase_target(GHOST_PINKY, prv_cell(1, 1));

    TEST_ASSERT_TRUE(playfield_are_cells_equal(prv_cell(PACMAN_X + PINKY_LOOK_AHEAD, PACMAN_Y), target));
}

void test_pinky_follows_pacmans_facing_not_his_position(void)
{
    /* The ambush only works if the look-ahead turns with him. */
    ghost_reset(&g_ghost, GHOST_PINKY, prv_cell(1, 1), false);
    ghost_set_mode(&g_ghost, GHOST_MODE_CHASE);

    TEST_ASSERT_TRUE(playfield_are_cells_equal(
        prv_cell(PACMAN_X, PACMAN_Y - PINKY_LOOK_AHEAD),
        ghost_get_target(&g_ghost, &g_playfield, prv_pacman_cell(), DIRECTION_NORTH, prv_blinky_cell())));
}

void test_inky_aims_past_pacman_along_the_line_from_blinky(void)
{
    /* §10.4: pivot one ahead of Pacman, then double the vector from Blinky to the pivot.
     * Worked through with the fixture: pivot is (6,3); Blinky is (2,3); so the target is
     * (6,3) + ((6,3) - (2,3)) = (10,3). A flipped sign would give (2,3) instead — a ghost
     * that trails Blinky rather than flanking, which looks perfectly reasonable in play. */
    const cell_t pivot = prv_cell(PACMAN_X + INKY_LOOK_AHEAD, PACMAN_Y);
    const cell_t expected =
        prv_cell((int16_t)(pivot.x + (pivot.x - BLINKY_X)), (int16_t)(pivot.y + (pivot.y - BLINKY_Y)));
    const cell_t target = prv_chase_target(GHOST_INKY, prv_cell(1, 5));

    TEST_ASSERT_TRUE(playfield_are_cells_equal(expected, target));
    /* And it really is past Pacman, not behind him. */
    TEST_ASSERT_GREATER_THAN_INT16(prv_pacman_cell().x, target.x);
}

void test_inky_aims_at_pacman_when_blinky_sits_on_the_pivot(void)
{
    /* Degenerate but reachable: the doubled vector collapses to the pivot itself. Worth
     * pinning down so it cannot become a divide-by-zero or a wild target later. */
    const cell_t pivot = prv_cell(PACMAN_X + INKY_LOOK_AHEAD, PACMAN_Y);

    ghost_reset(&g_ghost, GHOST_INKY, prv_cell(1, 5), false);
    ghost_set_mode(&g_ghost, GHOST_MODE_CHASE);

    TEST_ASSERT_TRUE(playfield_are_cells_equal(
        pivot, ghost_get_target(&g_ghost, &g_playfield, prv_pacman_cell(), DIRECTION_EAST, pivot)));
}

void test_clyde_hunts_pacman_from_far_away(void)
{
    /* Fifteen rows below Pacman and four columns across: comfortably past the eight cells
     * at which Clyde stops being shy. */
    const cell_t far_cell = prv_cell(1, 20);
    const cell_t target = prv_chase_target(GHOST_CLYDE, far_cell);

    TEST_ASSERT_GREATER_THAN_UINT32(CLYDE_SHY_SQUARED, playfield_get_squared_distance(far_cell, prv_pacman_cell()));
    TEST_ASSERT_TRUE(playfield_are_cells_equal(prv_pacman_cell(), target));
}

void test_clyde_breaks_off_for_his_corner_when_close(void)
{
    /* This is the rule that makes Clyde the one who lets you past, so it is worth proving
     * he actually switches rather than merely being slower. */
    const cell_t near_cell = prv_cell(PACMAN_X + 1, PACMAN_Y);
    const cell_t target = prv_chase_target(GHOST_CLYDE, near_cell);

    TEST_ASSERT_LESS_THAN_UINT32(CLYDE_SHY_SQUARED, playfield_get_squared_distance(near_cell, prv_pacman_cell()));
    TEST_ASSERT_TRUE(playfield_are_cells_equal(playfield_get_scatter_target(GHOST_CLYDE), target));
}

void test_each_personality_gets_its_own_corner(void)
{
    for (uint8_t personality = 0U; personality < GHOST_COUNT; ++personality)
    {
        ghost_reset(&g_ghost, (ghost_personality_e)personality, prv_cell(CORRIDOR_X, CORRIDOR_Y), false);

        for (uint8_t other = 0U; other < personality; ++other)
        {
            TEST_ASSERT_FALSE(playfield_are_cells_equal(playfield_get_scatter_target(personality),
                                                        playfield_get_scatter_target(other)));
        }
    }
}

/* --- scatter mode -------------------------------------------------------- */

void test_scattering_ignores_pacman_entirely(void)
{
    ghost_reset(&g_ghost, GHOST_BLINKY, prv_cell(1, 1), false);
    ghost_set_mode(&g_ghost, GHOST_MODE_SCATTER);

    TEST_ASSERT_TRUE(playfield_are_cells_equal(
        playfield_get_scatter_target(GHOST_BLINKY),
        ghost_get_target(&g_ghost, &g_playfield, prv_pacman_cell(), DIRECTION_EAST, prv_blinky_cell())));
}

void test_a_reset_ghost_starts_scattering_and_unfrightened(void)
{
    TEST_ASSERT_EQUAL(GHOST_MODE_SCATTER, ghost_get_mode(&g_ghost));
    TEST_ASSERT_FALSE(ghost_is_frightened(&g_ghost));
}

/* --- frightened mode (§10.5) --------------------------------------------- */

void test_a_frightened_ghost_runs_away_from_pacman(void)
{
    const cell_t start = prv_cell(PACMAN_X + 2, PACMAN_Y);

    ghost_reset(&g_ghost, GHOST_BLINKY, start, false);
    ghost_set_mode(&g_ghost, GHOST_MODE_FRIGHTENED);

    TEST_ASSERT_TRUE(ghost_is_frightened(&g_ghost));
    TEST_ASSERT_TRUE(ghost_advance(&g_ghost, &g_playfield, prv_pacman_cell(), DIRECTION_EAST, prv_blinky_cell()));

    TEST_ASSERT_GREATER_THAN_UINT32(playfield_get_squared_distance(start, prv_pacman_cell()),
                                    playfield_get_squared_distance(ghost_get_cell(&g_ghost), prv_pacman_cell()));
}

void test_a_chasing_ghost_closes_in(void)
{
    const cell_t start = prv_cell(PACMAN_X + 3, PACMAN_Y);

    ghost_reset(&g_ghost, GHOST_BLINKY, start, false);
    ghost_set_mode(&g_ghost, GHOST_MODE_CHASE);

    TEST_ASSERT_TRUE(ghost_advance(&g_ghost, &g_playfield, prv_pacman_cell(), DIRECTION_EAST, prv_blinky_cell()));

    TEST_ASSERT_LESS_THAN_UINT32(playfield_get_squared_distance(start, prv_pacman_cell()),
                                 playfield_get_squared_distance(ghost_get_cell(&g_ghost), prv_pacman_cell()));
}

void test_being_eaten_returns_the_ghost_to_the_pen_unfrightened(void)
{
    const cell_t pen_cell = playfield_get_ghost_start(&g_playfield, (uint8_t)GHOST_PINKY);

    ghost_reset(&g_ghost, GHOST_BLINKY, prv_cell(1, 1), false);
    ghost_set_mode(&g_ghost, GHOST_MODE_FRIGHTENED);

    ghost_send_to_pen(&g_ghost, pen_cell);

    TEST_ASSERT_TRUE(playfield_are_cells_equal(pen_cell, ghost_get_cell(&g_ghost)));
    TEST_ASSERT_FALSE(ghost_is_frightened(&g_ghost));
}

/* --- the reversal rule (§10.1) ------------------------------------------- */

void test_a_ghost_does_not_turn_around_of_its_own_accord(void)
{
    /* Walk it one step so it has a facing, then put the target behind it: it must carry on
     * rather than double back. */
    const cell_t start = prv_cell(CORRIDOR_X, CORRIDOR_Y);
    direction_e facing;

    ghost_reset(&g_ghost, GHOST_BLINKY, start, false);
    ghost_set_mode(&g_ghost, GHOST_MODE_CHASE);
    (void)ghost_advance(&g_ghost, &g_playfield, prv_pacman_cell(), DIRECTION_EAST, prv_blinky_cell());

    facing = ghost_get_direction(&g_ghost);

    /* Now aim it back the way it came. */
    (void)ghost_advance(&g_ghost, &g_playfield, start, DIRECTION_EAST, prv_blinky_cell());

    TEST_ASSERT_NOT_EQUAL(playfield_get_opposite_direction(facing), ghost_get_direction(&g_ghost));
}

void test_a_mode_change_earns_exactly_one_reversal(void)
{
    const cell_t start = prv_cell(CORRIDOR_X, CORRIDOR_Y);
    direction_e facing;

    ghost_reset(&g_ghost, GHOST_BLINKY, start, false);
    ghost_set_mode(&g_ghost, GHOST_MODE_CHASE);
    (void)ghost_advance(&g_ghost, &g_playfield, prv_pacman_cell(), DIRECTION_EAST, prv_blinky_cell());

    facing = ghost_get_direction(&g_ghost);

    /* Frightened is a mode change, and Pacman is now right where it came from, so fleeing
     * means turning around — which the exemption must permit exactly once. */
    ghost_set_mode(&g_ghost, GHOST_MODE_FRIGHTENED);
    (void)ghost_advance(&g_ghost, &g_playfield, ghost_get_cell(&g_ghost), DIRECTION_EAST, prv_blinky_cell());

    TEST_ASSERT_EQUAL(playfield_get_opposite_direction(facing), ghost_get_direction(&g_ghost));
}

void test_setting_the_same_mode_again_earns_nothing(void)
{
    /* The orchestrator drives the mode every tick, so a no-op change must not hand out a
     * reversal — otherwise the ghosts jitter on the spot. */
    const cell_t start = prv_cell(CORRIDOR_X, CORRIDOR_Y);
    direction_e facing;

    ghost_reset(&g_ghost, GHOST_BLINKY, start, false);
    ghost_set_mode(&g_ghost, GHOST_MODE_CHASE);
    (void)ghost_advance(&g_ghost, &g_playfield, prv_pacman_cell(), DIRECTION_EAST, prv_blinky_cell());

    facing = ghost_get_direction(&g_ghost);

    ghost_set_mode(&g_ghost, GHOST_MODE_CHASE);
    (void)ghost_advance(&g_ghost, &g_playfield, start, DIRECTION_EAST, prv_blinky_cell());

    TEST_ASSERT_NOT_EQUAL(playfield_get_opposite_direction(facing), ghost_get_direction(&g_ghost));
}

/* --- the ghost house (§10.4) ---------------------------------------------- */

void test_a_ghost_out_in_the_maze_cannot_get_back_into_the_house(void)
{
    /* The Dossier is flat about it: once a ghost leaves, it cannot reenter unless it is
     * first captured. Without this the four of them drift back into their cage and sit
     * there, which is what the panel was showing. */
    const cell_t exit_cell = playfield_get_house_exit(&g_playfield);
    const cell_t gate = playfield_step(exit_cell, DIRECTION_SOUTH);
    direction_e chosen;

    ghost_reset(&g_ghost, GHOST_BLINKY, exit_cell, false);
    ghost_set_mode(&g_ghost, GHOST_MODE_CHASE);

    TEST_ASSERT_FALSE(ghost_is_in_house(&g_ghost));

    /* Aim it straight down the gate, which is the one direction it must refuse. */
    chosen = ghost_path_find_step_towards(&g_playfield, exit_cell, gate, DIRECTION_NONE, false);

    TEST_ASSERT_NOT_EQUAL(DIRECTION_SOUTH, chosen);
}

void test_a_ghost_in_the_house_heads_for_the_gate_and_gets_out(void)
{
    /* And the other half of the rule: from inside, the gate is open and the target is the
     * exit rather than its usual one — the house is enclosed, so a chase target would leave
     * it pacing the floor. */
    const cell_t start = playfield_get_ghost_start(&g_playfield, (uint8_t)GHOST_PINKY);
    uint8_t steps = 0U;

    ghost_reset(&g_ghost, GHOST_PINKY, start, true);
    ghost_set_mode(&g_ghost, GHOST_MODE_CHASE);

    /* Reset leaves it shut in until its dot limit comes up; the game is what lets it out. */
    TEST_ASSERT_TRUE(ghost_is_waiting_in_house(&g_ghost));
    ghost_release_from_house(&g_ghost);

    TEST_ASSERT_TRUE(playfield_are_cells_equal(
        playfield_get_house_exit(&g_playfield),
        ghost_get_target(&g_ghost, &g_playfield, prv_pacman_cell(), DIRECTION_EAST, prv_blinky_cell())));

    while (playfield_is_house(&g_playfield, ghost_get_cell(&g_ghost)) && (steps < 20U))
    {
        (void)ghost_advance(&g_ghost, &g_playfield, prv_pacman_cell(), DIRECTION_EAST, prv_blinky_cell());
        ++steps;
    }

    TEST_ASSERT_FALSE(playfield_is_house(&g_playfield, ghost_get_cell(&g_ghost)));
}

void test_being_eaten_puts_a_ghost_back_inside_the_house(void)
{
    /* The one way in, and the reason the gate opens both ways rather than one. */
    const cell_t revive = playfield_get_ghost_start(&g_playfield, (uint8_t)GHOST_PINKY);

    ghost_reset(&g_ghost, GHOST_BLINKY, playfield_get_house_exit(&g_playfield), false);
    ghost_send_to_pen(&g_ghost, revive);

    TEST_ASSERT_TRUE(ghost_is_in_house(&g_ghost));
    TEST_ASSERT_TRUE(playfield_are_cells_equal(revive, ghost_get_cell(&g_ghost)));
}

/* --- preconditions ------------------------------------------------------- */

void test_an_unknown_personality_asserts(void)
{
    ASSERT_PROBE_EXPECT(ghost_reset(&g_ghost, GHOST_COUNT, prv_cell(1, 1), false), "in_personality < GHOST_COUNT");
}

void test_a_null_ghost_asserts(void)
{
    ASSERT_PROBE_EXPECT(ghost_set_mode(NULL, GHOST_MODE_CHASE), "inout_ghost != NULL");
}
