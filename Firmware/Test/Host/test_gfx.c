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

/* A line placed away from every edge, so clipping cannot mask a wrong span. */
#define TEST_ORIGIN_X (10)
#define TEST_ORIGIN_Y (20)
#define TEST_SPAN (8)

/* A shallow and a steep line, to take both branches of Bresenham. */
#define TEST_SHALLOW_START_X (0)
#define TEST_SHALLOW_START_Y (0)
#define TEST_SHALLOW_END_X (20)
#define TEST_SHALLOW_END_Y (10)
#define TEST_STEEP_START_X (5)
#define TEST_STEEP_START_Y (0)
#define TEST_STEEP_END_X (10)
#define TEST_STEEP_END_Y (40)
#define TEST_REVERSIBLE_START_X (3)
#define TEST_REVERSIBLE_START_Y (4)
#define TEST_REVERSIBLE_END_X (30)
#define TEST_REVERSIBLE_END_Y (25)

/* A rectangle and a point known to sit inside it. */
#define TEST_RECT_X (4)
#define TEST_RECT_Y (6)
#define TEST_RECT_WIDTH (10)
#define TEST_RECT_HEIGHT (8)
#define TEST_RECT_INSIDE_X (8)
#define TEST_RECT_INSIDE_Y (9)
#define TEST_RECT_CORNER_COUNT (4U)
#define TEST_OVERDRAW_SIDE (4)

/* Circles, and a triangle with a point known to sit inside it. */
#define TEST_CIRCLE_CENTER (40)
#define TEST_OUTLINE_RADIUS (10)
#define TEST_FILLED_RADIUS (6)
#define TEST_TRIANGLE_LEFT_X (20)
#define TEST_TRIANGLE_BASE_Y (40)
#define TEST_TRIANGLE_APEX_X (30)
#define TEST_TRIANGLE_APEX_Y (20)
#define TEST_TRIANGLE_RIGHT_X (40)
#define TEST_TRIANGLE_INSIDE_Y (30)

/* A degenerate triangle: all three corners on one row. */
#define TEST_FLAT_LEFT_X (10)
#define TEST_FLAT_MIDDLE_X (15)
#define TEST_FLAT_RIGHT_X (20)
#define TEST_FLAT_ROW (50)
#define TEST_FLAT_INK_COUNT (11U)

/* Shapes that hang over, or sit entirely beyond, the edges. */
#define TEST_OVERHANG (5)
#define TEST_OVERHANG_MARGIN (10)
#define TEST_OFF_SCREEN_CENTER (-50)
#define TEST_OFF_SCREEN_RADIUS (10)
#define TEST_OFF_SCREEN_START (-20)
#define TEST_OFF_SCREEN_END (-10)

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
    gfx_line(&g_framebuffer, TEST_SHALLOW_START_X, TEST_SHALLOW_START_Y, TEST_SHALLOW_END_X,
             TEST_SHALLOW_END_Y, FRAMEBUFFER_COLOR_BLACK);

    TEST_ASSERT_TRUE(prv_is_ink(TEST_SHALLOW_START_X, TEST_SHALLOW_START_Y));
    TEST_ASSERT_TRUE(prv_is_ink(TEST_SHALLOW_END_X, TEST_SHALLOW_END_Y));
}

void test_a_steep_line_inks_both_of_its_endpoints(void)
{
    /* The steep case takes the mirrored path through Bresenham, so it is worth its
     * own check. */
    gfx_line(&g_framebuffer, TEST_STEEP_START_X, TEST_STEEP_START_Y, TEST_STEEP_END_X,
             TEST_STEEP_END_Y, FRAMEBUFFER_COLOR_BLACK);

    TEST_ASSERT_TRUE(prv_is_ink(TEST_STEEP_START_X, TEST_STEEP_START_Y));
    TEST_ASSERT_TRUE(prv_is_ink(TEST_STEEP_END_X, TEST_STEEP_END_Y));
}

void test_a_line_drawn_backwards_covers_the_same_pixels(void)
{
    uint32_t forward_count;

    gfx_line(&g_framebuffer, TEST_REVERSIBLE_START_X, TEST_REVERSIBLE_START_Y,
             TEST_REVERSIBLE_END_X, TEST_REVERSIBLE_END_Y, FRAMEBUFFER_COLOR_BLACK);
    forward_count = prv_count_ink();

    framebuffer_clear(&g_framebuffer);
    gfx_line(&g_framebuffer, TEST_REVERSIBLE_END_X, TEST_REVERSIBLE_END_Y,
             TEST_REVERSIBLE_START_X, TEST_REVERSIBLE_START_Y, FRAMEBUFFER_COLOR_BLACK);

    TEST_ASSERT_EQUAL_UINT32(forward_count, prv_count_ink());
    TEST_ASSERT_TRUE(prv_is_ink(TEST_REVERSIBLE_START_X, TEST_REVERSIBLE_START_Y));
    TEST_ASSERT_TRUE(prv_is_ink(TEST_REVERSIBLE_END_X, TEST_REVERSIBLE_END_Y));
}

/* --- rectangles ----------------------------------------------------------- */

void test_a_rectangle_outline_inks_its_border_only(void)
{
    gfx_rectangle(&g_framebuffer, TEST_RECT_X, TEST_RECT_Y, TEST_RECT_WIDTH, TEST_RECT_HEIGHT,
                  FRAMEBUFFER_COLOR_BLACK);

    /* Four corners present, interior empty. */
    TEST_ASSERT_TRUE(prv_is_ink(TEST_RECT_X, TEST_RECT_Y));
    TEST_ASSERT_TRUE(prv_is_ink(TEST_RECT_X + TEST_RECT_WIDTH - 1, TEST_RECT_Y));
    TEST_ASSERT_TRUE(prv_is_ink(TEST_RECT_X, TEST_RECT_Y + TEST_RECT_HEIGHT - 1));
    TEST_ASSERT_TRUE(
        prv_is_ink(TEST_RECT_X + TEST_RECT_WIDTH - 1, TEST_RECT_Y + TEST_RECT_HEIGHT - 1));
    TEST_ASSERT_FALSE(prv_is_ink(TEST_RECT_INSIDE_X, TEST_RECT_INSIDE_Y));

    /* Perimeter of a 10x8 outline, corners not double-counted. */
    TEST_ASSERT_EQUAL_UINT32((2U * TEST_RECT_WIDTH) + (2U * TEST_RECT_HEIGHT)
                                 - TEST_RECT_CORNER_COUNT,
                             prv_count_ink());
}

void test_a_filled_rectangle_inks_its_whole_area(void)
{
    gfx_filled_rectangle(&g_framebuffer, TEST_RECT_X, TEST_RECT_Y, TEST_RECT_WIDTH,
                         TEST_RECT_HEIGHT, FRAMEBUFFER_COLOR_BLACK);

    TEST_ASSERT_EQUAL_UINT32((uint32_t)TEST_RECT_WIDTH * TEST_RECT_HEIGHT, prv_count_ink());
    TEST_ASSERT_TRUE(prv_is_ink(TEST_RECT_INSIDE_X, TEST_RECT_INSIDE_Y));
}

void test_white_draws_over_black(void)
{
    framebuffer_fill(&g_framebuffer, FRAMEBUFFER_COLOR_BLACK);
    gfx_filled_rectangle(&g_framebuffer, 0, 0, TEST_OVERDRAW_SIDE, TEST_OVERDRAW_SIDE,
                         FRAMEBUFFER_COLOR_WHITE);

    TEST_ASSERT_FALSE(prv_is_ink(0, 0));
    TEST_ASSERT_FALSE(prv_is_ink(TEST_OVERDRAW_SIDE - 1, TEST_OVERDRAW_SIDE - 1));
    TEST_ASSERT_TRUE(prv_is_ink(TEST_OVERDRAW_SIDE, TEST_OVERDRAW_SIDE));
}

/* --- circles and triangles ------------------------------------------------ */

void test_a_circle_outline_inks_its_axis_points_but_not_its_centre(void)
{
    const int16_t center = TEST_CIRCLE_CENTER;
    const int16_t radius = TEST_OUTLINE_RADIUS;

    gfx_circle(&g_framebuffer, center, center, radius, FRAMEBUFFER_COLOR_BLACK);

    TEST_ASSERT_TRUE(prv_is_ink(center, center - radius));
    TEST_ASSERT_TRUE(prv_is_ink(center, center + radius));
    TEST_ASSERT_TRUE(prv_is_ink(center - radius, center));
    TEST_ASSERT_TRUE(prv_is_ink(center + radius, center));
    TEST_ASSERT_FALSE(prv_is_ink(center, center));
}

void test_a_filled_circle_inks_its_centre_and_stays_within_its_radius(void)
{
    const int16_t center = TEST_CIRCLE_CENTER;
    const int16_t radius = TEST_FILLED_RADIUS;

    gfx_filled_circle(&g_framebuffer, center, center, radius, FRAMEBUFFER_COLOR_BLACK);

    TEST_ASSERT_TRUE(prv_is_ink(center, center));
    TEST_ASSERT_TRUE(prv_is_ink(center, center - radius));
    /* Just outside the radius on the diagonal, and one pixel past it on the axis. */
    TEST_ASSERT_FALSE(prv_is_ink(center + radius, center + radius));
    TEST_ASSERT_FALSE(prv_is_ink(center, center - radius - 1));
}

void test_a_filled_triangle_inks_its_interior(void)
{
    gfx_filled_triangle(&g_framebuffer, TEST_TRIANGLE_LEFT_X, TEST_TRIANGLE_BASE_Y,
                        TEST_TRIANGLE_APEX_X, TEST_TRIANGLE_APEX_Y, TEST_TRIANGLE_RIGHT_X,
                        TEST_TRIANGLE_BASE_Y, FRAMEBUFFER_COLOR_BLACK);

    TEST_ASSERT_TRUE(prv_is_ink(TEST_TRIANGLE_APEX_X, TEST_TRIANGLE_INSIDE_Y));
    TEST_ASSERT_TRUE(prv_is_ink(TEST_TRIANGLE_APEX_X, TEST_TRIANGLE_APEX_Y));
    TEST_ASSERT_FALSE(prv_is_ink(TEST_TRIANGLE_LEFT_X, TEST_TRIANGLE_APEX_Y));
}

void test_a_degenerate_triangle_draws_a_single_row(void)
{
    /* All three corners on one row — the special case in the scanline fill. */
    gfx_filled_triangle(&g_framebuffer, TEST_FLAT_LEFT_X, TEST_FLAT_ROW, TEST_FLAT_RIGHT_X,
                        TEST_FLAT_ROW, TEST_FLAT_MIDDLE_X, TEST_FLAT_ROW,
                        FRAMEBUFFER_COLOR_BLACK);

    TEST_ASSERT_EQUAL_UINT32(TEST_FLAT_INK_COUNT, prv_count_ink());
    TEST_ASSERT_TRUE(prv_is_ink(TEST_FLAT_LEFT_X, TEST_FLAT_ROW));
    TEST_ASSERT_TRUE(prv_is_ink(TEST_FLAT_RIGHT_X, TEST_FLAT_ROW));
    TEST_ASSERT_FALSE(prv_is_ink(TEST_FLAT_RIGHT_X + 1, TEST_FLAT_ROW));
}

/* --- clipping ------------------------------------------------------------- */

void test_shapes_may_hang_over_the_edges(void)
{
    /* Straddling every edge at once: nothing should be written outside, and the run
     * must not fault. */
    gfx_filled_rectangle(&g_framebuffer, -TEST_OVERHANG, -TEST_OVERHANG,
                         FRAMEBUFFER_WIDTH + TEST_OVERHANG_MARGIN,
                         FRAMEBUFFER_HEIGHT + TEST_OVERHANG_MARGIN, FRAMEBUFFER_COLOR_BLACK);

    TEST_ASSERT_EQUAL_UINT32((uint32_t)FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT, prv_count_ink());
}

void test_a_shape_entirely_off_screen_inks_nothing(void)
{
    gfx_filled_circle(&g_framebuffer, TEST_OFF_SCREEN_CENTER, TEST_OFF_SCREEN_CENTER,
                      TEST_OFF_SCREEN_RADIUS, FRAMEBUFFER_COLOR_BLACK);
    gfx_line(&g_framebuffer, TEST_OFF_SCREEN_START, TEST_OFF_SCREEN_START, TEST_OFF_SCREEN_END,
             TEST_OFF_SCREEN_END, FRAMEBUFFER_COLOR_BLACK);

    TEST_ASSERT_EQUAL_UINT32(0U, prv_count_ink());
}
