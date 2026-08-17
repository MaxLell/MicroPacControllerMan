/*
 * Unit tests for App/pacman_ai (VT-UNIT-009).
 *
 * Two of these are worth more than the rest and are the reason the module exists.
 *
 * The first measures the observation rather than eyeballing it: a distance the module reports
 * is compared against a breadth-first search written independently here, over the same rules.
 * A feature extractor that is subtly wrong about distance produces an agent that is subtly bad
 * at the game, and nothing else in the system would notice.
 *
 * The second is the rotation invariance the whole design rests on
 * ([M6 §3/§4](../../../Docu/Design/M6-Pacman-AI.md)). Every level's maze is generated, so a
 * policy is only transferable if what the agent sees turns with it: the group of features
 * belonging to whichever action points north must be the same however Pacman is facing. If that
 * ever breaks, training still converges — on a policy that cannot play a maze it has not seen.
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "ai_weights.h"
#include "assert_probe.h"
#include "custom_assert.h"
#include "maze_gen.h"
#include "msg.h"
#include "neural_net.h"
#include "pacman_ai.h"
#include "playfield.h"
#include "unity.h"

#define SEED_UNDER_TEST (1000U)

static playfield_t g_playfield;
static msg_game_state_t g_state;
static float g_features[PACMAN_AI_FEATURE_COUNT];

/* --- an independent breadth-first search ---------------------------------- */

/* Deliberately not a copy of the module's: this one keeps a plain distance grid, fans out in a
 * different direction order, and knows nothing about "first step". If the two agree on a
 * distance, that distance is a property of the maze rather than of one implementation. */
static uint16_t prv_reference_distance_to_pellet(cell_t in_origin)
{
    static uint16_t distance[PLAYFIELD_HEIGHT][PLAYFIELD_WIDTH];
    static cell_t queue[PLAYFIELD_WIDTH * PLAYFIELD_HEIGHT];
    static const direction_e k_order[] = {DIRECTION_WEST, DIRECTION_EAST, DIRECTION_SOUTH, DIRECTION_NORTH};

    for (int16_t y = 0; y < PLAYFIELD_HEIGHT; ++y)
    {
        for (int16_t x = 0; x < PLAYFIELD_WIDTH; ++x)
        {
            distance[y][x] = UINT16_MAX;
        }
    }

    uint16_t head = 0U;
    uint16_t tail = 0U;

    distance[in_origin.y][in_origin.x] = 0U;
    queue[tail++] = in_origin;

    uint16_t best = UINT16_MAX;

    while (head < tail)
    {
        const cell_t current = queue[head++];
        const uint16_t current_distance = distance[current.y][current.x];

        if ((current_distance > 0U) && (current_distance < best)
            && msg_cell_bitmap_get(g_state.has_pellet, (uint8_t)current.x, (uint8_t)current.y))
        {
            best = current_distance;
        }

        for (uint8_t index = 0U; index < 4U; ++index)
        {
            const cell_t next = playfield_step(current, k_order[index]);

            if (!playfield_is_walkable(&g_playfield, next) || playfield_is_house(&g_playfield, next)
                || playfield_is_gate(&g_playfield, next))
            {
                continue;
            }

            if (distance[next.y][next.x] != UINT16_MAX)
            {
                continue;
            }

            distance[next.y][next.x] = (uint16_t)(current_distance + 1U);
            queue[tail++] = next;
        }
    }

    return best;
}

/* --- fixtures ------------------------------------------------------------- */

static cell_t prv_make_cell(int16_t in_x, int16_t in_y)
{
    const cell_t cell = {in_x, in_y};

    return cell;
}

/* A state that matches the loaded maze: every pellet present, the ghosts parked out of the way
 * unless a test moves them. Built by hand rather than taken from `game`, so that these tests
 * describe the observation and not the game's current opening move. */
static void prv_build_state(cell_t in_pacman, direction_e in_facing)
{
    memset(&g_state, 0, sizeof(g_state));

    g_state.pacman.column = (uint8_t)in_pacman.x;
    g_state.pacman.row = (uint8_t)in_pacman.y;
    g_state.pacman.direction = (uint8_t)in_facing;
    g_state.pacman.progress = MSG_CELL_PROGRESS_ARRIVED;

    for (int16_t y = 0; y < PLAYFIELD_HEIGHT; ++y)
    {
        for (int16_t x = 0; x < PLAYFIELD_WIDTH; ++x)
        {
            const playfield_pellet_e pellet = playfield_get_pellet(&g_playfield, prv_make_cell(x, y));

            msg_cell_bitmap_set(g_state.has_pellet, (uint8_t)x, (uint8_t)y, pellet != PLAYFIELD_PELLET_NONE);
            msg_cell_bitmap_set(g_state.is_power, (uint8_t)x, (uint8_t)y, pellet == PLAYFIELD_PELLET_POWER);
        }
    }

    /* Every ghost in its own start cell, which for three of them is inside the house. */
    for (uint8_t index = 0U; index < MSG_GHOST_COUNT; ++index)
    {
        const cell_t start = playfield_get_ghost_start(&g_playfield, index);

        g_state.ghosts[index].column = (uint8_t)start.x;
        g_state.ghosts[index].row = (uint8_t)start.y;
        g_state.ghosts[index].direction = (uint8_t)DIRECTION_WEST;
        g_state.ghosts[index].progress = MSG_CELL_PROGRESS_ARRIVED;
    }

    g_state.lives = 3U;
    g_state.level = 1U;
}

void setUp(void)
{
    playfield_map_t map;

    assert_probe_begin();

    maze_gen_generate(&map, SEED_UNDER_TEST);
    playfield_load_from_map(&g_playfield, &map);

    prv_build_state(playfield_get_pacman_start(&g_playfield), DIRECTION_NONE);
    memset(g_features, 0, sizeof(g_features));
}

void tearDown(void)
{
}

/* Slot `in_slot` of whichever action currently points `in_direction`. */
static float prv_feature_for_direction(direction_e in_direction, uint8_t in_slot, direction_e in_facing)
{
    for (uint8_t action = 0U; action < (uint8_t)PACMAN_AI_ACTION_COUNT; ++action)
    {
        if (pacman_ai_action_to_direction((pacman_ai_action_e)action, in_facing) == in_direction)
        {
            return g_features[(action * PACMAN_AI_FEATURES_PER_ACTION) + in_slot];
        }
    }

    TEST_FAIL_MESSAGE("no action points that way");

    return 0.0F;
}

/* --- the observation is measured, not eyeballed --------------------------- */

void test_the_nearest_pellet_distance_matches_an_independent_search(void)
{
    /* Checked from several cells, because one cell could agree by luck — and from cells with
     * different numbers of ways out, which is where a "first step" mistake would show. */
    static const uint8_t k_offsets[] = {0U, 1U, 2U, 3U, 5U, 8U};

    const cell_t start = playfield_get_pacman_start(&g_playfield);
    uint8_t checked = 0U;

    for (uint8_t index = 0U; index < (sizeof(k_offsets) / sizeof(k_offsets[0])); ++index)
    {
        const cell_t origin = prv_make_cell((int16_t)(start.x - k_offsets[index]), start.y);

        if (!playfield_is_walkable(&g_playfield, origin))
        {
            continue;
        }

        prv_build_state(origin, DIRECTION_WEST);
        pacman_ai_get_features(&g_state, &g_playfield, g_features);

        /* The nearest pellet overall is the smallest of the four per-direction slots. */
        float nearest = PACMAN_AI_DISTANCE_NONE;
        for (uint8_t action = 0U; action < (uint8_t)PACMAN_AI_ACTION_COUNT; ++action)
        {
            const float value = g_features[(action * PACMAN_AI_FEATURES_PER_ACTION) + 1U];

            if (value < nearest)
            {
                nearest = value;
            }
        }

        const uint16_t reference = prv_reference_distance_to_pellet(origin);
        const float expected = (reference >= (uint16_t)PACMAN_AI_DISTANCE_SCALE)
                                   ? PACMAN_AI_DISTANCE_NONE
                                   : ((float)reference / (float)PACMAN_AI_DISTANCE_SCALE);

        TEST_ASSERT_FLOAT_WITHIN(0.0001F, expected, nearest);
        checked++;
    }

    TEST_ASSERT_GREATER_THAN_UINT8(2U, checked);
}

void test_a_wall_is_reported_as_closed_and_a_corridor_as_open(void)
{
    const cell_t start = playfield_get_pacman_start(&g_playfield);

    prv_build_state(start, DIRECTION_NONE);
    pacman_ai_get_features(&g_state, &g_playfield, g_features);

    static const direction_e k_directions[] = {DIRECTION_NORTH, DIRECTION_SOUTH, DIRECTION_EAST, DIRECTION_WEST};
    uint8_t open_ways = 0U;

    for (uint8_t index = 0U; index < 4U; ++index)
    {
        const cell_t next = playfield_step(start, k_directions[index]);
        const bool is_open = playfield_is_walkable(&g_playfield, next) && !playfield_is_house(&g_playfield, next)
                             && !playfield_is_gate(&g_playfield, next);
        const float reported = prv_feature_for_direction(k_directions[index], 0U, DIRECTION_NONE);

        TEST_ASSERT_EQUAL_FLOAT(is_open ? 1.0F : 0.0F, reported);
        open_ways = (uint8_t)(open_ways + (is_open ? 1U : 0U));
    }

    /* The arcade sets Pacman down walled above and below, and the generator keeps that cell —
     * so if this ever reports four ways out, the fixture is not the maze it thinks it is. */
    TEST_ASSERT_GREATER_THAN_UINT8(0U, open_ways);
    TEST_ASSERT_LESS_THAN_UINT8(4U, open_ways);
}

/* --- the invariance the design rests on ---------------------------------- */

void test_what_the_agent_sees_turns_with_it(void)
{
    static const direction_e k_facings[] = {DIRECTION_NORTH, DIRECTION_SOUTH, DIRECTION_EAST, DIRECTION_WEST};
    static const direction_e k_absolute[] = {DIRECTION_NORTH, DIRECTION_SOUTH, DIRECTION_EAST, DIRECTION_WEST};

    float reference[4][PACMAN_AI_FEATURES_PER_ACTION];
    float reference_globals[PACMAN_AI_GLOBAL_FEATURES];

    const cell_t start = playfield_get_pacman_start(&g_playfield);

    for (uint8_t facing_index = 0U; facing_index < 4U; ++facing_index)
    {
        prv_build_state(start, k_facings[facing_index]);
        pacman_ai_get_features(&g_state, &g_playfield, g_features);

        for (uint8_t absolute = 0U; absolute < 4U; ++absolute)
        {
            for (uint8_t slot = 0U; slot < PACMAN_AI_FEATURES_PER_ACTION; ++slot)
            {
                const float value = prv_feature_for_direction(k_absolute[absolute], slot, k_facings[facing_index]);

                if (facing_index == 0U)
                {
                    reference[absolute][slot] = value;
                }
                else
                {
                    /* The same fact about the same absolute direction, whichever way Pacman
                     * happens to be pointing. */
                    TEST_ASSERT_EQUAL_FLOAT(reference[absolute][slot], value);
                }
            }
        }

        const uint8_t global = (uint8_t)PACMAN_AI_ACTION_COUNT * PACMAN_AI_FEATURES_PER_ACTION;

        for (uint8_t index = 0U; index < PACMAN_AI_GLOBAL_FEATURES; ++index)
        {
            if (facing_index == 0U)
            {
                reference_globals[index] = g_features[global + index];
            }
            else
            {
                TEST_ASSERT_EQUAL_FLOAT(reference_globals[index], g_features[global + index]);
            }
        }
    }
}

void test_the_four_actions_are_four_different_directions_from_every_facing(void)
{
    static const direction_e k_facings[] = {DIRECTION_NONE, DIRECTION_NORTH, DIRECTION_SOUTH, DIRECTION_EAST,
                                            DIRECTION_WEST};

    for (uint8_t index = 0U; index < (sizeof(k_facings) / sizeof(k_facings[0])); ++index)
    {
        bool seen[5] = {false, false, false, false, false};

        for (uint8_t action = 0U; action < (uint8_t)PACMAN_AI_ACTION_COUNT; ++action)
        {
            const direction_e direction = pacman_ai_action_to_direction((pacman_ai_action_e)action, k_facings[index]);

            TEST_ASSERT_NOT_EQUAL(DIRECTION_NONE, direction);
            TEST_ASSERT_FALSE(seen[(uint8_t)direction]);
            seen[(uint8_t)direction] = true;
        }
    }
}

void test_forward_is_the_way_pacman_faces_and_back_is_the_way_home(void)
{
    static const direction_e k_facings[] = {DIRECTION_NORTH, DIRECTION_SOUTH, DIRECTION_EAST, DIRECTION_WEST};

    for (uint8_t index = 0U; index < 4U; ++index)
    {
        TEST_ASSERT_EQUAL_INT(k_facings[index],
                              pacman_ai_action_to_direction(PACMAN_AI_ACTION_FORWARD, k_facings[index]));
        TEST_ASSERT_EQUAL_INT(playfield_get_opposite_direction(k_facings[index]),
                              pacman_ai_action_to_direction(PACMAN_AI_ACTION_BACK, k_facings[index]));
    }

    /* A level begins with Pacman stationary, so this is a state the game really produces. */
    TEST_ASSERT_EQUAL_INT(DIRECTION_NORTH, pacman_ai_action_to_direction(PACMAN_AI_ACTION_FORWARD, DIRECTION_NONE));
}

/* --- what the agent is and is not told ----------------------------------- */

void test_a_ghost_waiting_in_the_house_reports_as_absent(void)
{
    /* Documented in the module: no walkable route exists to a cell Pacman may not enter, and a
     * distance through the gate would be a fiction. This test exists so the behaviour is a
     * decision on record rather than a surprise. */
    prv_build_state(playfield_get_pacman_start(&g_playfield), DIRECTION_WEST);
    pacman_ai_get_features(&g_state, &g_playfield, g_features);

    uint8_t housed = 0U;
    for (uint8_t index = 0U; index < MSG_GHOST_COUNT; ++index)
    {
        const cell_t cell = prv_make_cell((int16_t)g_state.ghosts[index].column, (int16_t)g_state.ghosts[index].row);

        if (playfield_is_house(&g_playfield, cell))
        {
            housed++;
        }
    }

    TEST_ASSERT_GREATER_THAN_UINT8(0U, housed);

    if (housed == MSG_GHOST_COUNT)
    {
        for (uint8_t action = 0U; action < (uint8_t)PACMAN_AI_ACTION_COUNT; ++action)
        {
            TEST_ASSERT_EQUAL_FLOAT(PACMAN_AI_DISTANCE_NONE, g_features[(action * PACMAN_AI_FEATURES_PER_ACTION) + 3U]);
        }
    }
}

void test_a_ghost_in_the_corridor_is_seen_and_frightened_ones_are_told_apart(void)
{
    const cell_t start = playfield_get_pacman_start(&g_playfield);

    /* Put Blinky a few cells west of Pacman, along the corridor his start cell opens onto. */
    cell_t ghost_cell = start;
    for (uint8_t step = 0U; step < 3U; ++step)
    {
        const cell_t next = playfield_step(ghost_cell, DIRECTION_WEST);

        if (!playfield_is_walkable(&g_playfield, next))
        {
            break;
        }

        ghost_cell = next;
    }

    TEST_ASSERT_FALSE(playfield_are_cells_equal(start, ghost_cell));

    prv_build_state(start, DIRECTION_WEST);
    g_state.ghosts[0].column = (uint8_t)ghost_cell.x;
    g_state.ghosts[0].row = (uint8_t)ghost_cell.y;

    /* Dangerous first. */
    g_state.frightened_ghosts = 0U;
    pacman_ai_get_features(&g_state, &g_playfield, g_features);

    const float danger = prv_feature_for_direction(DIRECTION_WEST, 3U, DIRECTION_WEST);
    const float prey = prv_feature_for_direction(DIRECTION_WEST, 4U, DIRECTION_WEST);

    TEST_ASSERT_LESS_THAN_FLOAT(PACMAN_AI_DISTANCE_NONE, danger);
    TEST_ASSERT_EQUAL_FLOAT(PACMAN_AI_DISTANCE_NONE, prey);

    /* The same ghost, now edible: the two slots must swap and nothing else about it change. */
    g_state.frightened_ghosts = 0x01U;
    pacman_ai_get_features(&g_state, &g_playfield, g_features);

    TEST_ASSERT_EQUAL_FLOAT(PACMAN_AI_DISTANCE_NONE, prv_feature_for_direction(DIRECTION_WEST, 3U, DIRECTION_WEST));
    TEST_ASSERT_EQUAL_FLOAT(danger, prv_feature_for_direction(DIRECTION_WEST, 4U, DIRECTION_WEST));
}

void test_the_agent_is_told_the_flashing_and_not_a_timer(void)
{
    /* FR-035: the player has no countdown, only the ghosts flashing back towards their own
     * colours. The observation carries exactly that and no clock. */
    const uint8_t global = (uint8_t)PACMAN_AI_ACTION_COUNT * PACMAN_AI_FEATURES_PER_ACTION;

    prv_build_state(playfield_get_pacman_start(&g_playfield), DIRECTION_WEST);
    pacman_ai_get_features(&g_state, &g_playfield, g_features);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, g_features[global]);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, g_features[global + 1U]);

    g_state.frightened_ghosts = 0x05U;
    g_state.are_frightened_ghosts_flashing = true;
    pacman_ai_get_features(&g_state, &g_playfield, g_features);
    TEST_ASSERT_EQUAL_FLOAT(1.0F, g_features[global]);
    TEST_ASSERT_EQUAL_FLOAT(1.0F, g_features[global + 1U]);
}

void test_the_remaining_pellet_fraction_falls_as_the_maze_empties(void)
{
    const uint8_t global = (uint8_t)PACMAN_AI_ACTION_COUNT * PACMAN_AI_FEATURES_PER_ACTION;

    prv_build_state(playfield_get_pacman_start(&g_playfield), DIRECTION_WEST);
    pacman_ai_get_features(&g_state, &g_playfield, g_features);

    TEST_ASSERT_EQUAL_FLOAT(1.0F, g_features[global + 2U]);

    uint16_t eaten = 0U;
    for (int16_t y = 0; (y < PLAYFIELD_HEIGHT) && (eaten < 100U); ++y)
    {
        for (int16_t x = 0; (x < PLAYFIELD_WIDTH) && (eaten < 100U); ++x)
        {
            if (playfield_eat_pellet(&g_playfield, prv_make_cell(x, y)) != PLAYFIELD_PELLET_NONE)
            {
                eaten++;
            }
        }
    }

    TEST_ASSERT_EQUAL_UINT16(100U, eaten);

    pacman_ai_get_features(&g_state, &g_playfield, g_features);
    TEST_ASSERT_LESS_THAN_FLOAT(1.0F, g_features[global + 2U]);
    TEST_ASSERT_GREATER_THAN_FLOAT(0.0F, g_features[global + 2U]);
}

/* --- the tie-break is part of the interface ------------------------------ */

void test_the_highest_score_wins(void)
{
    const float scores[PACMAN_AI_ACTION_COUNT] = {0.1F, 0.2F, 0.9F, 0.3F};

    TEST_ASSERT_EQUAL_INT(PACMAN_AI_ACTION_RIGHT, pacman_ai_choose_action(scores));
}

void test_an_exact_tie_goes_to_the_lower_action(void)
{
    /* FR-039 compares the host's chosen action against the target's. Two equal outputs must
     * not be free to decide differently on the two machines, so the rule is written down and
     * tested rather than left to whichever comparison the compiler emits. */
    const float all_equal[PACMAN_AI_ACTION_COUNT] = {0.5F, 0.5F, 0.5F, 0.5F};
    const float back_ties_with_left[PACMAN_AI_ACTION_COUNT] = {0.1F, 0.7F, 0.2F, 0.7F};

    TEST_ASSERT_EQUAL_INT(PACMAN_AI_ACTION_FORWARD, pacman_ai_choose_action(all_equal));
    TEST_ASSERT_EQUAL_INT(PACMAN_AI_ACTION_LEFT, pacman_ai_choose_action(back_ties_with_left));
}

void test_the_observation_is_all_within_the_unit_range(void)
{
    /* A network's inputs being in a known range is not cosmetic: NEAT's weight bounds are
     * chosen against it, and one feature quietly leaving [0, 1] would move the goalposts for
     * every genome at once. */
    static const uint32_t k_seeds[] = {1000U, 1001U, 1007U, 1019U};

    for (uint8_t index = 0U; index < (sizeof(k_seeds) / sizeof(k_seeds[0])); ++index)
    {
        playfield_map_t map;

        maze_gen_generate(&map, k_seeds[index]);
        playfield_load_from_map(&g_playfield, &map);
        prv_build_state(playfield_get_pacman_start(&g_playfield), DIRECTION_WEST);
        pacman_ai_get_features(&g_state, &g_playfield, g_features);

        for (uint8_t feature = 0U; feature < PACMAN_AI_FEATURE_COUNT; ++feature)
        {
            TEST_ASSERT_TRUE(g_features[feature] >= 0.0F);
            TEST_ASSERT_TRUE(g_features[feature] <= 1.0F);
        }
    }
}

/* --- preconditions ------------------------------------------------------- */

void test_a_null_argument_asserts(void)
{
    ASSERT_PROBE_EXPECT(pacman_ai_get_features(NULL, &g_playfield, g_features), "in_state != NULL");
    ASSERT_PROBE_EXPECT(pacman_ai_get_features(&g_state, NULL, g_features), "in_playfield != NULL");
    ASSERT_PROBE_EXPECT(pacman_ai_get_features(&g_state, &g_playfield, NULL), "out_features != NULL");
    ASSERT_PROBE_EXPECT((void)pacman_ai_choose_action(NULL), "in_scores != NULL");
}

/* --- the trained network the firmware carries ------------------------------ */

/* The generated table has to be one this build can evaluate, and shaped for *this* observation.
 * Both halves matter: a table exported before a feature was added is well formed and would read
 * past the end of the 23 numbers it is given, which is a bug that looks like bad training. */
void test_the_generated_weight_table_fits_this_firmware(void)
{
    TEST_ASSERT_TRUE(pacman_ai_is_available());
    TEST_ASSERT_EQUAL_UINT16((uint16_t)PACMAN_AI_FEATURE_COUNT, g_ai_weights_network.input_count);
    TEST_ASSERT_EQUAL_UINT16((uint16_t)PACMAN_AI_ACTION_COUNT, g_ai_weights_network.output_count);
}

/* VT-UNIT-011, the host half of FR-039: no state between calls, so the same state always decides
 * the same way. If this ever failed, the target would disagree with the host and the on-target
 * check would report it as a porting fault. */
void test_the_same_state_always_decides_the_same_way(void)
{
    prv_build_state(prv_make_cell(13, 23), DIRECTION_WEST);

    const direction_e first = pacman_ai_decide(&g_state, &g_playfield);

    for (uint8_t attempt = 0U; attempt < 8U; ++attempt)
    {
        TEST_ASSERT_EQUAL_UINT8((uint8_t)first, (uint8_t)pacman_ai_decide(&g_state, &g_playfield));
    }
}

/* `pacman_ai_decide` is the three steps put together, and this is what says it put them together in
 * the right order — in particular that the winning *relative* action was read against Pacman's own
 * facing, which is the one place a plausible-looking wrong answer could come from. */
void test_the_decision_is_the_observation_the_network_and_the_facing(void)
{
    static const direction_e k_facings[] = {DIRECTION_NORTH, DIRECTION_SOUTH, DIRECTION_EAST, DIRECTION_WEST};

    for (uint8_t index = 0U; index < (sizeof(k_facings) / sizeof(k_facings[0])); ++index)
    {
        float scores[PACMAN_AI_ACTION_COUNT];

        prv_build_state(prv_make_cell(13, 23), k_facings[index]);

        pacman_ai_get_features(&g_state, &g_playfield, g_features);
        neural_net_evaluate(&g_ai_weights_network, g_features, scores);

        const pacman_ai_action_e expected_action = pacman_ai_choose_action(scores);
        const direction_e expected = pacman_ai_action_to_direction(expected_action, k_facings[index]);

        TEST_ASSERT_EQUAL_UINT8((uint8_t)expected, (uint8_t)pacman_ai_decide(&g_state, &g_playfield));
    }
}

/* The tie-break is part of the interface (FR-039), so it is asserted rather than left to whichever
 * comparison the compiler emits: two equal outputs must not be free to decide differently on two
 * machines. */
void test_an_exact_tie_goes_to_the_lowest_action(void)
{
    static const float k_all_equal[PACMAN_AI_ACTION_COUNT] = {0.0F, 0.0F, 0.0F, 0.0F};
    static const float k_two_equal[PACMAN_AI_ACTION_COUNT] = {1.0F, 2.0F, 2.0F, 1.0F};

    TEST_ASSERT_EQUAL_UINT(PACMAN_AI_ACTION_FORWARD, pacman_ai_choose_action(k_all_equal));
    TEST_ASSERT_EQUAL_UINT(PACMAN_AI_ACTION_LEFT, pacman_ai_choose_action(k_two_equal));
}
