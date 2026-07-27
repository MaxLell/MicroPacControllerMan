/*
 * Unit tests for Services/gfx.
 *
 * Pure logic on a frame buffer, so no mocks. These check the properties that matter
 * for each shape — the right pixels, the right count, and nothing spilling outside —
 * rather than pinning down whole bitmaps, which would break on any harmless change to
 * a rasterisation tie-break.
 */
#include <stdbool.h>
#include <stdint.h>

#include "custom_assert.h"
#include "framebuffer.h"
#include "gfx.h"
#include "unity.h"

#define TEST_ORIGIN_X (10)
#define TEST_ORIGIN_Y (20)
#define TEST_SPAN (8)

static framebuffer_t g_framebuffer;

static uint32_t prv_count_ink(void)
{
    uint32_t count = 0U;

    for (int16_t y = 0; y < FRAMEBUFFER_HEIGHT; ++y)
    {
        for (int16_t x = 0; x < FRAMEBUFFER_WIDTH; ++x)
        {
            if (framebuffer_get_pixel(&g_framebuffer, x, y) == FRAMEBUFFER_COLOR_BLACK)
            {
                ++count;
            }
        }
    }

    return count;
}

static bool prv_is_ink(int16_t in_x, int16_t in_y)
{
    return framebuffer_get_pixel(&g_framebuffer, in_x, in_y) == FRAMEBUFFER_COLOR_BLACK;
}

void setUp(void)
{
    framebuffer_clear(&g_framebuffer);
}

void tearDown(void)
{
}

/* --- lines ---------------------------------------------------------------- */

void test_a_horizontal_line_inks_exactly_its_span(void)
{
    gfx_horizontal_line(&g_framebuffer, TEST_ORIGIN_X, TEST_ORIGIN_Y, TEST_SPAN,
                        FRAMEBUFFER_COLOR_BLACK);

    TEST_ASSERT_EQUAL_UINT32(TEST_SPAN, prv_count_ink());
    TEST_ASSERT_TRUE(prv_is_ink(TEST_ORIGIN_X, TEST_ORIGIN_Y));
    TEST_ASSERT_TRUE(prv_is_ink(TEST_ORIGIN_X + TEST_SPAN - 1, TEST_ORIGIN_Y));
    TEST_ASSERT_FALSE(prv_is_ink(TEST_ORIGIN_X - 1, TEST_ORIGIN_Y));
    TEST_ASSERT_FALSE(prv_is_ink(TEST_ORIGIN_X + TEST_SPAN, TEST_ORIGIN_Y));
}

void test_a_vertical_line_inks_exactly_its_span(void)
{
    gfx_vertical_line(&g_framebuffer, TEST_ORIGIN_X, TEST_ORIGIN_Y, TEST_SPAN,
                      FRAMEBUFFER_COLOR_BLACK);

    TEST_ASSERT_EQUAL_UINT32(TEST_SPAN, prv_count_ink());
    TEST_ASSERT_TRUE(prv_is_ink(TEST_ORIGIN_X, TEST_ORIGIN_Y));
    TEST_ASSERT_TRUE(prv_is_ink(TEST_ORIGIN_X, TEST_ORIGIN_Y + TEST_SPAN - 1));
    TEST_ASSERT_FALSE(prv_is_ink(TEST_ORIGIN_X, TEST_ORIGIN_Y + TEST_SPAN));
}

void test_a_line_inks_both_of_its_endpoints(void)
{
    gfx_line(&g_framebuffer, 0, 0, 20, 10, FRAMEBUFFER_COLOR_BLACK);

    TEST_ASSERT_TRUE(prv_is_ink(0, 0));
    TEST_ASSERT_TRUE(prv_is_ink(20, 10));
}

void test_a_steep_line_inks_both_of_its_endpoints(void)
{
    /* The steep case takes the mirrored path through Bresenham, so it is worth its
     * own check. */
    gfx_line(&g_framebuffer, 5, 0, 10, 40, FRAMEBUFFER_COLOR_BLACK);

    TEST_ASSERT_TRUE(prv_is_ink(5, 0));
    TEST_ASSERT_TRUE(prv_is_ink(10, 40));
}

void test_a_line_drawn_backwards_covers_the_same_pixels(void)
{
    uint32_t forward_count;

    gfx_line(&g_framebuffer, 3, 4, 30, 25, FRAMEBUFFER_COLOR_BLACK);
    forward_count = prv_count_ink();

    framebuffer_clear(&g_framebuffer);
    gfx_line(&g_framebuffer, 30, 25, 3, 4, FRAMEBUFFER_COLOR_BLACK);

    TEST_ASSERT_EQUAL_UINT32(forward_count, prv_count_ink());
    TEST_ASSERT_TRUE(prv_is_ink(3, 4));
    TEST_ASSERT_TRUE(prv_is_ink(30, 25));
}

/* --- rectangles ----------------------------------------------------------- */

void test_a_rectangle_outline_inks_its_border_only(void)
{
    gfx_rectangle(&g_framebuffer, 4, 6, 10, 8, FRAMEBUFFER_COLOR_BLACK);

    /* Four corners present, interior empty. */
    TEST_ASSERT_TRUE(prv_is_ink(4, 6));
    TEST_ASSERT_TRUE(prv_is_ink(13, 6));
    TEST_ASSERT_TRUE(prv_is_ink(4, 13));
    TEST_ASSERT_TRUE(prv_is_ink(13, 13));
    TEST_ASSERT_FALSE(prv_is_ink(8, 9));

    /* Perimeter of a 10x8 outline, corners not double-counted. */
    TEST_ASSERT_EQUAL_UINT32((2U * 10U) + (2U * 8U) - 4U, prv_count_ink());
}

void test_a_filled_rectangle_inks_its_whole_area(void)
{
    gfx_filled_rectangle(&g_framebuffer, 4, 6, 10, 8, FRAMEBUFFER_COLOR_BLACK);

    TEST_ASSERT_EQUAL_UINT32(10U * 8U, prv_count_ink());
    TEST_ASSERT_TRUE(prv_is_ink(8, 9));
}

void test_white_draws_over_black(void)
{
    framebuffer_fill(&g_framebuffer, FRAMEBUFFER_COLOR_BLACK);
    gfx_filled_rectangle(&g_framebuffer, 0, 0, 4, 4, FRAMEBUFFER_COLOR_WHITE);

    TEST_ASSERT_FALSE(prv_is_ink(0, 0));
    TEST_ASSERT_FALSE(prv_is_ink(3, 3));
    TEST_ASSERT_TRUE(prv_is_ink(4, 4));
}

/* --- circles and triangles ------------------------------------------------ */

void test_a_circle_outline_inks_its_axis_points_but_not_its_centre(void)
{
    const int16_t center = 40;
    const int16_t radius = 10;

    gfx_circle(&g_framebuffer, center, center, radius, FRAMEBUFFER_COLOR_BLACK);

    TEST_ASSERT_TRUE(prv_is_ink(center, center - radius));
    TEST_ASSERT_TRUE(prv_is_ink(center, center + radius));
    TEST_ASSERT_TRUE(prv_is_ink(center - radius, center));
    TEST_ASSERT_TRUE(prv_is_ink(center + radius, center));
    TEST_ASSERT_FALSE(prv_is_ink(center, center));
}

void test_a_filled_circle_inks_its_centre_and_stays_within_its_radius(void)
{
    const int16_t center = 40;
    const int16_t radius = 6;

    gfx_filled_circle(&g_framebuffer, center, center, radius, FRAMEBUFFER_COLOR_BLACK);

    TEST_ASSERT_TRUE(prv_is_ink(center, center));
    TEST_ASSERT_TRUE(prv_is_ink(center, center - radius));
    /* Just outside the radius on the diagonal, and one pixel past it on the axis. */
    TEST_ASSERT_FALSE(prv_is_ink(center + radius, center + radius));
    TEST_ASSERT_FALSE(prv_is_ink(center, center - radius - 1));
}

void test_a_filled_triangle_inks_its_interior(void)
{
    gfx_filled_triangle(&g_framebuffer, 20, 40, 30, 20, 40, 40, FRAMEBUFFER_COLOR_BLACK);

    TEST_ASSERT_TRUE(prv_is_ink(30, 30));
    TEST_ASSERT_TRUE(prv_is_ink(30, 20));
    TEST_ASSERT_FALSE(prv_is_ink(20, 20));
}

void test_a_degenerate_triangle_draws_a_single_row(void)
{
    /* All three corners on one row — the special case in the scanline fill. */
    gfx_filled_triangle(&g_framebuffer, 10, 50, 20, 50, 15, 50, FRAMEBUFFER_COLOR_BLACK);

    TEST_ASSERT_EQUAL_UINT32(11U, prv_count_ink());
    TEST_ASSERT_TRUE(prv_is_ink(10, 50));
    TEST_ASSERT_TRUE(prv_is_ink(20, 50));
    TEST_ASSERT_FALSE(prv_is_ink(21, 50));
}

/* --- clipping ------------------------------------------------------------- */

void test_shapes_may_hang_over_the_edges(void)
{
    /* Straddling every edge at once: nothing should be written outside, and the run
     * must not fault. */
    gfx_filled_rectangle(&g_framebuffer, -5, -5, FRAMEBUFFER_WIDTH + 10, FRAMEBUFFER_HEIGHT + 10,
                         FRAMEBUFFER_COLOR_BLACK);

    TEST_ASSERT_EQUAL_UINT32((uint32_t)FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT, prv_count_ink());
}

void test_a_shape_entirely_off_screen_inks_nothing(void)
{
    gfx_filled_circle(&g_framebuffer, -50, -50, 10, FRAMEBUFFER_COLOR_BLACK);
    gfx_line(&g_framebuffer, -20, -20, -10, -10, FRAMEBUFFER_COLOR_BLACK);

    TEST_ASSERT_EQUAL_UINT32(0U, prv_count_ink());
}
