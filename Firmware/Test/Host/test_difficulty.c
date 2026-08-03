/*
 * Unit tests for App/difficulty.
 *
 * The module is a table and a division, which sounds like nothing worth testing. It is the
 * opposite: it is the entire difficulty of the game, twenty-one rows of numbers typed in by
 * hand from an external source, and a transposed digit in it would not fail a build, would
 * not fail any other test, and would surface only as a level that plays wrong — which is
 * exactly the kind of wrong nobody can spot by playing.
 *
 * So the tests come in two kinds. A few pin literal milliseconds, to catch the conversion
 * arithmetic drifting. The rest state the *shape* of the curve — never slower, Elroy always
 * quicker than a plain ghost, the frightened window gone by the end — which is what a
 * mistyped row breaks even when every individual number still looks plausible.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "assert_probe.h"
#include "custom_assert.h"
#include "difficulty.h"
#include "unity.h"

/* Full speed is 1 pixel a frame at 60 Hz across 8-pixel cells: 7.5 cells a second, so
 * 133 ms a cell. These are that, scaled by the percentages of §10.9's level 1 row and
 * rounded — spelled out rather than recomputed, because a test that redoes the module's
 * own arithmetic proves only that it is self-consistent. */
#define LEVEL_1_PACMAN_MS           (167U) /*  80 % */
#define LEVEL_1_PACMAN_EATING_MS    (188U) /*  71 % */
#define LEVEL_1_GHOST_MS            (178U) /*  75 % */
#define LEVEL_1_GHOST_TUNNEL_MS     (333U) /*  40 % */
#define LEVEL_1_GHOST_FRIGHTENED_MS (267U) /* 50 % */
#define LEVEL_5_ELROY2_MS           (127U) /* 105 % */

#define FULL_SPEED_MS               (133U)

#define LEVEL_1                     (1U)
#define LEVEL_5                     (5U)
#define LEVEL_17                    (17U)
#define LEVEL_18                    (18U)
#define LEVEL_19                    (19U)

static difficulty_t g_difficulty;

void setUp(void)
{
    assert_probe_begin();
}

void tearDown(void)
{
    assert_probe_end();
}

/* --- the conversion ------------------------------------------------------ */

void test_level_one_is_the_arcades_level_one(void)
{
    difficulty_get(LEVEL_1, &g_difficulty);

    TEST_ASSERT_EQUAL_UINT32(LEVEL_1_PACMAN_MS, g_difficulty.pacman_period_ms);
    TEST_ASSERT_EQUAL_UINT32(LEVEL_1_PACMAN_EATING_MS, g_difficulty.pacman_eating_period_ms);
    TEST_ASSERT_EQUAL_UINT32(LEVEL_1_GHOST_MS, g_difficulty.ghost_period_ms);
    TEST_ASSERT_EQUAL_UINT32(LEVEL_1_GHOST_TUNNEL_MS, g_difficulty.ghost_tunnel_period_ms);
    TEST_ASSERT_EQUAL_UINT32(LEVEL_1_GHOST_FRIGHTENED_MS, g_difficulty.ghost_frightened_period_ms);
    TEST_ASSERT_EQUAL_UINT32(6000U, g_difficulty.frightened_duration_ms);
}

void test_full_speed_is_a_cell_every_hundred_and_thirty_three_milliseconds(void)
{
    /* Level 5 is where Pacman first reaches 100 %, so it is where the base rate is visible
     * without any scaling on top of it. */
    difficulty_get(LEVEL_5, &g_difficulty);

    TEST_ASSERT_EQUAL_UINT32(FULL_SPEED_MS, g_difficulty.pacman_period_ms);
}

void test_cruise_elroy_at_its_second_stage_passes_full_speed(void)
{
    /* 105 %: the only thing in the game that moves faster than a cell every 133 ms. */
    difficulty_get(LEVEL_5, &g_difficulty);

    TEST_ASSERT_EQUAL_UINT32(LEVEL_5_ELROY2_MS, g_difficulty.elroy2_period_ms);
    TEST_ASSERT_LESS_THAN_UINT32(FULL_SPEED_MS, g_difficulty.elroy2_period_ms);
}

/* --- the shape of the curve ---------------------------------------------- */

void test_no_level_is_easier_than_the_one_before_it(void)
{
    difficulty_t previous;

    difficulty_get(DIFFICULTY_FIRST_LEVEL, &previous);

    for (uint8_t level = DIFFICULTY_FIRST_LEVEL + 1U; level <= DIFFICULTY_FINAL_LEVEL; ++level)
    {
        char message[64];

        difficulty_get(level, &g_difficulty);

        /* The ghosts never slow down, and Elroy never wakes later than he did. Both are
         * staircases with long flat stretches, so "not worse" is the strongest claim that
         * holds end to end — and it is the one a transposed digit breaks. */
        (void)snprintf(message, sizeof(message), "level %u lets the ghosts slow down", level);
        TEST_ASSERT_LESS_OR_EQUAL_UINT32_MESSAGE(previous.ghost_period_ms, g_difficulty.ghost_period_ms, message);

        (void)snprintf(message, sizeof(message), "level %u wakes Elroy later", level);
        TEST_ASSERT_GREATER_OR_EQUAL_UINT16_MESSAGE(previous.elroy1_pellets_left, g_difficulty.elroy1_pellets_left,
                                                    message);

        previous = g_difficulty;
    }
}

void test_cruise_elroy_is_always_two_stages_and_always_faster_than_a_plain_ghost(void)
{
    for (uint8_t level = DIFFICULTY_FIRST_LEVEL; level <= DIFFICULTY_FINAL_LEVEL; ++level)
    {
        char message[64];

        difficulty_get(level, &g_difficulty);

        /* The second stage triggers later — that is, on fewer pellets left — and is the
         * faster of the two. Swap those and Blinky would get *slower* as the maze empties,
         * which is the exact opposite of the point. */
        (void)snprintf(message, sizeof(message), "level %u: Elroy's stages are the wrong way round", level);
        TEST_ASSERT_LESS_THAN_UINT16_MESSAGE(g_difficulty.elroy1_pellets_left, g_difficulty.elroy2_pellets_left,
                                             message);
        TEST_ASSERT_LESS_THAN_UINT32_MESSAGE(g_difficulty.elroy1_period_ms, g_difficulty.elroy2_period_ms, message);

        (void)snprintf(message, sizeof(message), "level %u: Elroy is no faster than a plain ghost", level);
        TEST_ASSERT_LESS_THAN_UINT32_MESSAGE(g_difficulty.ghost_period_ms, g_difficulty.elroy1_period_ms, message);
    }
}

void test_a_ghost_always_crawls_in_the_tunnel(void)
{
    for (uint8_t level = DIFFICULTY_FIRST_LEVEL; level <= DIFFICULTY_FINAL_LEVEL; ++level)
    {
        char message[64];

        difficulty_get(level, &g_difficulty);

        (void)snprintf(message, sizeof(message), "level %u: the tunnel does not slow a ghost", level);
        TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(g_difficulty.ghost_period_ms, g_difficulty.ghost_tunnel_period_ms,
                                                message);
    }
}

void test_pacman_is_always_slower_while_eating(void)
{
    for (uint8_t level = DIFFICULTY_FIRST_LEVEL; level <= DIFFICULTY_FINAL_LEVEL; ++level)
    {
        char message[64];

        difficulty_get(level, &g_difficulty);

        (void)snprintf(message, sizeof(message), "level %u: a mouthful costs him nothing", level);
        TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(g_difficulty.pacman_period_ms, g_difficulty.pacman_eating_period_ms,
                                                message);
    }
}

/* --- where the frightened window stops -------------------------------- */

void test_the_frightened_window_is_gone_at_seventeen_back_at_eighteen_and_gone_for_good_at_nineteen(void)
{
    /* Not a smooth fade, and worth pinning exactly because it looks like a typo: 17 has no
     * window, 18 hands one second back, and from 19 on there is never one again. */
    difficulty_get(LEVEL_17, &g_difficulty);
    TEST_ASSERT_EQUAL_UINT32(0U, g_difficulty.frightened_duration_ms);

    difficulty_get(LEVEL_18, &g_difficulty);
    TEST_ASSERT_EQUAL_UINT32(1000U, g_difficulty.frightened_duration_ms);

    for (uint8_t level = LEVEL_19; level <= DIFFICULTY_FINAL_LEVEL; ++level)
    {
        difficulty_get(level, &g_difficulty);
        TEST_ASSERT_EQUAL_UINT32(0U, g_difficulty.frightened_duration_ms);
    }
}

void test_a_level_without_a_window_flashes_no_warning(void)
{
    difficulty_get(DIFFICULTY_FINAL_LEVEL, &g_difficulty);

    TEST_ASSERT_EQUAL_UINT32(0U, g_difficulty.frightened_duration_ms);
    TEST_ASSERT_EQUAL_UINT8(0U, g_difficulty.frightened_flash_count);
}

void test_every_window_that_exists_is_flagged_before_it_closes(void)
{
    for (uint8_t level = DIFFICULTY_FIRST_LEVEL; level <= DIFFICULTY_FINAL_LEVEL; ++level)
    {
        char message[64];

        difficulty_get(level, &g_difficulty);

        if (g_difficulty.frightened_duration_ms == 0U)
        {
            continue;
        }

        /* A window with no flashes would close without warning, which is unfair rather
         * than difficult. */
        (void)snprintf(message, sizeof(message), "level %u: the window closes unannounced", level);
        TEST_ASSERT_GREATER_THAN_UINT8_MESSAGE(0U, g_difficulty.frightened_flash_count, message);
    }
}

/* --- the scatter/chase plan ---------------------------------------------- */

void test_every_level_has_a_plan_that_starts_with_a_scatter(void)
{
    for (uint8_t level = DIFFICULTY_FIRST_LEVEL; level <= DIFFICULTY_FINAL_LEVEL; ++level)
    {
        difficulty_get(level, &g_difficulty);

        TEST_ASSERT_NOT_NULL(g_difficulty.phase_durations_ms);
        TEST_ASSERT_GREATER_THAN_UINT8(0U, g_difficulty.phase_count);
        TEST_ASSERT_LESS_OR_EQUAL_UINT8(DIFFICULTY_PHASE_MAX, g_difficulty.phase_count);

        /* Entry 0 is a scatter by construction (§10.9), and a zero-length one would mean
         * the ghosts hunt from the first tick — which is not how any level opens. */
        TEST_ASSERT_GREATER_THAN_UINT32(0U, g_difficulty.phase_durations_ms[0]);
    }
}

void test_the_later_levels_open_with_a_shorter_scatter(void)
{
    uint32_t level_1_scatter_ms;
    uint32_t level_5_scatter_ms;

    difficulty_get(LEVEL_1, &g_difficulty);
    level_1_scatter_ms = g_difficulty.phase_durations_ms[0];

    difficulty_get(LEVEL_5, &g_difficulty);
    level_5_scatter_ms = g_difficulty.phase_durations_ms[0];

    /* Seven seconds of grace to begin with, five from level 5 on. */
    TEST_ASSERT_EQUAL_UINT32(7000U, level_1_scatter_ms);
    TEST_ASSERT_EQUAL_UINT32(5000U, level_5_scatter_ms);
}

/* --- the ends of the table ----------------------------------------------- */

void test_the_table_stops_changing_after_the_final_level(void)
{
    difficulty_t final_level;
    difficulty_t far_beyond;

    difficulty_get(DIFFICULTY_FINAL_LEVEL, &final_level);
    difficulty_get(200U, &far_beyond);

    /* The lookup has to stay total — it is called from inside a tick — and the arcade's own
     * table flattens at 21, so past the finish line is the finish line's row. */
    TEST_ASSERT_EQUAL_MEMORY(&final_level, &far_beyond, sizeof(final_level));
}

void test_only_the_last_level_is_the_last_level(void)
{
    for (uint8_t level = DIFFICULTY_FIRST_LEVEL; level < DIFFICULTY_FINAL_LEVEL; ++level)
    {
        TEST_ASSERT_FALSE(difficulty_is_final_level(level));
    }

    TEST_ASSERT_TRUE(difficulty_is_final_level(DIFFICULTY_FINAL_LEVEL));
}

/* --- preconditions ------------------------------------------------------- */

void test_level_zero_asserts(void)
{
    ASSERT_PROBE_EXPECT(difficulty_get(0U, &g_difficulty), "in_level >= DIFFICULTY_FIRST_LEVEL");
    ASSERT_PROBE_EXPECT((void)difficulty_is_final_level(0U), "in_level >= DIFFICULTY_FIRST_LEVEL");
}

void test_a_null_result_asserts(void)
{
    ASSERT_PROBE_EXPECT(difficulty_get(LEVEL_1, NULL), "out_difficulty != NULL");
}
