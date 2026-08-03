#include "difficulty.h"

#include <stddef.h>
#include <stdint.h>

#include "custom_assert.h"

/* ==========================================================================
 * difficulty - private
 * ========================================================================= */

/* Milliseconds to cross one cell at 100 % speed.
 *
 * The arcade expresses every speed as a percentage of one thing: a character moving one
 * pixel per frame, at 60 frames a second, across 8-pixel cells. That is 7.5 cells a
 * second, so a cell takes 1000 / 7.5 ms — which is 133.33, not a whole number, and
 * rounding it before dividing would drift the slow rows by a percent or two.
 *
 * So the division is done in one step against a numerator that keeps the thirds:
 * 1000 / (7.5 * percent / 100) == 40000 / (3 * percent). */
#define PERIOD_NUMERATOR               (40000U)
#define PERIOD_DENOMINATOR_PER_PERCENT (3U)

/*! \brief The one-frame scatter blip of the late plans, at the arcade's 60 Hz. */
#define ONE_FRAME_MS                   (17U)

/* Scatter/chase plans (§10.9). Alternating, starting with scatter; when the entries run
 * out the ghosts chase for good.
 *
 * The two long plans are transcribed as they are, quirks included: after the third
 * scatter the arcade chases for seventeen *minutes* and then scatters for a single frame.
 * No one has ever seen that blip — a level is long over by then — but inventing a tidier
 * plan here would be inventing a different game, and the tidier version is exactly the
 * kind of change that is impossible to notice later. */
static const uint32_t g_plan_level_1_durations_ms[] = {7000U, 20000U, 7000U, 20000U, 5000U, 20000U, 5000U};
static const uint32_t g_plan_levels_2_to_4_durations_ms[] = {7000U, 20000U,   7000U,       20000U,
                                                             5000U, 1033000U, ONE_FRAME_MS};
static const uint32_t g_plan_levels_5_up_durations_ms[] = {5000U, 20000U, 5000U, 20000U, 5000U, 1037000U, ONE_FRAME_MS};

typedef struct
{
    const uint32_t* durations_ms;
    uint8_t count;
} phase_plan_t;

#define PHASE_COUNT_OF(array) ((uint8_t)(sizeof(array) / sizeof((array)[0])))

static const phase_plan_t g_plan_level_1 = {g_plan_level_1_durations_ms, PHASE_COUNT_OF(g_plan_level_1_durations_ms)};
static const phase_plan_t g_plan_levels_2_to_4 = {g_plan_levels_2_to_4_durations_ms,
                                                  PHASE_COUNT_OF(g_plan_levels_2_to_4_durations_ms)};
static const phase_plan_t g_plan_levels_5_up = {g_plan_levels_5_up_durations_ms,
                                                PHASE_COUNT_OF(g_plan_levels_5_up_durations_ms)};

_Static_assert(PHASE_COUNT_OF(g_plan_level_1_durations_ms) <= DIFFICULTY_PHASE_MAX, "plan longer than declared");
_Static_assert(PHASE_COUNT_OF(g_plan_levels_2_to_4_durations_ms) <= DIFFICULTY_PHASE_MAX, "plan longer than declared");
_Static_assert(PHASE_COUNT_OF(g_plan_levels_5_up_durations_ms) <= DIFFICULTY_PHASE_MAX, "plan longer than declared");

/* One row of the arcade's level table, in the arcade's own units so that it can be read
 * straight across against the source. Speeds are percentages of full speed; the Elroy
 * columns are how many pellets must be left for that stage to wake; the frightened window
 * is in whole seconds, because the arcade only ever states it that way. */
typedef struct
{
    uint8_t pacman_percent;
    uint8_t pacman_eating_percent;
    uint8_t pacman_frightened_percent;
    uint8_t pacman_frightened_eating_percent;
    uint8_t ghost_percent;
    uint8_t ghost_tunnel_percent;
    uint8_t ghost_frightened_percent;
    uint16_t elroy1_pellets_left;
    uint8_t elroy1_percent;
    uint16_t elroy2_pellets_left;
    uint8_t elroy2_percent;
    uint8_t frightened_seconds;
    uint8_t frightened_flash_count;
    uint16_t inky_dots;
    uint16_t clyde_dots;
    uint8_t house_idle_seconds;
    const phase_plan_t* plan;
} difficulty_row_t;

/* The table, one row per level, transcribed from the Pac-Man Dossier's level table.
 *
 * Read it as the shape of a whole run. Levels 1-4 hand out speed; level 5 is where the
 * ghosts pass Pacman and the frightened window shrinks to two seconds; from 9 on the
 * window is mostly one second and Elroy wakes with a quarter of the maze still full; at
 * 17 and from 19 on a power pellet stops frightening anyone at all. Level 21 takes
 * Pacman's own speed back down while leaving the ghosts at 95 %, and is the last row
 * there is — hence FR-027's finish line.
 *
 * The formatter is off across the table for the same reason as the maze: one row per
 * line *is* the data, and a row reflowed to fill the line cannot be checked against the
 * source it came from. */
/* clang-format off */
static const difficulty_row_t g_levels[DIFFICULTY_FINAL_LEVEL] = {
    /*  pac eat frPac frEat | gho tun frGho | e1@  e1  e2@  e2 | frS fl | inky clyde idle | plan   level */
    {    80, 71,   90,   79,   75,  40,  50,  20,  80,  10,  85,   6,  5,  30,  60, 4, &g_plan_level_1 },      /*  1 */
    {    90, 79,   95,   83,   85,  45,  55,  30,  90,  15,  95,   5,  5,   0,  50, 4, &g_plan_levels_2_to_4 },/*  2 */
    {    90, 79,   95,   83,   85,  45,  55,  40,  90,  20,  95,   4,  5,   0,   0, 4, &g_plan_levels_2_to_4 },/*  3 */
    {    90, 79,   95,   83,   85,  45,  55,  40,  90,  20,  95,   3,  5,   0,   0, 4, &g_plan_levels_2_to_4 },/*  4 */
    {   100, 87,  100,   87,   95,  50,  60,  40, 100,  20, 105,   2,  5,   0,   0, 3, &g_plan_levels_5_up },  /*  5 */
    {   100, 87,  100,   87,   95,  50,  60,  50, 100,  25, 105,   5,  5,   0,   0, 3, &g_plan_levels_5_up },  /*  6 */
    {   100, 87,  100,   87,   95,  50,  60,  50, 100,  25, 105,   2,  5,   0,   0, 3, &g_plan_levels_5_up },  /*  7 */
    {   100, 87,  100,   87,   95,  50,  60,  50, 100,  25, 105,   2,  5,   0,   0, 3, &g_plan_levels_5_up },  /*  8 */
    {   100, 87,  100,   87,   95,  50,  60,  60, 100,  30, 105,   1,  3,   0,   0, 3, &g_plan_levels_5_up },  /*  9 */
    {   100, 87,  100,   87,   95,  50,  60,  60, 100,  30, 105,   5,  5,   0,   0, 3, &g_plan_levels_5_up },  /* 10 */
    {   100, 87,  100,   87,   95,  50,  60,  60, 100,  30, 105,   2,  5,   0,   0, 3, &g_plan_levels_5_up },  /* 11 */
    {   100, 87,  100,   87,   95,  50,  60,  80, 100,  40, 105,   1,  3,   0,   0, 3, &g_plan_levels_5_up },  /* 12 */
    {   100, 87,  100,   87,   95,  50,  60,  80, 100,  40, 105,   1,  3,   0,   0, 3, &g_plan_levels_5_up },  /* 13 */
    {   100, 87,  100,   87,   95,  50,  60,  80, 100,  40, 105,   3,  5,   0,   0, 3, &g_plan_levels_5_up },  /* 14 */
    {   100, 87,  100,   87,   95,  50,  60, 100, 100,  50, 105,   1,  3,   0,   0, 3, &g_plan_levels_5_up },  /* 15 */
    {   100, 87,  100,   87,   95,  50,  60, 100, 100,  50, 105,   1,  3,   0,   0, 3, &g_plan_levels_5_up },  /* 16 */
    {   100, 87,    0,    0,   95,  50,   0, 100, 100,  50, 105,   0,  0,   0,   0, 3, &g_plan_levels_5_up },  /* 17 */
    {   100, 87,  100,   87,   95,  50,  60, 100, 100,  50, 105,   1,  3,   0,   0, 3, &g_plan_levels_5_up },  /* 18 */
    {   100, 87,    0,    0,   95,  50,   0, 120, 100,  60, 105,   0,  0,   0,   0, 3, &g_plan_levels_5_up },  /* 19 */
    {   100, 87,    0,    0,   95,  50,   0, 120, 100,  60, 105,   0,  0,   0,   0, 3, &g_plan_levels_5_up },  /* 20 */
    {    90, 79,    0,    0,   95,  50,   0, 120, 100,  60, 105,   0,  0,   0,   0, 3, &g_plan_levels_5_up },  /* 21 */
};
/* clang-format on */

/* A percentage of full speed as milliseconds per cell, rounded to the nearest whole one.
 *
 * Zero in means the case does not arise at this level — the frightened columns of the
 * rows where a power pellet no longer frightens — and zero comes back out, rather than a
 * division by zero or an absurdly large period that would look like a very slow ghost. */
static uint32_t prv_get_period_ms(uint8_t in_percent)
{
    const uint32_t denominator = (uint32_t)in_percent * PERIOD_DENOMINATOR_PER_PERCENT;

    if (in_percent == 0U)
    {
        return 0U;
    }

    return (PERIOD_NUMERATOR + (denominator / 2U)) / denominator;
}

/* ==========================================================================
 * difficulty - public
 * ========================================================================= */

void difficulty_get(uint8_t in_level, difficulty_t* out_difficulty)
{
    const difficulty_row_t* row;
    uint8_t level = in_level;

    ASSERT(in_level >= DIFFICULTY_FIRST_LEVEL);
    ASSERT(out_difficulty != NULL);

    if (level > DIFFICULTY_FINAL_LEVEL)
    {
        level = DIFFICULTY_FINAL_LEVEL;
    }

    row = &g_levels[level - DIFFICULTY_FIRST_LEVEL];

    out_difficulty->pacman_period_ms = prv_get_period_ms(row->pacman_percent);
    out_difficulty->pacman_eating_period_ms = prv_get_period_ms(row->pacman_eating_percent);
    out_difficulty->pacman_frightened_period_ms = prv_get_period_ms(row->pacman_frightened_percent);
    out_difficulty->pacman_frightened_eating_period_ms = prv_get_period_ms(row->pacman_frightened_eating_percent);

    out_difficulty->ghost_period_ms = prv_get_period_ms(row->ghost_percent);
    out_difficulty->ghost_tunnel_period_ms = prv_get_period_ms(row->ghost_tunnel_percent);
    out_difficulty->ghost_frightened_period_ms = prv_get_period_ms(row->ghost_frightened_percent);

    out_difficulty->elroy1_pellets_left = row->elroy1_pellets_left;
    out_difficulty->elroy1_period_ms = prv_get_period_ms(row->elroy1_percent);
    out_difficulty->elroy2_pellets_left = row->elroy2_pellets_left;
    out_difficulty->elroy2_period_ms = prv_get_period_ms(row->elroy2_percent);

    out_difficulty->inky_dot_limit = row->inky_dots;
    out_difficulty->clyde_dot_limit = row->clyde_dots;
    out_difficulty->house_idle_limit_ms = (uint32_t)row->house_idle_seconds * 1000U;

    out_difficulty->frightened_duration_ms = (uint32_t)row->frightened_seconds * 1000U;
    out_difficulty->frightened_flash_count = row->frightened_flash_count;

    out_difficulty->phase_count = row->plan->count;
    out_difficulty->phase_durations_ms = row->plan->durations_ms;
}

bool difficulty_is_final_level(uint8_t in_level)
{
    ASSERT(in_level >= DIFFICULTY_FIRST_LEVEL);

    return in_level >= DIFFICULTY_FINAL_LEVEL;
}
