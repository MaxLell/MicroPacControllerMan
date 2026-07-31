/*
 * test_sprite.c
 *
 * The sprite primitive and the game's sprite set.
 *
 * What is worth pinning down here is transparency and the palette, because both are
 * invisible failures on real hardware: a sprite that paints its background erases the
 * pellet a ghost walks over, and a palette applied to the wrong index turns Pinky into
 * Blinky. Neither shows up as a crash.
 */

#include "unity.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "assert_probe.h"
#include "custom_assert.h"
#include "framebuffer.h"
#include "msg.h"
#include "sprite.h"
#include "sprite_set.h"

/* ==========================================================================
 * fixtures
 * ========================================================================= */

#define BACKGROUND FRAMEBUFFER_COLOR_BLUE
#define BODY       FRAMEBUFFER_COLOR_YELLOW
#define DETAIL     FRAMEBUFFER_COLOR_WHITE
#define PUPIL      FRAMEBUFFER_COLOR_RED

static framebuffer_t g_framebuffer;

/* clang-format off */
static const char* const g_test_rows[] = {
    ".1.",
    "123",
    ".1.",
};
/* clang-format on */

static const sprite_t g_test_sprite = {3U, 3U, g_test_rows};

static const sprite_palette_t g_test_palette = {{0U, BODY, DETAIL, PUPIL}};

void setUp(void)
{
    assert_probe_begin();
    framebuffer_fill(&g_framebuffer, BACKGROUND);
}

void tearDown(void)
{
    assert_probe_end();
}

/* ==========================================================================
 * the primitive
 * ========================================================================= */

void test_an_index_character_is_drawn_in_its_palette_colour(void)
{
    sprite_draw(&g_framebuffer, &g_test_sprite, &g_test_palette, 10, 10);

    TEST_ASSERT_EQUAL_HEX16(BODY, framebuffer_get_pixel(&g_framebuffer, 11, 10));
    TEST_ASSERT_EQUAL_HEX16(BODY, framebuffer_get_pixel(&g_framebuffer, 10, 11));
    TEST_ASSERT_EQUAL_HEX16(DETAIL, framebuffer_get_pixel(&g_framebuffer, 11, 11));
    TEST_ASSERT_EQUAL_HEX16(PUPIL, framebuffer_get_pixel(&g_framebuffer, 12, 11));
}

void test_a_transparent_pixel_leaves_what_was_underneath(void)
{
    sprite_draw(&g_framebuffer, &g_test_sprite, &g_test_palette, 10, 10);

    /* The corners of the test sprite are '.', and the background must survive them —
     * this is what lets a round actor pass over a pellet without rubbing it out. */
    TEST_ASSERT_EQUAL_HEX16(BACKGROUND, framebuffer_get_pixel(&g_framebuffer, 10, 10));
    TEST_ASSERT_EQUAL_HEX16(BACKGROUND, framebuffer_get_pixel(&g_framebuffer, 12, 10));
    TEST_ASSERT_EQUAL_HEX16(BACKGROUND, framebuffer_get_pixel(&g_framebuffer, 10, 12));
    TEST_ASSERT_EQUAL_HEX16(BACKGROUND, framebuffer_get_pixel(&g_framebuffer, 12, 12));
}

void test_the_same_drawing_takes_a_different_palette(void)
{
    const sprite_palette_t other = {{0U, FRAMEBUFFER_COLOR_GREEN, DETAIL, PUPIL}};

    sprite_draw(&g_framebuffer, &g_test_sprite, &other, 10, 10);

    /* One drawing, four ghosts: the palette is the only thing that differs. */
    TEST_ASSERT_EQUAL_HEX16(FRAMEBUFFER_COLOR_GREEN, framebuffer_get_pixel(&g_framebuffer, 11, 10));
}

void test_a_sprite_may_hang_off_an_edge(void)
{
    /* An actor entering through a tunnel is half off the screen, so clipping is normal
     * operation and not an error. */
    sprite_draw(&g_framebuffer, &g_test_sprite, &g_test_palette, -1, -1);

    TEST_ASSERT_EQUAL_HEX16(DETAIL, framebuffer_get_pixel(&g_framebuffer, 0, 0));

    /* And the far corner: the sprite's centre pixel lands on the last pixel of the
     * buffer, so the bottom-right edge is drawn rather than clipped away. */
    sprite_draw(&g_framebuffer, &g_test_sprite, &g_test_palette, (int16_t)(FRAMEBUFFER_WIDTH - 2),
                (int16_t)(FRAMEBUFFER_HEIGHT - 2));

    TEST_ASSERT_EQUAL_HEX16(DETAIL, framebuffer_get_pixel(&g_framebuffer, (int16_t)(FRAMEBUFFER_WIDTH - 1),
                                                          (int16_t)(FRAMEBUFFER_HEIGHT - 1)));
}

void test_an_unknown_character_is_left_transparent(void)
{
    static const char* const rows[] = {"?"};
    const sprite_t odd = {1U, 1U, rows};

    /* A typo in hand-edited art should leave a visible hole, not stop the firmware. */
    sprite_draw(&g_framebuffer, &odd, &g_test_palette, 5, 5);

    TEST_ASSERT_EQUAL_HEX16(BACKGROUND, framebuffer_get_pixel(&g_framebuffer, 5, 5));
}

/* ==========================================================================
 * the game's set
 * ========================================================================= */

void test_every_sprite_in_the_set_has_pixels(void)
{
    for (uint8_t id = 0U; id < SPRITE_SET_ID_COUNT; ++id)
    {
        const sprite_t* const sprite = sprite_set_get((sprite_set_id_e)id);

        TEST_ASSERT_NOT_NULL(sprite);
        TEST_ASSERT_NOT_NULL(sprite->rows);
        TEST_ASSERT_GREATER_THAN_UINT8(0U, sprite->width);
        TEST_ASSERT_GREATER_THAN_UINT8(0U, sprite->height);

        for (uint8_t row = 0U; row < sprite->height; ++row)
        {
            TEST_ASSERT_NOT_NULL(sprite->rows[row]);
            TEST_ASSERT_EQUAL_UINT32(sprite->width, strlen(sprite->rows[row]));
        }
    }
}

void test_each_ghost_direction_has_its_own_drawing(void)
{
    TEST_ASSERT_EQUAL(SPRITE_SET_GHOST_NORTH, sprite_set_get_ghost_sprite(DIRECTION_NORTH));
    TEST_ASSERT_EQUAL(SPRITE_SET_GHOST_SOUTH, sprite_set_get_ghost_sprite(DIRECTION_SOUTH));
    TEST_ASSERT_EQUAL(SPRITE_SET_GHOST_WEST, sprite_set_get_ghost_sprite(DIRECTION_WEST));
    TEST_ASSERT_EQUAL(SPRITE_SET_GHOST_EAST, sprite_set_get_ghost_sprite(DIRECTION_EAST));
}

void test_a_ghost_in_the_pen_still_looks_somewhere(void)
{
    TEST_ASSERT_EQUAL(SPRITE_SET_GHOST_EAST, sprite_set_get_ghost_sprite(DIRECTION_NONE));
}

void test_pacman_chews_in_the_middle_of_a_step(void)
{
    TEST_ASSERT_EQUAL(SPRITE_SET_PACMAN_CLOSED, sprite_set_get_pacman_sprite(DIRECTION_EAST, 0U));
    TEST_ASSERT_EQUAL(SPRITE_SET_PACMAN_OPEN_EAST, sprite_set_get_pacman_sprite(DIRECTION_EAST, 128U));
    TEST_ASSERT_EQUAL(SPRITE_SET_PACMAN_CLOSED, sprite_set_get_pacman_sprite(DIRECTION_EAST, 255U));
}

void test_pacmans_mouth_faces_the_way_he_moves(void)
{
    TEST_ASSERT_EQUAL(SPRITE_SET_PACMAN_OPEN_NORTH, sprite_set_get_pacman_sprite(DIRECTION_NORTH, 128U));
    TEST_ASSERT_EQUAL(SPRITE_SET_PACMAN_OPEN_SOUTH, sprite_set_get_pacman_sprite(DIRECTION_SOUTH, 128U));
    TEST_ASSERT_EQUAL(SPRITE_SET_PACMAN_OPEN_WEST, sprite_set_get_pacman_sprite(DIRECTION_WEST, 128U));
}

void test_the_open_mouth_actually_removes_pixels(void)
{
    const sprite_t* const closed = sprite_set_get(SPRITE_SET_PACMAN_CLOSED);
    const sprite_t* const open = sprite_set_get(SPRITE_SET_PACMAN_OPEN_EAST);
    uint16_t closed_count = 0U;
    uint16_t open_count = 0U;

    for (uint8_t row = 0U; row < closed->height; ++row)
    {
        for (uint8_t column = 0U; column < closed->width; ++column)
        {
            closed_count += (closed->rows[row][column] != SPRITE_CHAR_TRANSPARENT) ? 1U : 0U;
            open_count += (open->rows[row][column] != SPRITE_CHAR_TRANSPARENT) ? 1U : 0U;
        }
    }

    /* Otherwise the animation is two identical discs and nobody notices until the panel
     * is in front of them. */
    TEST_ASSERT_GREATER_THAN_UINT16(open_count, closed_count);
}

void test_the_four_ghosts_differ_only_in_colour(void)
{
    const sprite_palette_t* const blinky = sprite_set_get_palette(SPRITE_SET_PALETTE_BLINKY);
    const sprite_palette_t* const pinky = sprite_set_get_palette(SPRITE_SET_PALETTE_PINKY);

    TEST_ASSERT_NOT_EQUAL_HEX16(blinky->colors[1], pinky->colors[1]);
    TEST_ASSERT_EQUAL_HEX16(blinky->colors[2], pinky->colors[2]); /* the eyes stay white */
    TEST_ASSERT_EQUAL_HEX16(blinky->colors[3], pinky->colors[3]);
}

/* ==========================================================================
 * preconditions
 * ========================================================================= */

void test_null_arguments_assert(void)
{
    ASSERT_PROBE_EXPECT(sprite_draw(NULL, &g_test_sprite, &g_test_palette, 0, 0), "inout_framebuffer != NULL");
    ASSERT_PROBE_EXPECT(sprite_draw(&g_framebuffer, NULL, &g_test_palette, 0, 0), "in_sprite != NULL");
    ASSERT_PROBE_EXPECT(sprite_draw(&g_framebuffer, &g_test_sprite, NULL, 0, 0), "in_palette != NULL");
    ASSERT_PROBE_EXPECT((void)sprite_set_get(SPRITE_SET_ID_COUNT), "in_id < SPRITE_SET_ID_COUNT");
    ASSERT_PROBE_EXPECT((void)sprite_set_get_palette(SPRITE_SET_PALETTE_COUNT),
                        "in_palette < SPRITE_SET_PALETTE_COUNT");
}
