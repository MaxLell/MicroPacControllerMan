/*
 * Unit tests for Services/framebuffer.
 *
 * No mocks needed — the module is pure memory and bit arithmetic. The bit packing is
 * what these are really guarding: eight pixels share a byte, and getting the mask or
 * the shift wrong corrupts a neighbour rather than the pixel asked for, which is
 * invisible until something renders.
 */
#include <stdint.h>

#include "assert_probe.h"
#include "custom_assert.h"
#include "framebuffer.h"
#include "unity.h"

/* Coordinates chosen so the byte-packing cases are covered explicitly: the LSB and MSB
 * of one byte, the first pixel of the next, and a row other than zero. */
#define TEST_PIXEL_X                 (3)
#define TEST_PIXEL_Y                 (7)
#define TEST_BYTE_FIRST_PIXEL_X      (0)
#define TEST_BYTE_LAST_PIXEL_X       (7)
#define TEST_NEXT_BYTE_FIRST_PIXEL_X (8)
#define TEST_ROW                     (0)
#define TEST_OTHER_ROW               (1)

/* A row used for the packed-bit check, and two pixels inside it. */
#define TEST_PACKED_ROW              (4)
#define TEST_PACKED_PIXEL_LOW_X      (0)
#define TEST_PACKED_PIXEL_HIGH_X     (9)
/* Two colours that share no bits, so a row that returned the wrong pixel — or the
 * bytes of one pixel swapped — cannot pass by coincidence. */
#define TEST_ROW_COLOR_LOW           FRAMEBUFFER_COLOR_RED
#define TEST_ROW_COLOR_HIGH          FRAMEBUFFER_COLOR_BLUE

#define TEST_LONE_PIXEL_X            (5)

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

void setUp(void)
{
    assert_probe_begin();
    framebuffer_clear(&g_framebuffer);
}

void tearDown(void)
{
    assert_probe_end();
}

/* --- whole-buffer operations ---------------------------------------------- */

void test_a_cleared_buffer_holds_no_ink(void)
{
    TEST_ASSERT_EQUAL_UINT32(0U, prv_count_ink());
}

void test_filling_black_inks_every_pixel(void)
{
    framebuffer_fill(&g_framebuffer, FRAMEBUFFER_COLOR_BLACK);

    TEST_ASSERT_EQUAL_UINT32((uint32_t)FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT, prv_count_ink());
}

void test_filling_white_removes_every_pixel(void)
{
    framebuffer_fill(&g_framebuffer, FRAMEBUFFER_COLOR_BLACK);
    framebuffer_fill(&g_framebuffer, FRAMEBUFFER_COLOR_WHITE);

    TEST_ASSERT_EQUAL_UINT32(0U, prv_count_ink());
}

/* --- single pixels -------------------------------------------------------- */

void test_a_set_pixel_reads_back_black(void)
{
    framebuffer_set_pixel(&g_framebuffer, TEST_PIXEL_X, TEST_PIXEL_Y, FRAMEBUFFER_COLOR_BLACK);

    TEST_ASSERT_EQUAL(FRAMEBUFFER_COLOR_BLACK, framebuffer_get_pixel(&g_framebuffer, TEST_PIXEL_X, TEST_PIXEL_Y));
    TEST_ASSERT_EQUAL_UINT32(1U, prv_count_ink());
}

void test_a_pixel_can_be_cleared_again(void)
{
    framebuffer_set_pixel(&g_framebuffer, TEST_PIXEL_X, TEST_PIXEL_Y, FRAMEBUFFER_COLOR_BLACK);
    framebuffer_set_pixel(&g_framebuffer, TEST_PIXEL_X, TEST_PIXEL_Y, FRAMEBUFFER_COLOR_WHITE);

    TEST_ASSERT_EQUAL(FRAMEBUFFER_COLOR_WHITE, framebuffer_get_pixel(&g_framebuffer, TEST_PIXEL_X, TEST_PIXEL_Y));
    TEST_ASSERT_EQUAL_UINT32(0U, prv_count_ink());
}

void test_pixels_sharing_a_byte_are_independent(void)
{
    /* Both ends of the first byte, plus the first pixel of the second. */
    framebuffer_set_pixel(&g_framebuffer, TEST_BYTE_FIRST_PIXEL_X, TEST_ROW, FRAMEBUFFER_COLOR_BLACK);
    framebuffer_set_pixel(&g_framebuffer, TEST_BYTE_LAST_PIXEL_X, TEST_ROW, FRAMEBUFFER_COLOR_BLACK);
    framebuffer_set_pixel(&g_framebuffer, TEST_NEXT_BYTE_FIRST_PIXEL_X, TEST_ROW, FRAMEBUFFER_COLOR_BLACK);

    framebuffer_set_pixel(&g_framebuffer, TEST_BYTE_LAST_PIXEL_X, TEST_ROW, FRAMEBUFFER_COLOR_WHITE);

    TEST_ASSERT_EQUAL(FRAMEBUFFER_COLOR_BLACK,
                      framebuffer_get_pixel(&g_framebuffer, TEST_BYTE_FIRST_PIXEL_X, TEST_ROW));
    TEST_ASSERT_EQUAL(FRAMEBUFFER_COLOR_WHITE, framebuffer_get_pixel(&g_framebuffer, TEST_BYTE_LAST_PIXEL_X, TEST_ROW));
    TEST_ASSERT_EQUAL(FRAMEBUFFER_COLOR_BLACK,
                      framebuffer_get_pixel(&g_framebuffer, TEST_NEXT_BYTE_FIRST_PIXEL_X, TEST_ROW));
    TEST_ASSERT_EQUAL_UINT32(2U, prv_count_ink());
}

void test_rows_are_independent(void)
{
    framebuffer_set_pixel(&g_framebuffer, TEST_LONE_PIXEL_X, TEST_ROW, FRAMEBUFFER_COLOR_BLACK);

    TEST_ASSERT_EQUAL(FRAMEBUFFER_COLOR_WHITE,
                      framebuffer_get_pixel(&g_framebuffer, TEST_LONE_PIXEL_X, TEST_OTHER_ROW));
    TEST_ASSERT_EQUAL_UINT32(1U, prv_count_ink());
}

void test_the_far_corner_is_addressable(void)
{
    framebuffer_set_pixel(&g_framebuffer, FRAMEBUFFER_WIDTH - 1, FRAMEBUFFER_HEIGHT - 1, FRAMEBUFFER_COLOR_BLACK);

    TEST_ASSERT_EQUAL(FRAMEBUFFER_COLOR_BLACK,
                      framebuffer_get_pixel(&g_framebuffer, FRAMEBUFFER_WIDTH - 1, FRAMEBUFFER_HEIGHT - 1));
    TEST_ASSERT_EQUAL_UINT32(1U, prv_count_ink());
}

/* --- clipping ------------------------------------------------------------- */

void test_pixels_outside_the_buffer_are_dropped(void)
{
    framebuffer_set_pixel(&g_framebuffer, -1, TEST_ROW, FRAMEBUFFER_COLOR_BLACK);
    framebuffer_set_pixel(&g_framebuffer, TEST_ROW, -1, FRAMEBUFFER_COLOR_BLACK);
    framebuffer_set_pixel(&g_framebuffer, FRAMEBUFFER_WIDTH, TEST_ROW, FRAMEBUFFER_COLOR_BLACK);
    framebuffer_set_pixel(&g_framebuffer, TEST_ROW, FRAMEBUFFER_HEIGHT, FRAMEBUFFER_COLOR_BLACK);

    TEST_ASSERT_EQUAL_UINT32(0U, prv_count_ink());
}

void test_reading_outside_the_buffer_reports_white(void)
{
    framebuffer_fill(&g_framebuffer, FRAMEBUFFER_COLOR_BLACK);

    TEST_ASSERT_EQUAL(FRAMEBUFFER_COLOR_WHITE, framebuffer_get_pixel(&g_framebuffer, -1, TEST_ROW));
    TEST_ASSERT_EQUAL(FRAMEBUFFER_COLOR_WHITE, framebuffer_get_pixel(&g_framebuffer, FRAMEBUFFER_WIDTH, TEST_ROW));
}

/* --- row access for a driver --------------------------------------------- */

void test_a_row_reflects_the_pixels_set_in_it(void)
{
    const framebuffer_color_t* row;

    framebuffer_set_pixel(&g_framebuffer, TEST_PACKED_PIXEL_LOW_X, TEST_PACKED_ROW, TEST_ROW_COLOR_LOW);
    framebuffer_set_pixel(&g_framebuffer, TEST_PACKED_PIXEL_HIGH_X, TEST_PACKED_ROW, TEST_ROW_COLOR_HIGH);

    row = framebuffer_get_line(&g_framebuffer, TEST_PACKED_ROW);

    /* A row is one RGB565 value per pixel, so a pixel's column indexes it directly. */
    TEST_ASSERT_EQUAL_HEX16(TEST_ROW_COLOR_LOW, row[TEST_PACKED_PIXEL_LOW_X]);
    TEST_ASSERT_EQUAL_HEX16(TEST_ROW_COLOR_HIGH, row[TEST_PACKED_PIXEL_HIGH_X]);

    /* Untouched pixels keep the cleared background rather than bleeding a neighbour. */
    TEST_ASSERT_EQUAL_HEX16(FRAMEBUFFER_COLOR_WHITE, row[TEST_PACKED_PIXEL_LOW_X + 1]);
}

/* --- preconditions -------------------------------------------------------- */

void test_a_null_buffer_asserts(void)
{
    ASSERT_PROBE_EXPECT(framebuffer_fill(NULL, FRAMEBUFFER_COLOR_BLACK), "inout_framebuffer != NULL");
}

void test_a_row_outside_the_buffer_asserts(void)
{
    ASSERT_PROBE_EXPECT(framebuffer_get_line(&g_framebuffer, FRAMEBUFFER_HEIGHT), "in_y < FRAMEBUFFER_HEIGHT");
}
