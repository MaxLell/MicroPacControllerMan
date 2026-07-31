#include "st7789.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "custom_assert.h"
#include "delay.h"
#include "dio_bsp.h"
#include "spi_bsp.h"

/* ==========================================================================
 * st7789 - private
 * ========================================================================= */

#define ST7789_CMD_SWRESET       (0x01U)
#define ST7789_CMD_SLPOUT        (0x11U)
#define ST7789_CMD_NORON         (0x13U)
#define ST7789_CMD_INVOFF        (0x20U)
#define ST7789_CMD_INVON         (0x21U)
#define ST7789_CMD_DISPOFF       (0x28U)
#define ST7789_CMD_DISPON        (0x29U)
#define ST7789_CMD_CASET         (0x2AU)
#define ST7789_CMD_RASET         (0x2BU)
#define ST7789_CMD_RAMWR         (0x2CU)
#define ST7789_CMD_MADCTL        (0x36U)
#define ST7789_CMD_COLMOD        (0x3AU)
#define ST7789_CMD_RDID1         (0xDAU)

/* 16 bits per pixel, RGB565, for both the interface and the frame memory. */
#define ST7789_COLMOD_RGB565     (0x55U)

/* Memory access control: no row/column exchange, no mirroring, RGB order. This is
 * the panel's native portrait orientation — 240 wide, 320 tall. */
#define ST7789_MADCTL_PORTRAIT   (0x00U)

/* Off, measured rather than assumed. Most ST7789 modules need inversion on, so that is
 * what this started at — and the panel then showed every colour as its complement: a
 * yellow disc came out blue, which in RGB565 is exactly ~0xFFE0 = 0x001F. This panel
 * does not want it. */
#define ST7789_USE_INVERSION     (false)

/* Reset timing, generous against the datasheet: RESX low for at least 10 us, up to
 * 120 ms before the controller accepts commands, and the same again after sleep-out. */
#define ST7789_RESET_LOW_MS      (20U)
#define ST7789_RESET_SETTLE_MS   (150U)
#define ST7789_SLEEP_OUT_MS      (150U)
#define ST7789_DISPLAY_ON_MS     (20U)

/* Pixels pushed per SPI transfer while filling. A larger buffer means fewer HAL
 * calls; 64 pixels is 128 bytes, which costs little RAM and already amortises the
 * per-call overhead. */
#define ST7789_FILL_CHUNK_PIXELS (64U)

/* Read behaviour, as measured on this board rather than as the datasheet describes it.
 *
 * RDDID (04h) comes back one clock cycle late: it gave 42 C2 A9, which shifted left by
 * a single bit is exactly the identity 85 85 52. The one-byte registers RDID1..3 do
 * not show that shift — RDID1 returned 85 in the very first byte. So those are read
 * plainly, one byte, and RDDID is avoided. */
#define ST7789_ID_READ_LENGTH    (1U)

static bool g_is_initialized = false;

static void prv_select(bool in_is_selected)
{
    /* Active LOW. UM2750 claims active high; the board disagrees, and so does the
     * ST7789V datasheet — see the M2 design document. */
    dio_bsp_set_pin(DIO_BSP_PIN_DISPLAY_CS, in_is_selected ? DIO_BSP_PIN_STATE_LOW : DIO_BSP_PIN_STATE_HIGH);
}

static void prv_write_command(uint8_t in_command)
{
    dio_bsp_set_pin(DIO_BSP_PIN_DISPLAY_DCX, DIO_BSP_PIN_STATE_LOW);
    spi_bsp_write(&in_command, sizeof(in_command));
    dio_bsp_set_pin(DIO_BSP_PIN_DISPLAY_DCX, DIO_BSP_PIN_STATE_HIGH);
}

static void prv_write_data(const uint8_t* const in_data, size_t in_length)
{
    dio_bsp_set_pin(DIO_BSP_PIN_DISPLAY_DCX, DIO_BSP_PIN_STATE_HIGH);
    spi_bsp_write(in_data, in_length);
}

static void prv_command(uint8_t in_command)
{
    prv_select(true);
    prv_write_command(in_command);
    prv_select(false);
}

static void prv_command_with_data(uint8_t in_command, const uint8_t* const in_data, size_t in_length)
{
    prv_select(true);
    prv_write_command(in_command);
    prv_write_data(in_data, in_length);
    prv_select(false);
}

/* Sets the rectangle that subsequent pixel writes fill, then opens RAM for writing.
 * Leaves the panel selected and DCX high, ready for pixel data. */
static void prv_open_window(uint16_t in_x, uint16_t in_y, uint16_t in_width, uint16_t in_height)
{
    const uint16_t x_end = (uint16_t)(in_x + in_width - 1U);
    const uint16_t y_end = (uint16_t)(in_y + in_height - 1U);
    const uint8_t columns[] = {(uint8_t)(in_x >> 8), (uint8_t)in_x, (uint8_t)(x_end >> 8), (uint8_t)x_end};
    const uint8_t rows[] = {(uint8_t)(in_y >> 8), (uint8_t)in_y, (uint8_t)(y_end >> 8), (uint8_t)y_end};

    prv_command_with_data(ST7789_CMD_CASET, columns, sizeof(columns));
    prv_command_with_data(ST7789_CMD_RASET, rows, sizeof(rows));

    prv_select(true);
    prv_write_command(ST7789_CMD_RAMWR);
}

/* ==========================================================================
 * st7789 - public
 * ========================================================================= */

void st7789_init(void)
{
    const uint8_t colour_mode = ST7789_COLMOD_RGB565;
    const uint8_t memory_access = ST7789_MADCTL_PORTRAIT;

    ASSERT(false == g_is_initialized);

    prv_select(false);

    dio_bsp_set_pin(DIO_BSP_PIN_DISPLAY_RESET, DIO_BSP_PIN_STATE_LOW);
    delay_ms(ST7789_RESET_LOW_MS);
    dio_bsp_set_pin(DIO_BSP_PIN_DISPLAY_RESET, DIO_BSP_PIN_STATE_HIGH);
    delay_ms(ST7789_RESET_SETTLE_MS);

    prv_command(ST7789_CMD_SWRESET);
    delay_ms(ST7789_RESET_SETTLE_MS);

    prv_command(ST7789_CMD_SLPOUT);
    delay_ms(ST7789_SLEEP_OUT_MS);

    prv_command_with_data(ST7789_CMD_COLMOD, &colour_mode, sizeof(colour_mode));
    prv_command_with_data(ST7789_CMD_MADCTL, &memory_access, sizeof(memory_access));

    prv_command(ST7789_USE_INVERSION ? ST7789_CMD_INVON : ST7789_CMD_INVOFF);
    prv_command(ST7789_CMD_NORON);

    g_is_initialized = true;

    /* The controller powers up with whatever was in its RAM; start from black rather
     * than from noise. */
    st7789_fill_screen(ST7789_RGB(0U, 0U, 0U));

    prv_command(ST7789_CMD_DISPON);
    delay_ms(ST7789_DISPLAY_ON_MS);
}

void st7789_read_id(uint8_t* out_id)
{
    const uint8_t zeros[ST7789_ID_READ_LENGTH] = {0U};

    ASSERT(out_id != NULL);

    for (uint8_t index = 0U; index < ST7789_ID_LENGTH; ++index)
    {
        uint8_t received[ST7789_ID_READ_LENGTH] = {0U};

        prv_select(true);
        prv_write_command((uint8_t)(ST7789_CMD_RDID1 + index));
        spi_bsp_transfer(zeros, received, sizeof(received));
        prv_select(false);

        out_id[index] = received[0];
    }
}

bool st7789_is_present(void)
{
    uint8_t id[ST7789_ID_LENGTH] = {0U};

    st7789_read_id(id);

    return (id[0] == ST7789_ID_EXPECTED_0) && (id[1] == ST7789_ID_EXPECTED_1) && (id[2] == ST7789_ID_EXPECTED_2);
}

void st7789_set_display_on(bool in_is_on)
{
    ASSERT(g_is_initialized);

    prv_command(in_is_on ? ST7789_CMD_DISPON : ST7789_CMD_DISPOFF);
}

void st7789_fill_rectangle(uint16_t in_x, uint16_t in_y, uint16_t in_width, uint16_t in_height, uint16_t in_colour)
{
    uint8_t chunk[ST7789_FILL_CHUNK_PIXELS * 2U];
    uint32_t remaining;

    ASSERT(g_is_initialized);
    ASSERT(in_width > 0U);
    ASSERT(in_height > 0U);
    ASSERT((uint32_t)in_x + in_width <= ST7789_WIDTH);
    ASSERT((uint32_t)in_y + in_height <= ST7789_HEIGHT);

    for (size_t index = 0U; index < ST7789_FILL_CHUNK_PIXELS; ++index)
    {
        chunk[index * 2U] = (uint8_t)(in_colour >> 8);
        chunk[(index * 2U) + 1U] = (uint8_t)in_colour;
    }

    prv_open_window(in_x, in_y, in_width, in_height);

    remaining = (uint32_t)in_width * in_height;

    while (remaining > 0U)
    {
        const uint32_t pixels = (remaining < ST7789_FILL_CHUNK_PIXELS) ? remaining : ST7789_FILL_CHUNK_PIXELS;

        spi_bsp_write(chunk, (size_t)pixels * 2U);

        remaining -= pixels;
    }

    prv_select(false);
}

void st7789_fill_screen(uint16_t in_colour)
{
    st7789_fill_rectangle(0U, 0U, ST7789_WIDTH, ST7789_HEIGHT, in_colour);
}

void st7789_write_pixels(uint16_t in_x, uint16_t in_y, uint16_t in_width, uint16_t in_height, const uint16_t* in_pixels,
                         uint16_t in_stride)
{
    /* One row at a time: the controller wants each pixel most-significant byte first,
     * which is the reverse of how a uint16_t sits in memory here, so the row has to be
     * repacked rather than handed over as-is. A full row is 480 bytes, which is cheap
     * to hold and keeps the number of SPI calls to one per row. */
    uint8_t row[ST7789_WIDTH * 2U];

    ASSERT(g_is_initialized);
    ASSERT(in_pixels != NULL);
    ASSERT(in_width > 0U);
    ASSERT(in_height > 0U);
    ASSERT(in_stride >= in_width);
    ASSERT((uint32_t)in_x + in_width <= ST7789_WIDTH);
    ASSERT((uint32_t)in_y + in_height <= ST7789_HEIGHT);

    prv_open_window(in_x, in_y, in_width, in_height);

    for (uint16_t line = 0U; line < in_height; ++line)
    {
        const uint16_t* const source = &in_pixels[(uint32_t)line * in_stride];

        for (uint16_t column = 0U; column < in_width; ++column)
        {
            row[column * 2U] = (uint8_t)(source[column] >> 8);
            row[(column * 2U) + 1U] = (uint8_t)source[column];
        }

        spi_bsp_write(row, (size_t)in_width * 2U);
    }

    prv_select(false);
}
