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
    framebuffer_set_pixel(&g_framebuffer, 3, 7, FRAMEBUFFER_COLOR_BLACK);

    TEST_ASSERT_EQUAL(FRAMEBUFFER_COLOR_BLACK, framebuffer_get_pixel(&g_framebuffer, 3, 7));
    TEST_ASSERT_EQUAL_UINT32(1U, prv_count_ink());
}

void test_a_pixel_can_be_cleared_again(void)
{
    framebuffer_set_pixel(&g_framebuffer, 3, 7, FRAMEBUFFER_COLOR_BLACK);
    framebuffer_set_pixel(&g_framebuffer, 3, 7, FRAMEBUFFER_COLOR_WHITE);

    TEST_ASSERT_EQUAL(FRAMEBUFFER_COLOR_WHITE, framebuffer_get_pixel(&g_framebuffer, 3, 7));
    TEST_ASSERT_EQUAL_UINT32(0U, prv_count_ink());
}

void test_pixels_sharing_a_byte_are_independent(void)
{
    /* Both ends of the first byte, plus the first pixel of the second. */
    framebuffer_set_pixel(&g_framebuffer, 0, 0, FRAMEBUFFER_COLOR_BLACK);
    framebuffer_set_pixel(&g_framebuffer, 7, 0, FRAMEBUFFER_COLOR_BLACK);
    framebuffer_set_pixel(&g_framebuffer, 8, 0, FRAMEBUFFER_COLOR_BLACK);

    framebuffer_set_pixel(&g_framebuffer, 7, 0, FRAMEBUFFER_COLOR_WHITE);

    TEST_ASSERT_EQUAL(FRAMEBUFFER_COLOR_BLACK, framebuffer_get_pixel(&g_framebuffer, 0, 0));
    TEST_ASSERT_EQUAL(FRAMEBUFFER_COLOR_WHITE, framebuffer_get_pixel(&g_framebuffer, 7, 0));
    TEST_ASSERT_EQUAL(FRAMEBUFFER_COLOR_BLACK, framebuffer_get_pixel(&g_framebuffer, 8, 0));
    TEST_ASSERT_EQUAL_UINT32(2U, prv_count_ink());
}

void test_rows_are_independent(void)
{
    framebuffer_set_pixel(&g_framebuffer, 5, 0, FRAMEBUFFER_COLOR_BLACK);

    TEST_ASSERT_EQUAL(FRAMEBUFFER_COLOR_WHITE, framebuffer_get_pixel(&g_framebuffer, 5, 1));
    TEST_ASSERT_EQUAL_UINT32(1U, prv_count_ink());
}

void test_the_far_corner_is_addressable(void)
{
    framebuffer_set_pixel(&g_framebuffer, FRAMEBUFFER_WIDTH - 1, FRAMEBUFFER_HEIGHT - 1,
                          FRAMEBUFFER_COLOR_BLACK);

    TEST_ASSERT_EQUAL(FRAMEBUFFER_COLOR_BLACK,
                      framebuffer_get_pixel(&g_framebuffer, FRAMEBUFFER_WIDTH - 1,
                                            FRAMEBUFFER_HEIGHT - 1));
    TEST_ASSERT_EQUAL_UINT32(1U, prv_count_ink());
}

/* --- clipping ------------------------------------------------------------- */

void test_pixels_outside_the_buffer_are_dropped(void)
{
    framebuffer_set_pixel(&g_framebuffer, -1, 0, FRAMEBUFFER_COLOR_BLACK);
    framebuffer_set_pixel(&g_framebuffer, 0, -1, FRAMEBUFFER_COLOR_BLACK);
    framebuffer_set_pixel(&g_framebuffer, FRAMEBUFFER_WIDTH, 0, FRAMEBUFFER_COLOR_BLACK);
    framebuffer_set_pixel(&g_framebuffer, 0, FRAMEBUFFER_HEIGHT, FRAMEBUFFER_COLOR_BLACK);

    TEST_ASSERT_EQUAL_UINT32(0U, prv_count_ink());
}

void test_reading_outside_the_buffer_reports_white(void)
{
    framebuffer_fill(&g_framebuffer, FRAMEBUFFER_COLOR_BLACK);

    TEST_ASSERT_EQUAL(FRAMEBUFFER_COLOR_WHITE, framebuffer_get_pixel(&g_framebuffer, -1, 0));
    TEST_ASSERT_EQUAL(FRAMEBUFFER_COLOR_WHITE,
                      framebuffer_get_pixel(&g_framebuffer, FRAMEBUFFER_WIDTH, 0));
}

/* --- row access for a driver --------------------------------------------- */

void test_a_row_reflects_the_pixels_set_in_it(void)
{
    const uint8_t* row;

    framebuffer_set_pixel(&g_framebuffer, 0, 4, FRAMEBUFFER_COLOR_BLACK);
    framebuffer_set_pixel(&g_framebuffer, 9, 4, FRAMEBUFFER_COLOR_BLACK);

    row = framebuffer_get_line(&g_framebuffer, 4);

    /* Bit 0 of a byte is its left-most pixel, so pixel 0 -> byte 0 bit 0 and
     * pixel 9 -> byte 1 bit 1. */
    TEST_ASSERT_EQUAL_HEX8(0x01U, row[0]);
    TEST_ASSERT_EQUAL_HEX8(0x02U, row[1]);
}

/* --- preconditions -------------------------------------------------------- */

void test_a_null_buffer_asserts(void)
{
    ASSERT_PROBE_EXPECT(framebuffer_fill(NULL, FRAMEBUFFER_COLOR_BLACK),
                        "inout_framebuffer != NULL");
}

void test_a_row_outside_the_buffer_asserts(void)
{
    ASSERT_PROBE_EXPECT(framebuffer_get_line(&g_framebuffer, FRAMEBUFFER_HEIGHT),
                        "in_y < FRAMEBUFFER_HEIGHT");
}
