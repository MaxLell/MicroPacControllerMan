#include "display.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "dio_bsp.h"
#include "spi_bsp.h"

/* ==========================================================================
 * display - private
 * ========================================================================= */

/* LS013B7DH03 mode-byte bits, in LSB-first wire order (matches the SPI setup). */
#define DISPLAY_COMMAND_WRITE_LINE (0x01U)
#define DISPLAY_COMMAND_VCOM (0x02U)
#define DISPLAY_COMMAND_CLEAR_ALL (0x04U)

#define DISPLAY_BITS_PER_BYTE (8)
#define DISPLAY_BYTES_PER_LINE (DISPLAY_WIDTH / DISPLAY_BITS_PER_BYTE)

/* Line addresses are 1-based on the wire. */
#define DISPLAY_FIRST_LINE_ADDRESS (1U)

/* Every command and every line is closed with a don't-care byte. */
#define DISPLAY_PADDING_BYTE (0x00U)

/* A frame-buffer byte of all-ones is all-white (panel-native polarity). */
#define DISPLAY_FRAME_BUFFER_WHITE_BYTE (0xFFU)

/* The panel wants ~6 us of setup and ~2 us of hold around chip-select.
 *
 * This is a spin count, not a time, so it scales inversely with the core clock. It
 * was tuned when the firmware ran at 16 MHz; the clock is now 170 MHz (DEC-004),
 * which shortened it by roughly 10x to an estimated 6-11 us — i.e. it now only
 * barely clears the 6 us setup requirement, and would silently violate it on any
 * further clock or compiler change. Widened so the margin is real again; the cost
 * is a few microseconds twice per command and twice per flush, which is noise next
 * to the ~2.3 kB the flush itself clocks out. */
#define DISPLAY_CHIP_SELECT_SETTLE_LOOPS (600U)

/* Panel-native bits: 1 = white, 0 = black. Bit 0 is the left-most pixel of a
 * byte, matching the LSB-first transfer. */
static uint8_t g_frame_buffer[DISPLAY_HEIGHT][DISPLAY_BYTES_PER_LINE];

/* Holds the VCOM command bit itself, so it can be OR-ed into a header directly. */
static uint8_t g_vcom_state;

static void prv_set_chip_select(dio_bsp_pin_state_e in_state)
{
    /* Chip-select is ACTIVE-HIGH on this panel. */
    dio_bsp_set_pin(DIO_BSP_PIN_DISPLAY_CS, in_state);
}

static void prv_wait_chip_select_settle(void)
{
    for (volatile uint32_t loop = 0U; loop < DISPLAY_CHIP_SELECT_SETTLE_LOOPS; ++loop) {}
}

static uint8_t prv_toggle_vcom(void)
{
    g_vcom_state ^= DISPLAY_COMMAND_VCOM;

    return g_vcom_state;
}

/* Send a bare two-byte command frame, chip-select included. */
static void prv_send_command(uint8_t in_command)
{
    const uint8_t frame[] = {in_command, DISPLAY_PADDING_BYTE};

    prv_set_chip_select(DIO_BSP_PIN_STATE_HIGH);
    prv_wait_chip_select_settle();

    spi_bsp_write(frame, sizeof(frame));

    prv_wait_chip_select_settle();
    prv_set_chip_select(DIO_BSP_PIN_STATE_LOW);
}

/* ==========================================================================
 * display - public
 * ========================================================================= */

void display_init(void)
{
    prv_set_chip_select(DIO_BSP_PIN_STATE_LOW);
    dio_bsp_set_pin(DIO_BSP_PIN_DISPLAY_EXTCOMIN, DIO_BSP_PIN_STATE_LOW);

    display_set_enabled(true);

    g_vcom_state = 0U;

    display_clear_all();
}

void display_clear(void)
{
    memset(g_frame_buffer, DISPLAY_FRAME_BUFFER_WHITE_BYTE, sizeof(g_frame_buffer));
}

void display_set_pixel(int16_t in_x, int16_t in_y, display_color_e in_color)
{
    uint8_t mask;

    if ((in_x < 0) || (in_x >= DISPLAY_WIDTH) || (in_y < 0) || (in_y >= DISPLAY_HEIGHT))
    {
        return;
    }

    mask = (uint8_t)(1U << (in_x % DISPLAY_BITS_PER_BYTE));

    if (in_color == DISPLAY_COLOR_BLACK)
    {
        g_frame_buffer[in_y][in_x / DISPLAY_BITS_PER_BYTE] &= (uint8_t)~mask;
    }
    else
    {
        g_frame_buffer[in_y][in_x / DISPLAY_BITS_PER_BYTE] |= mask;
    }
}

void display_flush(void)
{
    const uint8_t padding = DISPLAY_PADDING_BYTE;
    uint8_t header;
    uint8_t line_address;

    header = (uint8_t)(DISPLAY_COMMAND_WRITE_LINE | prv_toggle_vcom());

    prv_set_chip_select(DIO_BSP_PIN_STATE_HIGH);
    prv_wait_chip_select_settle();

    spi_bsp_write(&header, sizeof(header));

    for (int16_t line = 0; line < DISPLAY_HEIGHT; ++line)
    {
        line_address = (uint8_t)(line + DISPLAY_FIRST_LINE_ADDRESS);

        spi_bsp_write(&line_address, sizeof(line_address));
        spi_bsp_write(g_frame_buffer[line], DISPLAY_BYTES_PER_LINE);
        spi_bsp_write(&padding, sizeof(padding));
    }

    /* Closing dummy byte, which together with the last line's padding forms the
     * 16-bit trailer the panel expects. */
    spi_bsp_write(&padding, sizeof(padding));

    prv_wait_chip_select_settle();
    prv_set_chip_select(DIO_BSP_PIN_STATE_LOW);
}

void display_clear_all(void)
{
    display_clear();

    prv_send_command((uint8_t)(DISPLAY_COMMAND_CLEAR_ALL | g_vcom_state));
}

void display_set_enabled(bool in_is_enabled)
{
    dio_bsp_set_pin(DIO_BSP_PIN_DISPLAY_DISP,
                    in_is_enabled ? DIO_BSP_PIN_STATE_HIGH : DIO_BSP_PIN_STATE_LOW);
}

void display_service_vcom(void)
{
    /* Software inversion: a no-update command that only carries the toggled bit. */
    prv_send_command(prv_toggle_vcom());

    /* External inversion: pulse the dedicated clock pin as well, so the driver
     * does not care which mode the board's jumper selects. */
    dio_bsp_toggle_pin(DIO_BSP_PIN_DISPLAY_EXTCOMIN);
}
