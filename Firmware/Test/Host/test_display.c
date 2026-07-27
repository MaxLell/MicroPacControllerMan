/*
 * Unit tests for the target implementation of the display port (Drivers/display).
 *
 * The panel driver sits above the BSP, so it can be tested with spi_bsp and dio_bsp
 * mocked: capture every byte the driver would clock out and assert on the wire format.
 *
 * This is what replaces looking at the panel. The frame buffer stores a set bit as
 * ink, while the LS013B7DH03 wants a set bit to mean white, so the driver inverts on
 * the way out — get that backwards and the display shows a photographic negative,
 * which no build or on-target PASS/FAIL would ever catch.
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "assert_probe.h"
#include "custom_assert.h"
#include "display.h"
#include "framebuffer.h"
#include "mock_dio_bsp.h"
#include "mock_spi_bsp.h"
#include "unity.h"

/* Enough for a full frame: header + 128 x (address + 16 data + padding) + trailer. */
#define CAPTURE_MAX_BYTES (4096U)

#define COMMAND_WRITE_LINE (0x01U)
#define COMMAND_VCOM (0x02U)
#define COMMAND_CLEAR_ALL (0x04U)

#define BYTES_PER_LINE_RECORD (1U + FRAMEBUFFER_BYTES_PER_LINE + 1U)

#define FIRST_LINE (0U)
#define SECOND_LINE (1U)
#define LAST_LINE (FRAMEBUFFER_HEIGHT - 1U)

/* Wire-format expectations. The panel wants a set bit for white, so an un-inked buffer
 * goes out as all ones and a fully inked one as all zeroes. */
#define WIRE_ALL_WHITE (0xFFU)
#define WIRE_ALL_INK (0x00U)
#define PADDING_BYTE (0x00U)

/* First data byte of a line record, past its address byte. */
#define FIRST_DATA_BYTE_OFFSET (1U)
#define SECOND_DATA_BYTE_OFFSET (2U)

/* A lone inked pixel at each end of a byte, and the byte each produces on the wire. */
#define LONE_PIXEL_LOW_X (0)
#define LONE_PIXEL_HIGH_X (7)
#define LONE_PIXEL_ROW (5)
#define UNTOUCHED_ROW (4)
#define WIRE_LOW_BIT_INKED (0xFEU)
#define WIRE_HIGH_BIT_INKED (0x7FU)

/* A command frame is the command plus one padding byte. */
#define COMMAND_FRAME_BYTES (2U)

static uint8_t g_captured[CAPTURE_MAX_BYTES];
static size_t g_captured_length;
static framebuffer_t g_framebuffer;

/* Stand in for spi_bsp_write and record the bytes. */
static void prv_capture_spi(const uint8_t* const in_data, size_t in_length, int in_call_count)
{
    (void)in_call_count;

    TEST_ASSERT_NOT_NULL(in_data);
    TEST_ASSERT_TRUE((g_captured_length + in_length) <= sizeof(g_captured));

    memcpy(&g_captured[g_captured_length], in_data, in_length);
    g_captured_length += in_length;
}

/* Offset of one line's record within a captured frame (header occupies byte 0). */
static size_t prv_line_offset(uint16_t in_line)
{
    return 1U + ((size_t)in_line * BYTES_PER_LINE_RECORD);
}

void setUp(void)
{
    assert_probe_begin();

    g_captured_length = 0U;
    memset(g_captured, 0, sizeof(g_captured));

    framebuffer_clear(&g_framebuffer);

    dio_bsp_set_pin_Ignore();
    dio_bsp_toggle_pin_Ignore();
    spi_bsp_write_StubWithCallback(prv_capture_spi);
}

void tearDown(void)
{
    assert_probe_end();
}

/* --- frame format --------------------------------------------------------- */

void test_presenting_a_frame_clocks_out_the_expected_byte_count(void)
{
    display_present(&g_framebuffer);

    /* header + one record per line + closing trailer */
    TEST_ASSERT_EQUAL_UINT32(1U + (FRAMEBUFFER_HEIGHT * BYTES_PER_LINE_RECORD) + 1U,
                             (uint32_t)g_captured_length);
}

void test_the_frame_header_requests_a_line_write(void)
{
    display_present(&g_framebuffer);

    TEST_ASSERT_TRUE((g_captured[0] & COMMAND_WRITE_LINE) != 0U);
}

void test_line_addresses_are_one_based_and_ascending(void)
{
    display_present(&g_framebuffer);

    TEST_ASSERT_EQUAL_UINT8(1U, g_captured[prv_line_offset(FIRST_LINE)]);
    TEST_ASSERT_EQUAL_UINT8(2U, g_captured[prv_line_offset(SECOND_LINE)]);
    TEST_ASSERT_EQUAL_UINT8(FRAMEBUFFER_HEIGHT, g_captured[prv_line_offset(LAST_LINE)]);
}

void test_every_line_record_ends_with_a_padding_byte(void)
{
    display_present(&g_framebuffer);

    for (uint16_t line = 0U; line < FRAMEBUFFER_HEIGHT; ++line)
    {
        const size_t padding_index = prv_line_offset(line) + 1U + FRAMEBUFFER_BYTES_PER_LINE;

        TEST_ASSERT_EQUAL_UINT8(PADDING_BYTE, g_captured[padding_index]);
    }
}

/* --- bit polarity: the reason this test file exists ----------------------- */

void test_an_empty_frame_goes_out_as_all_ones(void)
{
    display_present(&g_framebuffer);

    /* No ink anywhere, so every data bit on the wire must be white, i.e. set. */
    for (uint16_t byte = 0U; byte < FRAMEBUFFER_BYTES_PER_LINE; ++byte)
    {
        TEST_ASSERT_EQUAL_HEX8(WIRE_ALL_WHITE,
                               g_captured[prv_line_offset(FIRST_LINE) + FIRST_DATA_BYTE_OFFSET + byte]);
    }
}

void test_a_fully_inked_frame_goes_out_as_all_zeroes(void)
{
    framebuffer_fill(&g_framebuffer, FRAMEBUFFER_COLOR_BLACK);

    display_present(&g_framebuffer);

    for (uint16_t byte = 0U; byte < FRAMEBUFFER_BYTES_PER_LINE; ++byte)
    {
        TEST_ASSERT_EQUAL_HEX8(WIRE_ALL_INK,
                               g_captured[prv_line_offset(FIRST_LINE) + FIRST_DATA_BYTE_OFFSET + byte]);
    }
}

void test_a_single_inked_pixel_clears_exactly_its_bit_on_the_wire(void)
{
    /* Pixel 0 of row 5 is bit 0 of that row's first data byte. Inked in the buffer, so
     * cleared on the wire; its seven neighbours stay set. */
    framebuffer_set_pixel(&g_framebuffer, LONE_PIXEL_LOW_X, LONE_PIXEL_ROW,
                          FRAMEBUFFER_COLOR_BLACK);

    display_present(&g_framebuffer);

    TEST_ASSERT_EQUAL_HEX8(WIRE_LOW_BIT_INKED,
                           g_captured[prv_line_offset(LONE_PIXEL_ROW) + FIRST_DATA_BYTE_OFFSET]);
    TEST_ASSERT_EQUAL_HEX8(WIRE_ALL_WHITE,
                           g_captured[prv_line_offset(LONE_PIXEL_ROW) + SECOND_DATA_BYTE_OFFSET]);
    /* A different row is untouched. */
    TEST_ASSERT_EQUAL_HEX8(WIRE_ALL_WHITE,
                           g_captured[prv_line_offset(UNTOUCHED_ROW) + FIRST_DATA_BYTE_OFFSET]);
}

void test_the_pixel_bit_order_within_a_byte_is_preserved(void)
{
    framebuffer_set_pixel(&g_framebuffer, LONE_PIXEL_HIGH_X, FIRST_LINE,
                          FRAMEBUFFER_COLOR_BLACK);

    display_present(&g_framebuffer);

    /* The last pixel of a byte is its most significant bit. */
    TEST_ASSERT_EQUAL_HEX8(WIRE_HIGH_BIT_INKED,
                           g_captured[prv_line_offset(FIRST_LINE) + FIRST_DATA_BYTE_OFFSET]);
}

/* --- VCOM and commands ---------------------------------------------------- */

void test_the_vcom_bit_alternates_between_frames(void)
{
    uint8_t first_header;
    uint8_t second_header;

    display_present(&g_framebuffer);
    first_header = g_captured[0];

    g_captured_length = 0U;
    display_present(&g_framebuffer);
    second_header = g_captured[0];

    TEST_ASSERT_NOT_EQUAL(first_header & COMMAND_VCOM, second_header & COMMAND_VCOM);
}

void test_clearing_sends_the_clear_all_command(void)
{
    display_clear();

    TEST_ASSERT_EQUAL_UINT32(COMMAND_FRAME_BYTES, (uint32_t)g_captured_length);
    TEST_ASSERT_TRUE((g_captured[0] & COMMAND_CLEAR_ALL) != 0U);
    TEST_ASSERT_EQUAL_UINT8(PADDING_BYTE, g_captured[1]);
}

void test_servicing_the_display_sends_no_line_write(void)
{
    display_service();

    /* Upkeep only: a command frame that must not ask the panel to update. */
    TEST_ASSERT_EQUAL_UINT32(COMMAND_FRAME_BYTES, (uint32_t)g_captured_length);
    TEST_ASSERT_EQUAL_UINT8(0U, g_captured[0] & COMMAND_WRITE_LINE);
}

/* --- chip select ---------------------------------------------------------- */

void test_a_frame_is_framed_by_chip_select(void)
{
    dio_bsp_set_pin_StopIgnore();

    dio_bsp_set_pin_Expect(DIO_BSP_PIN_DISPLAY_CS, DIO_BSP_PIN_STATE_HIGH);
    dio_bsp_set_pin_Expect(DIO_BSP_PIN_DISPLAY_CS, DIO_BSP_PIN_STATE_LOW);

    display_present(&g_framebuffer);
}

/* --- preconditions -------------------------------------------------------- */

void test_presenting_a_null_frame_asserts(void)
{
    /* The frame buffer pointer is dereferenced straight away, so this must not be
     * allowed to proceed. */
    ASSERT_PROBE_EXPECT(display_present(NULL), "in_framebuffer != NULL");
}
