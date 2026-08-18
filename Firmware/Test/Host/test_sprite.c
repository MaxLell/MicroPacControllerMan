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
#include <stdio.h>
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

/* How many pixels of a drawing are not transparent. */
static uint16_t prv_count_solid_pixels(sprite_set_id_e in_id)
{
    const sprite_t* const sprite = sprite_set_get(in_id);
    uint16_t count = 0U;

    for (uint8_t row = 0U; row < sprite->height; ++row)
    {
        for (uint8_t column = 0U; column < sprite->width; ++column)
        {
            count += (sprite->rows[row][column] != SPRITE_CHAR_TRANSPARENT) ? 1U : 0U;
        }
    }

    return count;
}

void test_each_ghost_direction_has_its_own_drawing(void)
{
    TEST_ASSERT_EQUAL(SPRITE_SET_GHOST_NORTH_A, sprite_set_get_ghost_sprite(DIRECTION_NORTH, 0U));
    TEST_ASSERT_EQUAL(SPRITE_SET_GHOST_SOUTH_A, sprite_set_get_ghost_sprite(DIRECTION_SOUTH, 0U));
    TEST_ASSERT_EQUAL(SPRITE_SET_GHOST_WEST_A, sprite_set_get_ghost_sprite(DIRECTION_WEST, 0U));
    TEST_ASSERT_EQUAL(SPRITE_SET_GHOST_EAST_A, sprite_set_get_ghost_sprite(DIRECTION_EAST, 0U));
}

void test_a_ghost_in_the_pen_still_looks_somewhere(void)
{
    TEST_ASSERT_EQUAL(SPRITE_SET_GHOST_EAST_A, sprite_set_get_ghost_sprite(DIRECTION_NONE, 0U));
}

void test_a_ghosts_skirt_waves_across_a_step(void)
{
    /* Two frames per cell, and they have to be different drawings — one frame and a ghost
     * reads as a sticker being slid across the maze. */
    TEST_ASSERT_EQUAL(SPRITE_SET_GHOST_EAST_A, sprite_set_get_ghost_sprite(DIRECTION_EAST, 0U));
    TEST_ASSERT_EQUAL(SPRITE_SET_GHOST_EAST_B, sprite_set_get_ghost_sprite(DIRECTION_EAST, 255U));

    TEST_ASSERT_NOT_EQUAL_UINT16(prv_count_solid_pixels(SPRITE_SET_GHOST_EAST_A),
                                 prv_count_solid_pixels(SPRITE_SET_GHOST_EAST_B));
}

void test_a_frightened_ghost_waves_too_and_has_no_direction(void)
{
    TEST_ASSERT_EQUAL(SPRITE_SET_GHOST_FRIGHTENED_A, sprite_set_get_frightened_sprite(0U));
    TEST_ASSERT_EQUAL(SPRITE_SET_GHOST_FRIGHTENED_B, sprite_set_get_frightened_sprite(255U));
}

void test_pacman_chews_shut_half_wide_half_across_a_step(void)
{
    /* The arcade's own three-frame cycle, clocked by the step. Half on the way open and
     * half again on the way shut — skip either and the chew becomes a blink. */
    TEST_ASSERT_EQUAL(SPRITE_SET_PACMAN_CLOSED, sprite_set_get_pacman_sprite(DIRECTION_EAST, 0U));
    TEST_ASSERT_EQUAL(SPRITE_SET_PACMAN_HALF_EAST, sprite_set_get_pacman_sprite(DIRECTION_EAST, 64U));
    TEST_ASSERT_EQUAL(SPRITE_SET_PACMAN_WIDE_EAST, sprite_set_get_pacman_sprite(DIRECTION_EAST, 128U));
    TEST_ASSERT_EQUAL(SPRITE_SET_PACMAN_HALF_EAST, sprite_set_get_pacman_sprite(DIRECTION_EAST, 192U));
}

void test_pacmans_mouth_faces_the_way_he_moves(void)
{
    TEST_ASSERT_EQUAL(SPRITE_SET_PACMAN_WIDE_NORTH, sprite_set_get_pacman_sprite(DIRECTION_NORTH, 128U));
    TEST_ASSERT_EQUAL(SPRITE_SET_PACMAN_WIDE_SOUTH, sprite_set_get_pacman_sprite(DIRECTION_SOUTH, 128U));
    TEST_ASSERT_EQUAL(SPRITE_SET_PACMAN_WIDE_WEST, sprite_set_get_pacman_sprite(DIRECTION_WEST, 128U));
    TEST_ASSERT_EQUAL(SPRITE_SET_PACMAN_WIDE_EAST, sprite_set_get_pacman_sprite(DIRECTION_EAST, 128U));
}

void test_the_mouth_takes_a_bigger_bite_each_frame(void)
{
    /* Otherwise the animation is three identical discs, and nobody notices until the
     * panel is in front of them. */
    const uint16_t closed = prv_count_solid_pixels(SPRITE_SET_PACMAN_CLOSED);
    const uint16_t half = prv_count_solid_pixels(SPRITE_SET_PACMAN_HALF_EAST);
    const uint16_t wide = prv_count_solid_pixels(SPRITE_SET_PACMAN_WIDE_EAST);

    TEST_ASSERT_GREATER_THAN_UINT16(half, closed);
    TEST_ASSERT_GREATER_THAN_UINT16(wide, half);
}

void test_the_mirrored_pacman_frames_really_are_mirrors(void)
{
    /* The cabinet has no west- or north-facing Pacman: it flips the east and south ones in
     * hardware, and those two below are that flip folded into the data. A mirror pasted in
     * by hand is the one mistake that a table cannot show and a panel shows instantly, so
     * it is checked rather than trusted.
     *
     * The flip is about the sprite's own centre, not the 16 px box: the figure sits a
     * pixel left of and above centre, which the cabinet corrects with a position offset
     * when it flips. Here the correction is in the data, so the mirror is off by one. */
    static const struct
    {
        sprite_set_id_e original;
        sprite_set_id_e mirrored;
        bool is_horizontal;
    } cases[] = {
        {SPRITE_SET_PACMAN_WIDE_EAST, SPRITE_SET_PACMAN_WIDE_WEST, true},
        {SPRITE_SET_PACMAN_HALF_EAST, SPRITE_SET_PACMAN_HALF_WEST, true},
        {SPRITE_SET_PACMAN_WIDE_SOUTH, SPRITE_SET_PACMAN_WIDE_NORTH, false},
        {SPRITE_SET_PACMAN_HALF_SOUTH, SPRITE_SET_PACMAN_HALF_NORTH, false},
    };

    for (uint8_t index = 0U; index < (sizeof(cases) / sizeof(cases[0])); ++index)
    {
        const sprite_t* const original = sprite_set_get(cases[index].original);
        const sprite_t* const mirrored = sprite_set_get(cases[index].mirrored);
        const uint8_t last = (uint8_t)(original->width - 1U);

        for (uint8_t row = 0U; row < original->height; ++row)
        {
            for (uint8_t column = 0U; column < original->width; ++column)
            {
                const uint8_t mirror_row = cases[index].is_horizontal ? row : (uint8_t)(last - row - 1U);
                const uint8_t mirror_column = cases[index].is_horizontal ? (uint8_t)(last - column - 1U) : column;
                char message[80];

                if ((mirror_row >= original->height) || (mirror_column >= original->width))
                {
                    continue; /* the pixel the offset pushed off the edge */
                }

                (void)snprintf(message, sizeof(message), "sprite %u differs from its mirror at %u,%u",
                               (unsigned)cases[index].mirrored, column, row);
                TEST_ASSERT_EQUAL_CHAR_MESSAGE(original->rows[row][column], mirrored->rows[mirror_row][mirror_column],
                                               message);
            }
        }
    }
}

/* The one glyph that was drawn rather than decoded out of the tile ROM, and the reason the title
 * screen can spell the game's own name. Checked as *ink in the middle rows and nowhere else*, which
 * is what makes it a hyphen rather than a tile that happens to have pixels — `SPRITE_SET_ID_COUNT`
 * already guarantees the latter. */
void test_the_font_has_a_hyphen(void)
{
    const sprite_t* const hyphen = sprite_set_get(SPRITE_SET_GLYPH_HYPHEN);

    TEST_ASSERT_EQUAL_UINT(SPRITE_SET_GLYPH_HYPHEN, sprite_set_get_glyph('-'));

    for (uint8_t row = 0U; row < hyphen->height; ++row)
    {
        const bool is_a_bar_row = (row == 3U) || (row == 4U);

        for (uint8_t column = 0U; column < hyphen->width; ++column)
        {
            /* Ink is index 1, the palette's foreground. The rest of a glyph is index 2 — the tile's
             * own background, opaque black, so one item paints a whole cell — and not transparency,
             * which is why this cannot be written as "not transparent". */
            const bool is_ink = hyphen->rows[row][column] == SPRITE_CHAR_INDEX_1;

            /* The bar spans the six-pixel box the letters either side of it are drawn in, so it
             * sits on the same grid rather than looking like it came from another font. */
            TEST_ASSERT_EQUAL(is_a_bar_row && (column >= 1U) && (column <= 6U), is_ink);
        }
    }
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
